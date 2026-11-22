#include "walker.h"
#include "../utils/fastmath.h" // ASR: shifts without libgcc's slow routines

// sin x1024 for a quarter turn in 64 steps (0x4000 / 64 = 256 ANGLE each), and its end: worked
// out once on the PC (round(1024 x sin(i x 90 / 64 degrees)))
static const short quarter_sine[65] = {
	0,	 25,  50,  75,	100, 125, 150, 175, 200, 224, 249, 273, 297, 321, 345, 369, 392,
	415, 438, 460, 483, 505, 526, 548, 569, 590, 610, 630, 650, 669, 688, 706, 724, 742,
	759, 775, 792, 807, 822, 837, 851, 865, 878, 891, 903, 915, 926, 936, 946, 955, 964,
	972, 980, 987, 993, 999, 1004, 1009, 1013, 1016, 1019, 1021, 1023, 1024, 1024,
};

int walker_sin(int angle)
{
	const int a = angle & 0xFFFF;
	const int quadrant = a >> 14;	// which quarter turn: 0..3
	int within = a & 0x3FFF;		// how far into it
	int i, value;

	// The second and fourth quarters run back down the table: sin(180 - a) = sin(a)
	if (quadrant & 1)
		within = 0x4000 - within;
	i = within >> 8;
	// In between two of the table's steps, a straight line: the share of the way (0..255) x the
	// step, >> 8. The last entry (90 degrees exactly) has no step after it
	value = i >= 64 ? quarter_sine[64] : quarter_sine[i] + (ASR((quarter_sine[i + 1] - quarter_sine[i]) * (within & 0xFF), 8));
	// And the second half turn is the first's, upside down: sin(180 + a) = -sin(a)
	return quadrant & 2 ? -value : value;
}

int walker_cos(int angle)
{
	return walker_sin(angle + 0x4000);
}

void walker_area_clear(walker_area *a)
{
	a->count = 0;
}

int walker_area_add(walker_area *a, int x0, int y0, int x1, int y1)
{
	return walker_area_add_roof(a, x0, y0, x1, y1, 0);
}

int walker_area_add_roof(walker_area *a, int x0, int y0, int x1, int y1, int height)
{
	return walker_area_add_bar(a, x0, y0, x1, y1, WALKER_NO_FLOOR, height); // all the way down
}

int walker_area_add_bar(walker_area *a, int x0, int y0, int x1, int y1, int bottom, int height)
{
	world_rect *r;

	if (a->count >= WALKER_AREA_RECTS_MAX)
		return 0;
	a->heights[a->count] = (short)height;
	a->bottoms[a->count] = (short)bottom;
	r = &a->rects[a->count++];
	// Smallest corner first: the roof as it is, out to its edges
	r->x_min = (short)(x0 < x1 ? x0 : x1);
	r->x_max = (short)(x0 < x1 ? x1 : x0);
	r->y_min = (short)(y0 < y1 ? y0 : y1);
	r->y_max = (short)(y0 < y1 ? y1 : y0);
	return 1;
}

int walker_area_holds(const walker_area *a, int x, int y)
{
	return walker_area_height(a, x, y) != WALKER_NO_FLOOR;
}

// Whether a point's on a roof (out to `margin` past its edges)
static int on_roof(const world_rect *r, int x, int y, int margin)
{
	return x >= r->x_min - margin && x <= r->x_max + margin && y >= r->y_min - margin && y <= r->y_max + margin;
}

// The highest roof under a point no higher than `top`: its index, or -1 for none. The top keeps
// out roofs that aren't under your feet but beside them: a taller one you're stood against (a
// wall), or the one you've just stepped off (falling)
static int roof_at(const walker_area *a, int x, int y, int top)
{
	int best = -1;

	for (int i = 0; i < a->count; ++i)
		if (a->heights[i] <= top && on_roof(&a->rects[i], x, y, 0) && (best < 0 || a->heights[i] > a->heights[best]))
			best = i;
	return best;
}

int walker_area_height(const walker_area *a, int x, int y)
{
	const int i = roof_at(a, x, y, 0x7FFF);

	return i < 0 ? WALKER_NO_FLOOR : a->heights[i];
}

void walker_init(walker *w, world_point at, int yaw)
{
	walker_init_at(w, at, 0, yaw);
}

void walker_init_at(walker *w, world_point at, int z, int yaw)
{
	w->x = at.x * WALKER_SUB;
	w->y = at.y * WALKER_SUB;
	w->z = z * WALKER_SUB;
	w->fall_speed = 0;
	w->fall_from = z;
	w->take_off = z;
	w->air_dx = 0;
	w->air_dy = 0;
	w->falling = 0;
	w->lag = 0;
	w->slide = 0;
	w->move = WALKER_MOVE_NONE;
	w->push = 0;
	w->stagger = 0;
	w->dip = 0;
	w->eye = WALKER_EYE_RIDING;
	w->yaw = yaw & 0xFFFF;
	w->turn_speed = 0;
	w->forward = 0;
	w->side = 0;
	w->stride = 0;
}

// One frame of a speed towards the speed wanted: slowing down (towards 0) by WALKER_SLOW_DOWN,
// speeding up (away from 0) by WALKER_SPEED_UP, landing exactly. Turning about (forwards to
// backwards) is a slow down to 0, then a speed up: you stop before you go back
static int approach(int now, int target)
{
	int next;

	if (now == target)
		return now;
	if (now < target)
		return -approach(-now, -target); // the same thing, mirrored: only going down is written out
	if (now > 0)
	{
		const int lowest = target > 0 ? target : 0; // slowing never takes you past standing still

		next = now - WALKER_SLOW_DOWN;
		return next < lowest ? lowest : next;
	}
	next = now - WALKER_SPEED_UP; // at or below 0 already: speeding up the other way
	return next < target ? target : next;
}

