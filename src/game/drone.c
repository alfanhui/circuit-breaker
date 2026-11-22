#include "drone.h"
#include "walker.h" // walker_sin, walker_cos
#include "world.h"	// WORLD_HEADING_DX, WORLD_HEADING_DY
#include "../utils/fastmath.h" // ASR: shifts without libgcc's slow routines

#define ABS(v) ((v) < 0 ? -(v) : (v))

void drone_init(drone *d, const drone_stop *route, int stops, bool back_and_forth, unsigned int seed)
{
	d->route = route;
	d->stops = (unsigned char)stops;
	d->at = 0;
	d->step = 1;
	d->back_and_forth = back_and_forth;
	d->waiting = true;
	d->glance = 0;
	d->waited = 0;
	d->wait = route[0].wait;
	d->speed = DRONE_SPEED_MIN;
	d->pause = 0;
	d->sway = 0;
	d->sub_x = route[0].x << 8;
	d->sub_y = route[0].y << 8;
	d->x = route[0].x;
	d->y = route[0].y;
	d->z = route[0].z;
	d->yaw = (unsigned short)(route[0].heading << 14);
	d->beam_z = (short)(route[0].z - DRONE_BEAM_DROP);
	d->scan = 0;
	d->random = seed;
	d->settle_phase = (unsigned char)(seed & (DRONE_SETTLE_EVERY - 1));
	d->seen = false;
	d->mode = DRONE_PATROL;
	d->returning = false;
	d->unseen = 255;
	d->held = 0;
	d->lost_x = d->search_x = d->x;
	d->lost_y = d->search_y = d->y;
	d->lost_z = d->z;
	d->look = d->yaw;
	d->search_left = 0;
	d->cruise_z = -30000; // nothing to clear till it's looked round (and on its patrol it needn't)
}

int drone_random(drone *d)
{
	// A linear congruential generator (the C standard's example constants): a multiply and an add.
	// Its low bits repeat quickly, so the top 8 are the number
	d->random = d->random * 1103515245u + 12345u;
	return (int)(d->random >> 24);
}

void drone_forward(const drone *d, int *fx, int *fy)
{
	// The model faces north (-y) at yaw 0, and yaw turns it the way the headings go round (north,
	// east, south, west: drone_draw.c turns it by -yaw). So it faces (-sin, -cos): at yaw 0xC000,
	// west, that's (+1, 0), WORLD_HEADING_DX's west
	*fx = -walker_sin(d->yaw);
	*fy = -walker_cos(d->yaw);
}

void drone_sensor(const drone *d, int *x, int *y, int *z)
{
	int fx, fy;

	drone_forward(d, &fx, &fy);
	*x = d->x + ((fx * DRONE_NOSE) >> 10);
	*y = d->y + ((fy * DRONE_NOSE) >> 10);
	*z = d->z - DRONE_SENSOR_DOWN;
}

// One value a step nearer `to`: `step`, or what's left if that's less
static int step_to(int from, int to, int step)
{
	if (from < to)
		return to - from > step ? from + step : to;
	if (from > to)
		return from - to > step ? from - step : to;
	return from;
}

// Waiting: its turn eases, an eighth of what's left a frame, the short way round
static void ease_yaw(drone *d, int to)
{
	// What's left of the turn, the short way round: the 16-bit difference, read as signed
	const int left = (short)(unsigned short)(to - d->yaw);
	int turn = left >> DRONE_TURN_SHIFT;

	if (turn == 0 && left != 0)
		turn = left > 0 ? 1 : -1; // the last few steps of the ease: or it'd never quite get there
	d->yaw = (unsigned short)(d->yaw + turn);
}

// At its stop: there exactly, and how long it waits there, and whether it glances aside, picked
static void arrive(drone *d, const drone_stop *s)
{
	int r;

	d->sub_x = s->x << 8;
	d->sub_y = s->y << 8;
	d->x = s->x;
	d->y = s->y;
	d->waiting = true;
	d->waited = 0;
	// 0.5 to 1.5 times the stop's wait: (128 + 0 .. 255) / 256, a shift
	d->wait = (unsigned short)((s->wait * (128 + drone_random(d))) >> 8);
	if (d->wait == 0)
		d->wait = 1;
	// A glance aside in 3 waits out of 8, either way alike
	r = drone_random(d);
	d->glance = (signed char)(r < 48 ? -1 : r < 96 ? 1 : 0);
}