// A speed held to at most `limit` either way
static int clamp_speed(int speed, int limit)
{
	return speed > limit ? limit : speed < -limit ? -limit : speed;
}

// How high your head is above your feet: lower sliding
static int head_room(const walker *w)
{
	return w->slide > 0 ? WALKER_SLIDE_HEAD : WALKER_HEAD;
}

/*
 * What stands in your way at a point, your feet at `feet` and your head at `head`: every roof
 * higher than `top` (a kerb above your feet, walking) whose bottom is below your head (a bar you
 * pass under isn't in the way). You keep WALKER_RADIUS off a tall one or a bar (your body hits
 * it), but only WALKER_FOOT off one lower than your waist (a step you can't step: your foot hits
 * it). How many there are, the highest top among them, and the lowest bottom
 */
typedef struct
{
	int count, top, bottom;
} walker_block;

static walker_block blocked_by(const walker_area *a, int x, int y, int feet, int top, int head)
{
	const int waist = feet + WALKER_WAIST;
	walker_block b = {0, -32768, 32767};

	for (int i = 0; i < a->count; ++i)
	{
		const int body = a->heights[i] > waist || a->bottoms[i] > feet;

		if (a->heights[i] > top && a->bottoms[i] < head && on_roof(&a->rects[i], x, y, body ? WALKER_RADIUS : WALKER_FOOT))
		{
			++b.count;
			b.top = a->heights[i] > b.top ? a->heights[i] : b.top;
			b.bottom = a->bottoms[i] < b.bottom ? a->bottoms[i] : b.bottom;
		}
	}
	return b;
}

static int wall_at(const walker_area *a, int x, int y, int feet, int top, int head)
{
	return blocked_by(a, x, y, feet, top, head).count > 0;
}

// How far a point is inside a roof's edges grown by `margin`: its distance from the nearest of
// them, more than 0 inside, 0 or less outside
static int depth_in(const world_rect *r, int x, int y, int margin)
{
	int d = x - (r->x_min - margin);

	d = (r->x_max + margin) - x < d ? (r->x_max + margin) - x : d;
	d = y - (r->y_min - margin) < d ? y - (r->y_min - margin) : d;
	return (r->y_max + margin) - y < d ? (r->y_max + margin) - y : d;
}

/*
 * A step from (from_x, from_y) to (x, y): whether a wall's in the way, as wall_at, but a wall you're
 * already nearer than its margin only stops a step that takes you further in. A landing can leave
 * you there (off a vault, 29 from the corridor's side; off the tall box onto the lower roof, 34 from
 * its side: WALKER_RADIUS is 40). Testing only where you'd be, every step was then into the wall,
 * stepping away too: stuck in the corner for good (play-test 2026-11-08). Now you walk out of it
 */
static int wall_in_way(const walker_area *a, int from_x, int from_y, int x, int y, int feet, int top, int head)
{
	const int waist = feet + WALKER_WAIST;

	for (int i = 0; i < a->count; ++i)
	{
		const int margin = a->heights[i] > waist || a->bottoms[i] > feet ? WALKER_RADIUS : WALKER_FOOT;
		const int deeper = depth_in(&a->rects[i], x, y, margin);

		if (a->heights[i] > top && a->bottoms[i] < head && deeper >= 0 &&
			deeper > depth_in(&a->rects[i], from_x, from_y, margin))
			return 1;
	}
	return 0;
}

// Whether anything's solid at a point between `feet` and `head` (no margin): in the air, what
// you'd go into the side of
static int solid_at(const walker_area *a, int x, int y, int feet, int head)
{
	for (int i = 0; i < a->count; ++i)
		if (a->heights[i] > feet && a->bottoms[i] < head && on_roof(&a->rects[i], x, y, 0))
			return 1;
	return 0;
}

// Pushing into a lip: pushed long enough, you step off, if there's a roof below to land on, a
// little past the lip the way you're pushing (sign: -1 or 1, along x or y). Only while the pad's
// held (pushing): the last of your speed as you stop isn't a push. Returns whether you stepped off
static int step_off(walker *w, const walker_area *area, int along_x, int sign, int pushing)
{
	const int feet = w->z >> WALKER_SUB_SHIFT;
	const int x = (w->x >> WALKER_SUB_SHIFT) + (along_x ? sign * WALKER_RADIUS : 0);
	const int y = (w->y >> WALKER_SUB_SHIFT) + (along_x ? 0 : sign * WALKER_RADIUS);
	const int below = roof_at(area, x, y, feet - WALKER_STEP_UP - 1); // more than a kerb down

	if (!pushing)
	{
		w->push = 0; // only sliding into it, slowing down: not meant
		return 0;
	}
	if (++w->push < WALKER_DROP_PUSH_FRAMES)
		return 0;
	w->push = WALKER_DROP_PUSH_FRAMES; // held there: over nothing, it never lets go
	if (below < 0)
		return 0;
	w->falling = 1;
	w->fall_speed = 0;
	w->fall_from = feet;
	w->take_off = feet;
	w->push = 0;
	return 1;
}

// The highest roof no higher than `top` under any of your footprint (WALKER_FOOT round your
// middle), or `lowest` if none's higher
static int footprint_height(const walker_area *a, int x, int y, int top, int lowest)
{
	for (int i = 0; i < a->count; ++i)
		if (a->heights[i] <= top && a->heights[i] > lowest && on_roof(&a->rects[i], x, y, WALKER_FOOT))
			lowest = a->heights[i];
	return lowest;
}

// The eye's lag (walker_step_lag): your feet went up `rise` (down, negative), the eye's left
// behind by that much more, up to WALKER_LAG_MAX either way
static void lag_by(walker *w, int rise)
{
	const int lag = w->lag + rise;

	w->lag = (signed char)(lag > WALKER_LAG_MAX ? WALKER_LAG_MAX : lag < -WALKER_LAG_MAX ? -WALKER_LAG_MAX : lag);
}

/*
 * One axis of a step on the roofs (x or y: the other's left as it is): 0 blocked, 1 moved, 2 at a
 * lip, 3 vaulted up. Where you'd be:
 *   - within WALKER_RADIUS of a roof more than `rise` above your feet: a wall, blocked. The rise
 *     is a kerb (WALKER_STEP_UP), or running (WALKER_MOVE_SPEED) your waist: a vault up onto
 *     something that low is a step that big, and you're carried on, WALKER_FOOT, onto its top (walk). Left with
 *     your middle still over the ground, the next step was a lip: you jumped straight off
 *   - your middle on a roof no more than a kerb up or down: moved
 *   - otherwise (lower, or nothing): a lip. You can stand right at a roof's edge, to look over
 * Moved, your feet go to the highest roof under your footprint (a kerb up at most), not just
 * under your middle: you step up a stair as your foot reaches it, as you would. Only stepping up
 * once your middle got there, the eye (35 up) was below the next step's top 3 units from its
 * front, too near to draw it: you saw in under the stairs. The eye follows the feet (lag_by)
 */
static int step_axis(walker *w, const walker_area *area, int x, int y, int rise)
{
	const int feet = w->z >> WALKER_SUB_SHIFT;
	const int top = feet + rise; // the highest you can step up to
	int under, to;

	if (wall_in_way(area, w->x >> WALKER_SUB_SHIFT, w->y >> WALKER_SUB_SHIFT, x, y, feet, top, feet + head_room(w)))
		return 0;
	under = roof_at(area, x, y, top);
	if (under < 0 || area->heights[under] < feet - WALKER_STEP_UP)
		return 2;
	to = footprint_height(area, x, y, top, area->heights[under]);
	lag_by(w, to - feet);
	w->z = to * WALKER_SUB;
	return to > feet + WALKER_STEP_UP ? 3 : 1;
}

static int onto_top(const walker_area *area, int x, int y, int along_x, int sign, int height, int *px, int *py); // below

/*
 * Catching a ledge in the air (the user, 2026-11-14: you couldn't get back from the CD block to
 * Backup RAM, as every street on the way down is a jump down, and a jump never landed higher than it
 * left). Going into the side of a roof whose top's no more than WALKER_CLIMB above your feet, your
 * hands catch its edge and you pull yourself up onto it: a climb, as from the ground (Mirror's Edge's
 * catch). (x, y): where you'd have gone, into its side, the way along_x and sign say; feet: your
 * feet's height before this frame's drop. Not a bar (it floats: there's no edge to pull up onto
 * from below it). Returns whether you caught it: stood on its top, at a walk, the eye following
 */
static int catch_ledge(walker *w, const walker_area *area, int x, int y, int along_x, int sign, int feet)
{
	int top = -32768, px, py;

	for (int i = 0; i < area->count; ++i)
		if (area->heights[i] > feet && area->bottoms[i] < feet + WALKER_HEAD && on_roof(&area->rects[i], x, y, 0))
		{
			if (area->bottoms[i] > feet)
				return 0; // a bar
			top = area->heights[i] > top ? area->heights[i] : top;
		}
	if (top == -32768 || top > feet + WALKER_CLIMB)
		return 0; // nothing there, or out of reach: you hit the side
	// Only a ledge higher than you left (more than a kerb): a jump up. One no higher, a jump that
	// falls short of it, isn't caught: a gap too far still holds you at its lip (vault_off refuses
	// it), so how far you can jump is the same as it was
	if (top <= w->take_off + WALKER_STEP_UP)
		return 0;
	if (!onto_top(area, x, y, along_x, sign, top, &px, &py))
		return 0; // no room to stand on it
	lag_by(w, top - feet);
	w->x = px * WALKER_SUB;
	w->y = py * WALKER_SUB;
	w->z = top * WALKER_SUB;
	w->falling = 0;
	w->fall_speed = 0;
	w->dip = 0;
	if (w->forward > WALKER_FORWARD_SPEED)
		w->forward = WALKER_FORWARD_SPEED;
	w->move = WALKER_MOVE_CLIMB;
	return 1;
}

/*
 * In the air: gravity, and the way you left the roof (air_dx, air_dy). Going up (a jump: the fall
 * speed's negative) the top's where the drop's measured from. Going into the side of a roof higher
 * than your feet, you catch its edge if it's in reach (catch_ledge) and climb up onto it; otherwise
 * you hit it: you stop against it and fall on down its side. You land on a roof once your feet
 * reach it. How far you fell decides the landing. Returns false if you hit a side this frame
 */