// Its wait's over: on to the next stop. A quarter of the time it turns back the way it came
static void leave(drone *d)
{
	int next;

	d->waiting = false;
	if (drone_random(d) < 64)
		d->step = (signed char)-d->step;
	next = d->at + d->step;
	if (next < 0 || next >= d->stops)
	{
		if (d->back_and_forth)
		{
			d->step = (signed char)-d->step; // at the end: back again
			next = d->at + d->step;
		}
		else
			next = next < 0 ? d->stops - 1 : 0; // a loop: round
	}
	if (next < 0 || next >= d->stops)
		next = 0; // a route of one stop
	d->at = (unsigned char)next;
	d->speed = (unsigned short)(DRONE_SPEED_MIN + ((drone_random(d) * DRONE_SPEED_SPREAD) >> 8));
	d->sway = 1 << (DRONE_SWAY_SHIFT - 1); // half a swing in: it weaves either side of its line alike
	d->pause = 0;
}

static void wait_at(drone *d, const drone_stop *s)
{
	int look = s->heading << 14;

	// Its glance: in the middle third of the wait (compared x3, not divided by 3)
	if (d->glance != 0 && d->waited * 3 >= d->wait && d->waited * 3 < 2 * d->wait)
		look += d->glance * 0x4000;
	ease_yaw(d, look);
	if (++d->waited >= d->wait)
		leave(d);
}

/*
 * Steers towards (tx, ty) and moves along where it faces, at up to `speed` (x256 a frame), turning
 * at most `turn_max` a frame. Within `standoff` of it, it holds there (still turning to face it):
 * chasing, that keeps you in the middle of its beam's square, rather than under its nose. Returns
 * how far it is from (tx, ty) (level_reach's measure: the longer way plus half the shorter).
 *
 * How far off its aim is, without an arctangent: where it faces is f (x1024), the point is t away.
 * `ahead` (f . t) is |t| cos(error), `aside` (t along the way a turn moves f: (fy, -fx)) is
 * |t| sin(error). So aside / ahead is tan(error), and aside's sign is which way to turn.
 *   - The point behind it (ahead <= 0): it turns at its fastest, and doesn't move on.
 *   - More than ~14 degrees off (|aside| > ahead / 4): it turns at its fastest, at half speed.
 *   - Less: it turns by tan(error) x 4 x turn_max, at full speed. That's the one division, once a
 *     drone a frame (CLAUDE.md: per object, not per element)
 */
static int steer(drone *d, int tx, int ty, int speed, int turn_max, int standoff, int *moved)
{
	const int dx = tx - d->x, dy = ty - d->y;
	const int long_way = ABS(dx) > ABS(dy) ? ABS(dx) : ABS(dy), short_way = ABS(dx) > ABS(dy) ? ABS(dy) : ABS(dx);
	const int distance = long_way + (short_way >> 1);
	int fx, fy, ahead, aside, turn;

	drone_forward(d, &fx, &fy);
	ahead = fx * dx + fy * dy;
	aside = fy * dx - fx * dy;
	if (ahead <= 0)
	{
		turn = aside >= 0 ? turn_max : -turn_max;
		speed = 0;
	}
	else if (ABS(aside) * 4 > ahead)
	{
		turn = aside >= 0 ? turn_max : -turn_max;
		speed >>= 1;
	}
	else
	{
		// Both taken down 6 bits first, so the product's well inside 32 bits (aside up to 10000
		// x 1024: down to 160000, x 0xC00 is 490 million)
		const int a = ahead >> 6;

		turn = a != 0 ? ((aside >> 6) * turn_max * 4) / a : 0;
	}
	if (distance <= standoff)
		speed = 0;
	d->yaw = (unsigned short)(d->yaw + turn);
	d->sub_x += ASR(fx * speed, 10);
	d->sub_y += ASR(fy * speed, 10);
	d->x = (short)(d->sub_x >> 8);
	d->y = (short)(d->sub_y >> 8);
	if (moved)
		*moved = speed;
	return distance;
}

/*
 * Flying its patrol to its stop: steered there at its leg's speed. Now and then it hesitates, and
 * away from its stops it weaves
 */