static int fall(walker *w, const walker_area *area)
{
	// Across at the height you were at, before this frame's drop: tested at the new one, the
	// frame you came down past a roof's top, its side would stop you (1 under it), not land you
	const int across = w->z >> WALKER_SUB_SHIFT;
	int feet, under, clear = 1;

	w->fall_speed += WALKER_GRAVITY;
	if (w->fall_speed > WALKER_FALL_MAX)
		w->fall_speed = WALKER_FALL_MAX;
	w->z -= w->fall_speed;
	if (w->z < (WALKER_NO_FLOOR / 2) * WALKER_SUB)
		w->z = (WALKER_NO_FLOOR / 2) * WALKER_SUB; // fell off everything (it shouldn't happen): it stops somewhere
	feet = w->z >> WALKER_SUB_SHIFT;
	if (feet > w->fall_from)
		w->fall_from = feet; // still going up
	if (!solid_at(area, (w->x + w->air_dx) >> WALKER_SUB_SHIFT, w->y >> WALKER_SUB_SHIFT, across, across + WALKER_HEAD))
		w->x += w->air_dx;
	else if (w->air_dx != 0)
	{
		if (catch_ledge(w, area, (w->x + w->air_dx) >> WALKER_SUB_SHIFT, w->y >> WALKER_SUB_SHIFT, 1, w->air_dx > 0 ? 1 : -1, across))
			return 1; // caught: up on its top
		clear = 0;
	}
	if (!solid_at(area, w->x >> WALKER_SUB_SHIFT, (w->y + w->air_dy) >> WALKER_SUB_SHIFT, across, across + WALKER_HEAD))
		w->y += w->air_dy;
	else if (w->air_dy != 0)
	{
		if (catch_ledge(w, area, w->x >> WALKER_SUB_SHIFT, (w->y + w->air_dy) >> WALKER_SUB_SHIFT, 0, w->air_dy > 0 ? 1 : -1, across))
			return 1;
		clear = 0;
	}

	// Landing: on the highest roof under you lower than where you fell from (not the one you
	// stepped off, still under your middle for the first frame or two), and no higher than your
	// feet were before this frame's drop (not a bar you're passing under: you've not come down
	// onto it)
	under = roof_at(area, w->x >> WALKER_SUB_SHIFT, w->y >> WALKER_SUB_SHIFT,
					w->fall_from - 1 < across ? w->fall_from - 1 : across);
	if (under >= 0 && feet <= area->heights[under])
	{
		// The eye dips with the landing: an eighth of the fall (from the jump's top), up to 8 (12
		// for a long drop). But whether it's a long drop goes by where you left from: a jump's
		// hop doesn't turn a clean drop into a long one
		const int fall = w->fall_from - area->heights[under];
		const int drop = w->take_off - area->heights[under];

		w->falling = 0;
		w->fall_speed = 0;
		w->z = area->heights[under] * WALKER_SUB;
		w->dip = (unsigned char)((fall >> 3) > 8 ? 8 : fall >> 3);
		if (drop > WALKER_CLEAN_DROP)
		{
			w->stagger = WALKER_STAGGER_FRAMES;
			w->dip = 12;
		}
	}
	return clear;
}

/*
 * The roofs near a point: those within `reach` of it, as an area of their own. A move's worked out
 * whole before it starts (a jump's flight, a slide), up to 64 frames of it, each testing every
 * roof several times: against the whole area (56 roofs in the corridor and the city) that was
 * half a frame's work in one frame, and the frame was late (46 fps at the yard's duct). Nothing
 * further than the move can carry you (and WALKER_RADIUS round that) can touch it, so the same
 * frames against just these give the same answer, from a handful of roofs. One area, kept here:
 * a walker_area's ~780 bytes are too many for the stack, and it's only ever used one move at a time
 */
#define WALKER_FLIGHT_REACH (WALKER_FLIGHT_FRAMES_MAX * 6 + 2 * WALKER_RADIUS) // a run's 6 a frame
#define WALKER_SLIDE_REACH (160 + 2 * WALKER_RADIUS)							 // a slide's ~155
static walker_area near_area;

// The roofs of `a` within `reach` of (x, y), into `to`. `a` may be `to` itself, narrowed already
// (run_on, then a flight or a slide within it): kept roofs only move down the list, so it's
// narrowed again in place. Its count's read first
static const walker_area *roofs_into(walker_area *to, const walker_area *a, int x, int y, int reach)
{
	const int count = a->count;

	to->count = 0;
	for (int i = 0; i < count; ++i)
		if (on_roof(&a->rects[i], x, y, reach))
		{
			to->rects[to->count] = a->rects[i];
			to->heights[to->count] = a->heights[i];
			to->bottoms[to->count] = a->bottoms[i];
			to->count++;
		}
	return to;
}

static const walker_area *roofs_near(const walker_area *a, int x, int y, int reach)
{
	return roofs_into(&near_area, a, x, y, reach);
}

/*
 * The roofs a frame can touch, once at its start (the user, 2026-11-18: the frames still missed
 * jumping the city's streets, the CPU at 108-120% there). Out in the city the area's ~300 roofs,
 * and a frame's step tested every one of them several times over: roof_at, wall_in_way and
 * footprint_height on each axis, the lip and the run's moves. ~12% of a frame (the debug line's
 * wk, less the drones' dr). Nothing a frame does looks further from you than run_on's narrowing:
 * a flight's reach and onto_top's step on past an edge, from where this frame's step takes you (a
 * run's 6, a fall's drift no more). So every test against these roofs gives just what it gave
 * against all of them: the same walk, from a dozen roofs. In an area of its own, not near_area:
 * the moves narrow that in place, and walk() goes on with this frame's afterwards
 */
#define WALKER_FRAME_REACH (WALKER_FLIGHT_REACH + WALKER_RADIUS + 2 * WALKER_FOOT + 32)
static walker_area frame_area;
int walker_frame_narrowing = 1; // (walker.h: the tests turn it off, to walk the same with all)

/*
 * Running off a lip at full speed: a jump, if it lands clean. The whole flight's flown first, on
 * a copy of you, frame by frame as it will be (nothing steers it once you've left, so it comes
 * out the same): taken only if it comes down on a roof within WALKER_FLIGHT_FRAMES_MAX frames,
 * without hitting a side, and not so far down you'd stagger. Up to 64 frames of fall(): one
 * jump's worth of work, once, as you reach the lip (after a refusal the run's lost, so it isn't
 * tried again every frame you stand there). Returns whether you jumped
 */
static int vault_off(walker *w, const walker_area *area)
{
	walker flight = *w;

	flight.falling = 1;
	flight.fall_speed = -WALKER_VAULT_HOP;
	flight.fall_from = w->z >> WALKER_SUB_SHIFT;
	flight.take_off = flight.fall_from;
	area = roofs_near(area, flight.x >> WALKER_SUB_SHIFT, flight.y >> WALKER_SUB_SHIFT, WALKER_FLIGHT_REACH);
	for (int i = 0; i < WALKER_FLIGHT_FRAMES_MAX; ++i)
	{
		if (!fall(&flight, area))
			return 0; // into the side of something
		if (!flight.falling)
		{
			if (flight.stagger > 0)
				return 0; // a long drop: that's for pushing off the lip, meaning it
			w->falling = 1; // as the copy started
			w->fall_speed = -WALKER_VAULT_HOP;
			w->fall_from = w->z >> WALKER_SUB_SHIFT;
			w->take_off = w->fall_from;
			w->push = 0;
			return 1;
		}
	}
	return 0; // still falling: over nothing, or too far down
}

/*
 * Getting onto what's in front of you (the vault over's start, and a climb): from (x, y), where
 * you were blocked, on along the axis (sign: -1 or 1) a few units at a time, until your middle's
 * on a roof `height` high with room to stand on it. Up to WALKER_RADIUS + 2 x WALKER_FOOT on: you
 * were blocked up to WALKER_RADIUS off its edge. Returns whether there's such a place: in
 * *px, *py (whole units)
 */
static int onto_top(const walker_area *area, int x, int y, int along_x, int sign, int height, int *px, int *py)
{
	for (int on = 0; on <= WALKER_RADIUS + 2 * WALKER_FOOT; on += WALKER_FOOT >> 1)
	{
		const int cx = x + (along_x ? sign * on : 0);
		const int cy = y + (along_x ? 0 : sign * on);
		const int under = roof_at(area, cx, cy, height);

		if (under >= 0 && area->heights[under] == height &&
			!wall_at(area, cx, cy, height, height + WALKER_STEP_UP, height + WALKER_HEAD))
		{
			*px = cx;
			*py = cy;
			return 1;
		}
	}
	return 0;
}

/*
 * A vault over something low (no higher than your waist, solid to the ground) in your way at
 * (x, y): onto its near edge, then a small hop off it, flying on at your speed. Flown first on a
 * copy, as a jump is (vault_off): taken only if you come down clean beyond it, lower than its
 * top. Too deep to clear, you'd come down on it: that's a vault up onto it instead (walk). Returns
 * whether you vaulted: you're in the air, your feet on its top (the eye following)
 */
static int vault_over(walker *w, const walker_area *area, int along_x, int sign, int x, int y)
{
	const int feet = w->z >> WALKER_SUB_SHIFT;
	const walker_block b = blocked_by(area, x, y, feet, feet + WALKER_STEP_UP, feet + WALKER_HEAD);
	walker flight;
	int px, py;

	if (b.count == 0 || b.top > feet + WALKER_WAIST || b.bottom > feet)
		return 0; // nothing, too high, or a bar (it's not low and solid)
	if (!onto_top(area, x, y, along_x, sign, b.top, &px, &py))
		return 0;
	flight = *w;
	flight.x = px * WALKER_SUB;
	flight.y = py * WALKER_SUB;
	flight.z = b.top * WALKER_SUB;
	flight.falling = 1;
	flight.fall_speed = -WALKER_VAULT_OVER_HOP;
	flight.fall_from = b.top;
	flight.take_off = b.top;
	area = roofs_near(area, flight.x >> WALKER_SUB_SHIFT, flight.y >> WALKER_SUB_SHIFT, WALKER_FLIGHT_REACH);
	for (int i = 0; i < WALKER_FLIGHT_FRAMES_MAX; ++i)
	{
		if (!fall(&flight, area))
			return 0; // into the side of something
		if (!flight.falling)
		{
			if (flight.stagger > 0 || walker_height(&flight) >= b.top)
				return 0; // a long drop beyond, or down on its own top: not over it
			lag_by(w, b.top - feet);
			w->x = px * WALKER_SUB;
			w->y = py * WALKER_SUB;
			w->z = b.top * WALKER_SUB;
			w->falling = 1; // as the copy started
			w->fall_speed = -WALKER_VAULT_OVER_HOP;
			w->fall_from = b.top;
			w->take_off = b.top;
			w->push = 0;
			w->move = WALKER_MOVE_VAULT;
			return 1;
		}
	}
	return 0;
}

/*
 * A climb onto a ledge in your way at (x, y), no more than WALKER_CLIMB above your feet: you're
 * on its top, just past its edge, your feet up at once and the eye following slowly (walker_update
 * eases a big lag at half the pace). It takes your run: you're at a walk on top. Returns whether
 * you climbed
 */