static void fly(drone *d, const drone_stop *s)
{
	const int tx = s->x - d->x, ty = s->y - d->y;
	const int distance = ABS(tx) + ABS(ty);
	int moved;

	if (ABS(tx) <= DRONE_ARRIVE && ABS(ty) <= DRONE_ARRIVE)
	{
		arrive(d, s);
		d->returning = false; // back on its patrol
		return;
	}
	if (d->pause > 0)
	{
		--d->pause; // hesitating: it hangs where it is
		return;
	}
	// About once in 4 s (1 in 256 frames), away from its stops: it hesitates, 1/3 s to 1.4 s
	if (distance > DRONE_SWAY_NEAR && drone_random(d) == 0)
		d->pause = (unsigned char)(20 + (drone_random(d) >> 2));
	steer(d, s->x, s->y, d->speed, DRONE_TURN_MAX, 0, &moved);
	// The weave: aside, one way then the other (a square wave of speed: a zigzag of place). Not
	// while it turns on the spot
	if (distance > DRONE_SWAY_NEAR && moved > 0)
	{
		const int sway = (d->sway >> DRONE_SWAY_SHIFT) & 1 ? DRONE_SWAY : -DRONE_SWAY;
		int fx, fy;

		drone_forward(d, &fx, &fy);
		d->sub_x += ASR(fy * sway, 10);
		d->sub_y -= ASR(fx * sway, 10);
		d->sway++;
		d->x = (short)(d->sub_x >> 8);
		d->y = (short)(d->sub_y >> 8);
	}
}

/*
 * Chasing: straight at where it last saw you, over the roofs (the user, 2026-11-17: walls hide you
 * from it, they don't stop it). While it sees you it holds back DRONE_STANDOFF, you in its beam's
 * middle. Once it's lost you for DRONE_LOSE_FRAMES it goes to where it saw you last, right over
 * it, and searches from there
 */
static void chase(drone *d)
{
	const bool lost = d->unseen >= DRONE_LOSE_FRAMES;
	const int distance =
		steer(d, d->lost_x, d->lost_y, DRONE_CHASE_SPEED, DRONE_CHASE_TURN, lost ? 0 : DRONE_STANDOFF, 0);

	if (lost && distance <= DRONE_SEARCH_THERE)
	{
		d->mode = DRONE_SEARCH;
		d->search_left = DRONE_SEARCH_FRAMES;
		d->waiting = true; // a first look round, from right where it lost you
		d->waited = 0;
		d->wait = (unsigned short)(30 + (drone_random(d) >> 3));
		d->look = (unsigned short)(d->yaw + ((drone_random(d) - 128) << 8));
	}
}

/*
 * Searching round where it lost you, for DRONE_SEARCH_FRAMES: it hovers and turns to look a random
 * way (up to half a turn either way), then hops to a random point within DRONE_SEARCH_REACH of where
 * it lost you, peering round the walls, and looks again. Then back to its patrol, to the stop it was
 * going to, high enough over the towers on the way (returning)
 */
static void search(drone *d)
{
	if (--d->search_left == 0)
	{
		d->mode = DRONE_PATROL;
		d->returning = true;
		d->waiting = false;
		d->pause = 0;
		d->speed = DRONE_SPEED_MIN + DRONE_SPEED_SPREAD;
		return;
	}
	if (d->waiting)
	{
		ease_yaw(d, d->look);
		if (++d->waited >= d->wait)
		{
			// Up to DRONE_SEARCH_REACH either way: (0 .. 255 - 128) x 2 x reach / 256, a shift
			d->waiting = false;
			d->search_x = (short)(d->lost_x + (((drone_random(d) - 128) * DRONE_SEARCH_REACH) >> 7));
			d->search_y = (short)(d->lost_y + (((drone_random(d) - 128) * DRONE_SEARCH_REACH) >> 7));
		}
		return;
	}
	if (steer(d, d->search_x, d->search_y, DRONE_SEARCH_SPEED, DRONE_CHASE_TURN, 0, 0) <= DRONE_SEARCH_THERE)
	{
		d->waiting = true;
		d->waited = 0;
		d->wait = (unsigned short)(40 + (drone_random(d) >> 2)); // 0.7 to 1.7 s
		d->look = (unsigned short)(d->yaw + ((drone_random(d) - 128) << 8));
	}
}

/*
 * What it learns from the boxes, once in DRONE_SETTLE_EVERY frames:
 *   - the roof under its beam's middle: its square eases to that height, a quarter of the way each
 *     time it looks. Over a street there's no roof: it keeps the height it had. Not a roof as high
 *     as the sensor or nearly (a wall beside it: its square would stand up into the drone);
 *   - the tallest tower round it (DRONE_CLEAR_AROUND each way): off its patrol it keeps
 *     DRONE_CLEAR over that (cruise_z), so a chase over the roofs never flies into a wall
 */