static int climb(walker *w, const walker_area *area, int along_x, int sign, int x, int y)
{
	const int feet = w->z >> WALKER_SUB_SHIFT;
	const walker_block b = blocked_by(area, x, y, feet, feet + WALKER_STEP_UP, feet + WALKER_HEAD);
	int px, py;

	if (b.count == 0 || b.top > feet + WALKER_CLIMB)
		return 0;
	if (!onto_top(area, x, y, along_x, sign, b.top, &px, &py))
		return 0;
	lag_by(w, b.top - feet);
	w->x = px * WALKER_SUB;
	w->y = py * WALKER_SUB;
	w->z = b.top * WALKER_SUB;
	if (w->forward > WALKER_FORWARD_SPEED)
		w->forward = WALKER_FORWARD_SPEED;
	w->push = 0;
	w->move = WALKER_MOVE_CLIMB;
	return 1;
}

static void walk(walker *w, int dx, int dy, const walker_area *area, int pushing); // below

/*
 * One frame of a slide: on the way you were facing as it started (the pad doesn't steer it, and
 * doesn't turn you), slowing by WALKER_SLIDE_FRICTION, your head low (head_room). Walls and lips
 * stop you as walking would: you never slide off a roof
 */
static void slide_frame(walker *w, const walker_area *area)
{
	const int forward_x = -walker_sin(w->yaw);
	const int forward_y = -walker_cos(w->yaw);
	int dx, dy;

	w->forward -= WALKER_SLIDE_FRICTION;
	if (w->forward < 0)
		w->forward = 0;
	w->side = 0;
	dx = ASR(forward_x * w->forward, 10);
	dy = ASR(forward_y * w->forward, 10);
	w->air_dx = dx;
	w->air_dy = dy;
	walk(w, dx, dy, area, 0);
	if (w->slide > 0)
		w->slide--;
}

/*
 * A slide under a bar in your way at (x, y): only if it's in your way standing but not sliding
 * (its bottom between WALKER_SLIDE_HEAD and WALKER_HEAD above your feet), and only if the whole
 * slide, run first on a copy of you, ends with room to stand. Otherwise you'd be left lying under
 * it. Returns whether it started (you move from next frame)
 */
static int slide_under(walker *w, const walker_area *area, int x, int y)
{
	const int feet = w->z >> WALKER_SUB_SHIFT;
	walker copy;
	world_point at;

	if (!wall_at(area, x, y, feet, feet + WALKER_STEP_UP, feet + WALKER_HEAD) ||
		wall_at(area, x, y, feet, feet + WALKER_STEP_UP, feet + WALKER_SLIDE_HEAD))
		return 0; // not in your way, or in it even sliding
	copy = *w;
	copy.slide = WALKER_SLIDE_FRAMES;
	area = roofs_near(area, x, y, WALKER_SLIDE_REACH);
	while (copy.slide > 0)
		slide_frame(&copy, area);
	at = walker_position(&copy);
	if (copy.falling ||
		wall_at(area, at.x, at.y, walker_height(&copy), walker_height(&copy) + WALKER_STEP_UP,
				walker_height(&copy) + WALKER_HEAD))
		return 0; // still under it at the end (or under another)
	w->slide = WALKER_SLIDE_FRAMES;
	w->push = 0;
	return 1;
}

/*
 * Blocked head on running (WALKER_MOVE_SPEED), going `d` (x256) along an axis: the moves that
 * carry you on, in order: slide under a bar, vault over something low and thin, vault up onto something low, climb
 * a ledge. Returns whether one did
 */
static int run_on(walker *w, const walker_area *area, int along_x, int d)
{
	const int sign = d > 0 ? 1 : -1;
	const int x = (w->x + (along_x ? d : 0)) >> WALKER_SUB_SHIFT;
	const int y = (w->y + (along_x ? 0 : d)) >> WALKER_SUB_SHIFT;
	int s;

	// The roofs a move from here can reach, once: each move tries several tests (a climb up to
	// ten places on the ledge), and against every roof the frame a climb started cost ~8% of it.
	// A flight reaches furthest, so every move's within it (and narrows it again for itself). A
	// vault over's flight starts on the top, up to WALKER_RADIUS + 2 x WALKER_FOOT further on
	// (onto_top): that much more, so its own narrowing never needs a roof this left out
	area = roofs_near(area, x, y, WALKER_FLIGHT_REACH + WALKER_RADIUS + 2 * WALKER_FOOT);
	if (slide_under(w, area, x, y) || vault_over(w, area, along_x, sign, x, y))
		return 1;
	s = step_axis(w, area, x, y, WALKER_WAIST);
	if (s == 1 || s == 3)
	{
		const int on = d + sign * WALKER_FOOT * WALKER_SUB; // onto its top

		if (along_x)
			w->x += on;
		else
			w->y += on;
		if (s == 3)
			w->move = WALKER_MOVE_CLIMB; // a vault up: hands on its edge, as a short climb
		return 1;
	}
	return climb(w, area, along_x, sign, x, y);
}

/*
 * The moves start from WALKER_MOVE_SPEED, not a full run, but each one is taken at a full run: as
 * a move starts you're kicked up to WALKER_RUN_SPEED (the last strides of a run-up). So a jump's
 * as long, a slide as far and a vault over as high whether you came at 4.5 a frame or 6: the same
 * move every time, as walker.h promises. The flight goes the way you were moving (air_dx, air_dy,
 * strafing too), worked out again at the new speed. Tried on the kicked walker; not taken, you're
 * put back as you were
 */