static void look_around(drone *d, const drone_box *boxes, int box_count)
{
	int fx, fy, sx, sy, sz, mx, my, tallest = -30000;
	bool roof = false;

	drone_forward(d, &fx, &fy);
	drone_sensor(d, &sx, &sy, &sz);
	mx = sx + (ASR(fx * DRONE_BEAM_AHEAD, 10));
	my = sy + (ASR(fy * DRONE_BEAM_AHEAD, 10));
	for (int b = 0; b < box_count; ++b)
	{
		const drone_box *box = &boxes[b];

		if (box->x_max >= d->x - DRONE_CLEAR_AROUND && box->x_min <= d->x + DRONE_CLEAR_AROUND &&
			box->y_max >= d->y - DRONE_CLEAR_AROUND && box->y_min <= d->y + DRONE_CLEAR_AROUND && box->top > tallest)
			tallest = box->top;
		if (!roof && mx >= box->x_min && mx <= box->x_max && my >= box->y_min && my <= box->y_max)
		{
			roof = true;
			if (box->top < sz - 100)
			{
				const int left = box->top - d->beam_z;
				int move = left >> 2;

				if (move == 0 && left != 0)
					move = left > 0 ? 1 : -1;
				d->beam_z = (short)(d->beam_z + move);
			}
		}
	}
	d->cruise_z = (short)(tallest + DRONE_CLEAR);
}

void drone_update(drone *d, const drone_box *boxes, int box_count)
{
	const drone_stop *s = &d->route[d->at];
	int height;

	// The scan line sweeps across, then flies back (a power of two: a mask, not a remainder)
	d->scan = (unsigned char)((d->scan + 1) & (DRONE_SCAN_FRAMES - 1));
	/*
	 * Its height: its stop's on patrol; chasing or searching, DRONE_CHASE_ABOVE over where it last
	 * saw your eye, climbing faster. Off its patrol's own stops (chasing, searching, or on its way
	 * back) never lower than cruise_z, clear of the towers round it
	 */
	if (d->mode == DRONE_PATROL)
		height = s->z;
	else
		height = d->lost_z + DRONE_CHASE_ABOVE;
	if ((d->mode != DRONE_PATROL || d->returning) && height < d->cruise_z)
		height = d->cruise_z;
	d->z = (short)step_to(d->z, height, d->mode == DRONE_PATROL ? DRONE_CLIMB : DRONE_CHASE_CLIMB);
	if (d->mode == DRONE_CHASE)
		chase(d);
	else if (d->mode == DRONE_SEARCH)
		search(d);
	else if (d->waiting)
		wait_at(d, s);
	else
		fly(d, s);
	/*
	 * The boxes: a walk over every one (256 in the city), the most of its frame's cost. Once in
	 * DRONE_SETTLE_EVERY frames is plenty (the heights ease anyway), each drone on its own frame of
	 * those (its seed's), so three don't all walk on the same one
	 */
	if (((d->scan + d->settle_phase) & (DRONE_SETTLE_EVERY - 1)) == 0)
		look_around(d, boxes, box_count);
}

void drone_notice(drone *d, bool seen, int x, int y, int z)
{
	d->seen = seen;
	if (seen)
	{
		// It has you: chasing, from wherever it was, and you're where it saw you
		d->mode = DRONE_CHASE;
		d->returning = false;
		d->waiting = false;
		d->pause = 0;
		d->unseen = 0;
		d->lost_x = (short)x;
		d->lost_y = (short)y;
		d->lost_z = (short)z;
		if (d->held < 255)
			d->held++;
		return;
	}
	if (d->unseen < 255)
		d->unseen++;
	if (d->held > 0)
		d->held--; // out of its beam a moment: the catch comes back down as slowly as it went up
}

bool drone_caught(const drone *d)
{
	return d->held >= DRONE_CATCH_FRAMES;
}

void drone_beam_get(const drone *d, drone_beam *b)
{
	int fx, fy, sx, sy, sz;

	drone_forward(d, &fx, &fy);
	drone_sensor(d, &sx, &sy, &sz);
	{
		// The square's middle, and its half side along where it faces (f) and across (side: f
		// turned a quarter, (-fy, fx))
		const int mx = sx + (ASR(fx * DRONE_BEAM_AHEAD, 10)), my = sy + (ASR(fy * DRONE_BEAM_AHEAD, 10));
		const int ax = ASR(fx * DRONE_BEAM_HALF, 10), ay = ASR(fy * DRONE_BEAM_HALF, 10);
		const int cx = ASR(-fy * DRONE_BEAM_HALF, 10), cy = ASR(fx * DRONE_BEAM_HALF, 10);
		// Near left, near right, far right, far left: (along, across) as -1 or +1 halves
		static const signed char corner[4][2] = {{-1, -1}, {-1, 1}, {1, 1}, {1, -1}};

		b->apex.x = sx;
		b->apex.y = sy;
		b->apex.z = sz;
		for (int k = 0; k < 4; ++k)
		{
			b->base[k].x = mx + corner[k][0] * ax + corner[k][1] * cx;
			b->base[k].y = my + corner[k][0] * ay + corner[k][1] * cy;
			b->base[k].z = d->beam_z;
		}
	}
}

/*
 * Each side's plane goes through the apex and two corners of the base. Its normal is the cross
 * product of the two edges from the apex (u x v); a point is on the same side of it as the base's
 * middle if (normal . point) and (normal . middle) have the same sign, everything from the apex.
 * Comparing signs (not multiplying them) needs no care about which way round the corners go.
 * Sizes: the edges are up to ~1500 each way, so the normal's up to 4.5 million: taken down 6
 * bits (70000), times the point (up to DRONE_BEAM_REACH, 2048) three times is 430 million, inside
 * 32 bits. 9 multiplies and 6 more a side: no division
 */
unsigned drone_beam_outside(const drone_beam *b, int x, int y, int z)
{
	const int ex = x - b->apex.x, ey = y - b->apex.y, ez = z - b->apex.z;
	// The base's middle: halfway along a diagonal
	const int mx = ((b->base[0].x + b->base[2].x) >> 1) - b->apex.x;
	const int my = ((b->base[0].y + b->base[2].y) >> 1) - b->apex.y;
	const int mz = ((b->base[0].z + b->base[2].z) >> 1) - b->apex.z;
	unsigned outside = 0;

	if (ABS(ex) > DRONE_BEAM_REACH || ABS(ey) > DRONE_BEAM_REACH || ABS(ez) > DRONE_BEAM_REACH)
		return 0xF;
	for (unsigned k = 0, bit = 1; k < 4; ++k, bit <<= 1) // (bit: 1u << k, without a shift by a variable count)
	{
		const drone_point *p = &b->base[k], *q = &b->base[(k + 1) & 3];
		const int ux = p->x - b->apex.x, uy = p->y - b->apex.y, uz = p->z - b->apex.z;
		const int vx = q->x - b->apex.x, vy = q->y - b->apex.y, vz = q->z - b->apex.z;
		const int nx = ASR(uy * vz - uz * vy, 6), ny = ASR(uz * vx - ux * vz, 6), nz = ASR(ux * vy - uy * vx, 6);
		const int to_eye = nx * ex + ny * ey + nz * ez, to_middle = nx * mx + ny * my + nz * mz;

		if (to_eye != 0 && (to_eye < 0) != (to_middle < 0))
			outside |= bit;
	}
	return outside;
}

/*
 * Nothing in the way from (sx, sy, sz) to (ex, ey, ez): tested at 16 points between (the ends left
 * out). Each point's (start + the whole line x k / 16): a shift, no division. A box is in the way
 * if a point's inside it, under its roof. Only boxes the line's bounds touch, and whose roof's over
 * the line's lower end, are tested at all
 */
static bool line_clear(int sx, int sy, int sz, int ex, int ey, int ez, const drone_box *boxes, int box_count)
{
	const int dx = ex - sx, dy = ey - sy, dz = ez - sz;
	const int x_min = dx < 0 ? ex : sx, x_max = dx < 0 ? sx : ex;
	const int y_min = dy < 0 ? ey : sy, y_max = dy < 0 ? sy : ey;
	const int z_low = dz < 0 ? ez : sz;

	for (int b = 0; b < box_count; ++b)
	{
		const drone_box *box = &boxes[b];

		if (box->x_max < x_min || box->x_min > x_max || box->y_max < y_min || box->y_min > y_max || box->top <= z_low)
			continue;
		for (int k = 1; k < (1 << DRONE_SIGHT_STEPS_SHIFT); ++k)
		{
			const int px = sx + ((dx * k) >> DRONE_SIGHT_STEPS_SHIFT);
			const int py = sy + ((dy * k) >> DRONE_SIGHT_STEPS_SHIFT);
			const int pz = sz + ((dz * k) >> DRONE_SIGHT_STEPS_SHIFT);

			if (pz < box->top && px >= box->x_min && px <= box->x_max && py >= box->y_min && py <= box->y_max)
				return false;
		}
	}
	return true;
}