static void kick_to_a_run(walker *w)
{
	const int forward_x = -walker_sin(w->yaw);
	const int forward_y = -walker_cos(w->yaw);

	if (w->forward >= WALKER_RUN_SPEED)
		return;
	w->forward = WALKER_RUN_SPEED;
	w->air_dx = ASR(forward_x * w->forward + forward_y * w->side, 10); // as walker_update's dx, dy
	w->air_dy = ASR(forward_y * w->forward - forward_x * w->side, 10);
}

static int kicked_run_on(walker *w, const walker_area *area, int along_x, int d)
{
	const walker before = *w;

	kick_to_a_run(w);
	if (run_on(w, area, along_x, d))
		return 1;
	*w = before;
	return 0;
}

static int kicked_vault_off(walker *w, const walker_area *area)
{
	const walker before = *w;

	kick_to_a_run(w);
	if (vault_off(w, area))
		return 1;
	*w = before;
	return 0;
}

/*
 * On a roof: each axis on its own (one that's blocked is left out, and you slide along the edge
 * on the other). At a lip you stop, unless you're running (WALKER_MOVE_SPEED) and the jump off
 * it lands clean (vault_off, kicked to a run), or you've pushed into it long enough (step_off): then you go on over it. Stopped
 * head on (the blocked axis most of the way you're going: not sliding along a wall), a run's lost
 */
static void walk(walker *w, int dx, int dy, const walker_area *area, int pushing)
{
	const int fast = w->forward >= WALKER_MOVE_SPEED; // quick enough for the moves: not a full run
	const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
	int lip = 0, head_on = 0;

	if (dx != 0)
	{
		const int sx = step_axis(w, area, (w->x + dx) >> WALKER_SUB_SHIFT, w->y >> WALKER_SUB_SHIFT, WALKER_STEP_UP);

		if (sx == 1)
			w->x += dx;
		else
		{
			head_on = 2 * ax >= ay;
			if (sx == 0 && fast && head_on && kicked_run_on(w, area, 1, dx))
				return;
			if (sx == 2)
			{
				lip = 1;
				if (fast && head_on && kicked_vault_off(w, area))
					return; // up and away: the flight starts next frame, as vault_off flew it
				if (step_off(w, area, 1, dx > 0 ? 1 : -1, pushing))
				{
					w->x += dx;
					w->y += dy;
					return;
				}
			}
		}
	}
	if (dy != 0)
	{
		const int sy = step_axis(w, area, w->x >> WALKER_SUB_SHIFT, (w->y + dy) >> WALKER_SUB_SHIFT, WALKER_STEP_UP);

		if (sy == 1)
			w->y += dy;
		else
		{
			head_on |= 2 * ay >= ax;
			if (sy == 0 && fast && 2 * ay >= ax && kicked_run_on(w, area, 0, dy))
				return;
			if (sy == 2 && !lip)
			{
				lip = 1;
				if (fast && 2 * ay >= ax && kicked_vault_off(w, area))
					return;
				if (step_off(w, area, 0, dy > 0 ? 1 : -1, pushing))
				{
					w->y += dy;
					return;
				}
			}
		}
	}
	if (head_on && w->forward > WALKER_FORWARD_SPEED)
		w->forward = WALKER_FORWARD_SPEED; // stopped short: back to a walk
	if (!lip)
		w->push = 0; // let go of the lip: pushing starts again from the beginning
}

// The landing's dip dying away, and the eye catching up with your feet after a step: a quarter of
// the way a frame, at least 1. After a climb (more than your waist up) an eighth: you pull
// yourself up, slower than a step
static void ease_eye(walker *w)
{
	if (w->dip > 0)
		w->dip--;
	if (w->lag != 0)
	{
		const int step = w->lag > WALKER_WAIST ? w->lag >> 3 : w->lag >> 2; // rounds down: -1 for -1..-4

		w->lag = (signed char)(w->lag - (step != 0 ? step : 1));
	}
	// A move's over once you're down from it (a vault) and the eye's all but caught up (a climb)
	if (w->move != WALKER_MOVE_NONE && !w->falling && w->lag < WALKER_MOVE_DONE_LAG)
		w->move = WALKER_MOVE_NONE;
}