bool drone_sees(const drone *d, int x, int y, int z, const drone_box *boxes, int box_count)
{
	drone_beam beam;

	drone_beam_get(d, &beam);
	// The cheap test first: most frames you're nowhere near its beam, and the boxes aren't looked at
	if (drone_beam_outside(&beam, x, y, z) != 0)
		return false;
	return line_clear(beam.apex.x, beam.apex.y, beam.apex.z, x, y, z, boxes, box_count);
}

int drone_settled_heading(const drone *d)
{
	// The nearest axis: an eighth of a turn on, then the top two bits
	const int heading = ((d->yaw + 0x2000) >> 14) & 3;
	const int off = (short)(unsigned short)(d->yaw - (heading << 14));

	return ABS(off) < DRONE_SCAN_SETTLED ? heading : -1;
}

int drone_scan_ahead(const drone *d)
{
	// Across the square, its near edge to its far (less the line's own depth), over the sweep: one
	// multiply and a shift (a power of two of frames)
	return DRONE_BEAM_AHEAD - DRONE_BEAM_HALF + (((2 * DRONE_BEAM_HALF - DRONE_SCAN_DEEP) * d->scan) >> DRONE_SCAN_SHIFT);
}

void drone_scan_rect(const drone *d, int heading, drone_box *rect)
{
	const int dx = WORLD_HEADING_DX(heading), dy = WORLD_HEADING_DY(heading);
	const int near = drone_scan_ahead(d), far = near + DRONE_SCAN_DEEP;
	int sx, sy, sz;

	drone_sensor(d, &sx, &sy, &sz);
	// Along where it faces, from near to far; across, the square's width
	if (dx != 0)
	{
		const int a = sx + dx * near, b = sx + dx * far;

		rect->x_min = (short)(a < b ? a : b);
		rect->x_max = (short)(a < b ? b : a);
		rect->y_min = (short)(sy - DRONE_BEAM_HALF);
		rect->y_max = (short)(sy + DRONE_BEAM_HALF);
	}
	else
	{
		const int a = sy + dy * near, b = sy + dy * far;

		rect->y_min = (short)(a < b ? a : b);
		rect->y_max = (short)(a < b ? b : a);
		rect->x_min = (short)(sx - DRONE_BEAM_HALF);
		rect->x_max = (short)(sx + DRONE_BEAM_HALF);
	}
	rect->top = 0;
}

int drone_scan_lit(const drone *d, const drone_box *boxes, int box_count, drone_box *lit)
{
	const int heading = drone_settled_heading(d);
	drone_box line;
	int sx, sy, sz, count = 0;

	if (heading < 0)
		return 0; // turning: the line's along an axis, and it isn't facing one
	drone_scan_rect(d, heading, &line);
	drone_sensor(d, &sx, &sy, &sz);
	for (int b = 0; b < box_count && count < DRONE_LIT_MAX; ++b)
	{
		const drone_box *box = &boxes[b];
		drone_box part;

		if (box->x_max < line.x_min || box->x_min > line.x_max || box->y_max < line.y_min || box->y_min > line.y_max ||
			box->top >= sz)
			continue; // not under the line, or as high as the sensor (it can't light its own floor)
		part.x_min = box->x_min > line.x_min ? box->x_min : line.x_min;
		part.x_max = box->x_max < line.x_max ? box->x_max : line.x_max;
		part.y_min = box->y_min > line.y_min ? box->y_min : line.y_min;
		part.y_max = box->y_max < line.y_max ? box->y_max : line.y_max;
		part.top = box->top;
		// In a nearer tower's shadow: the sensor can't see the middle of the part, just over it
		if (!line_clear(sx, sy, sz, (part.x_min + part.x_max) >> 1, (part.y_min + part.y_max) >> 1, part.top + 1, boxes,
						box_count))
			continue;
		lit[count++] = part;
	}
	return count;
}