void walker_update(walker *w, walker_controls controls, const walker_area *area)
{
	int forward_wanted, side_wanted, dx, dy;
	const int forward_x = -walker_sin(w->yaw); // facing (x1024): north is -y, east -x (world.h)
	const int forward_y = -walker_cos(w->yaw);
	// No area at all: nowhere's off it, and it's all at height 0. Asked of the whole area, before
	// it's narrowed to this frame's roofs: with none near, you're over nothing, not anywhere
	const int no_area = area == 0 || area->count == 0;

	if (!no_area && walker_frame_narrowing)
		area = roofs_into(&frame_area, area, w->x >> WALKER_SUB_SHIFT, w->y >> WALKER_SUB_SHIFT, WALKER_FRAME_REACH);

	// Sliding: the slide moves you, not the pad (slide_frame). The eye drops low, a quarter of the
	// way a frame; afterwards it straightens back up as from the crash (below)
	if (w->slide > 0 && !w->falling)
	{
		const int step = (w->eye - WALKER_EYE_SLIDE) >> 2;

		w->eye = (unsigned char)(w->eye > WALKER_EYE_SLIDE ? w->eye - (step > 0 ? step : 1) : w->eye);
		w->turn_speed = 0;
		ease_eye(w);
		if (!no_area)
			slide_frame(w, area);
		else
			w->slide = 0;
		w->stride = (w->stride + ((w->forward * 3) >> 1)) & 0xFFFF;
		return;
	}

	// Turning: a small first step, quicker each frame it's held, and stopped dead on letting go
	// (or on turning the other way: the first step that way, from nothing)
	if (controls.turn == 0)
		w->turn_speed = 0;
	else if ((w->turn_speed > 0 ? 1 : w->turn_speed < 0 ? -1 : 0) != controls.turn)
		w->turn_speed = controls.turn * WALKER_TURN_FIRST;
	else
	{
		w->turn_speed += controls.turn * WALKER_TURN_SPEED_UP;
		if (w->turn_speed > WALKER_TURN_MAX)
			w->turn_speed = WALKER_TURN_MAX;
		if (w->turn_speed < -WALKER_TURN_MAX)
			w->turn_speed = -WALKER_TURN_MAX;
	}
	w->yaw = (w->yaw + w->turn_speed) & 0xFFFF;

	// The speeds wanted, then a frame towards them. Walking and strafing at once keep both: a
	// sidestep costs nothing going forwards (walker.h)
	forward_wanted = controls.move > 0 ? WALKER_FORWARD_SPEED : controls.move < 0 ? -WALKER_BACK_SPEED : 0;
	side_wanted = controls.strafe * WALKER_STRAFE_SPEED;
	if (w->stagger > 0)
	{
		// A long drop's stagger: a creep at most, any way
		forward_wanted = clamp_speed(forward_wanted, WALKER_CREEP_SPEED);
		side_wanted = clamp_speed(side_wanted, WALKER_CREEP_SPEED);
		w->stagger--;
	}
	// Running: holding forward, at walking speed already, you want more. Strafing and turning
	// keep it (you line up on the way); staggering doesn't (the creep's clamp above)
	if (controls.move > 0 && w->stagger == 0 && w->forward >= WALKER_FORWARD_SPEED)
		forward_wanted = WALKER_RUN_SPEED;
	// In the air there's nothing to push off: the speeds stay as you left the roof
	if (!w->falling)
	{
		if (forward_wanted > w->forward && w->forward >= WALKER_FORWARD_SPEED)
		{
			// Past walking speed, the run builds slowly (approach's speeding up is for starting)
			w->forward += WALKER_RUN_SPEED_UP;
			if (w->forward > forward_wanted)
				w->forward = forward_wanted;
		}
		else
			w->forward = approach(w->forward, forward_wanted);
		w->side = approach(w->side, side_wanted);
	}
	ease_eye(w);
	// Straightening up: an eighth of the way a frame, at least 1 (most of it in ~half a second)
	if (w->eye < WALKER_EYE)
	{
		const int step = (WALKER_EYE - w->eye) >> 3;

		w->eye = (unsigned char)(w->eye + (step > 0 ? step : 1));
	}

	// The move: forwards along where you face, and sideways along your right, which is facing a
	// quarter turn on: (-cos, sin), which is (forward_y, -forward_x). Facing north, your right is
	// east, -x. x1024 x the speeds x256, >> 10: x256 again
	dx = ASR(forward_x * w->forward + forward_y * w->side, 10);
	dy = ASR(forward_y * w->forward - forward_x * w->side, 10);
	if (no_area) // (above)
	{
		w->x += dx;
		w->y += dy;
	}
	else if (w->falling)
	{
		fall(w, area); // the way you left: dx and dy (where you face now) don't steer it
		return;		   // no steps in the air
	}
	else
	{
		// On a roof: this frame's way is the way you'd leave it (stepping off, or jumping)
		w->air_dx = dx;
		w->air_dy = dy;
		walk(w, dx, dy, area, controls.move != 0 || controls.strafe != 0);
	}

	// The step's phase, on with the speed
	{
		const int speed = (w->forward < 0 ? -w->forward : w->forward) + (w->side < 0 ? -w->side : w->side);

		w->stride = (w->stride + ((speed * 3) >> 1)) & 0xFFFF;
	}
}

world_point walker_position(const walker *w)
{
	world_point p;

	p.x = (short)(w->x >> WALKER_SUB_SHIFT);
	p.y = (short)(w->y >> WALKER_SUB_SHIFT);
	return p;
}

int walker_bob(const walker *w)
{
	const int s = walker_sin(w->stride);
	const int speed = (w->forward < 0 ? -w->forward : w->forward) + (w->side < 0 ? -w->side : w->side);

	// Down at each step (both halves of the phase's turn), as deep as the speed's share of full
	return ((s < 0 ? -s : s) * speed) >> WALKER_BOB_SHIFT;
}

int walker_height(const walker *w)
{
	return w->z >> WALKER_SUB_SHIFT;
}

int walker_dip(const walker *w)
{
	return w->dip;
}

int walker_eye(const walker *w)
{
	return w->eye;
}

int walker_step_lag(const walker *w)
{
	return w->lag;
}

int walker_move(const walker *w)
{
	return w->move;
}

int walker_sliding(const walker *w)
{
	return w->slide > 0;
}

int walker_fell_off(const walker *w, const walker_area *area)
{
	const int feet = w->z >> WALKER_SUB_SHIFT;

	// The cheap test first: only once you're that far down does it look under you (every roof)
	return w->falling && feet < w->fall_from - WALKER_LOST_DROP &&
		   roof_at(area, w->x >> WALKER_SUB_SHIFT, w->y >> WALKER_SUB_SHIFT, feet) < 0;
}
