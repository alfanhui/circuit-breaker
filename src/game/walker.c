#include "walker.h"

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
	value = i >= 64 ? quarter_sine[64] : quarter_sine[i] + (((quarter_sine[i + 1] - quarter_sine[i]) * (within & 0xFF)) >> 8);
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
	world_rect *r;

	if (a->count >= WALKER_AREA_RECTS_MAX)
		return 0;
	a->heights[a->count] = (short)height;
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
	w->air_dx = 0;
	w->air_dy = 0;
	w->falling = 0;
	w->lag = 0;
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

void walker_look_from_buttons(int a, int b, int c, int x, int y, int z, signed char *look_x, signed char *look_y)
{
	// The left column looks left, the right column right; the top row up, the bottom row down.
	// Both ways at once cancel out
	const int left = a | x, right = c | z;
	const int up = x | y | z, down = a | b | c;

	*look_x = (signed char)(right - left);
	*look_y = (signed char)(up - down);
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

// Whether a roof higher than `top` (a kerb above your feet, walking) stands in your way at a
// point: its side's a wall. You keep WALKER_RADIUS off a tall one, but only WALKER_FOOT off one
// lower than your waist (a step you can't step: your foot hits it, not your body)
static int wall_at(const walker_area *a, int x, int y, int feet, int top)
{
	const int waist = feet + WALKER_WAIST;

	for (int i = 0; i < a->count; ++i)
		if (a->heights[i] > top && on_roof(&a->rects[i], x, y, a->heights[i] > waist ? WALKER_RADIUS : WALKER_FOOT))
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
 *   - within WALKER_RADIUS of a roof more than a kerb above your feet: a wall, blocked. At a full
 *     run, though, a roof no higher than your waist you vault up onto: it's a step that big, and
 *     you're carried on, WALKER_FOOT, onto its top (walk). Left with your middle still over the
 *     ground, more than a kerb below your feet, the next step was a lip: you jumped straight off
 *   - your middle on a roof no more than a kerb up or down: moved
 *   - otherwise (lower, or nothing): a lip. You can stand right at a roof's edge, to look over
 * Moved, your feet go to the highest roof under your footprint (a kerb up at most), not just
 * under your middle: you step up a stair as your foot reaches it, as you would. Only stepping up
 * once your middle got there, the eye (35 up) was below the next step's top 3 units from its
 * front, too near to draw it: you saw in under the stairs. The eye follows the feet (lag_by)
 */
static int step_axis(walker *w, const walker_area *area, int x, int y, int full_run)
{
	const int feet = w->z >> WALKER_SUB_SHIFT;
	int top = feet + WALKER_STEP_UP; // the highest you can step up to
	int under, to;

	if (wall_at(area, x, y, feet, top))
	{
		if (!full_run || wall_at(area, x, y, feet, feet + WALKER_WAIST))
			return 0;
		top = feet + WALKER_WAIST; // a vault up: as a step, up to your waist
	}
	under = roof_at(area, x, y, top);
	if (under < 0 || area->heights[under] < feet - WALKER_STEP_UP)
		return 2;
	to = footprint_height(area, x, y, top, area->heights[under]);
	lag_by(w, to - feet);
	w->z = to * WALKER_SUB;
	return to > feet + WALKER_STEP_UP ? 3 : 1;
}

/*
 * In the air: gravity, and the way you left the roof (air_dx, air_dy). Going up (a jump: the fall
 * speed's negative) the top's where the drop's measured from. You can't go into the side of a roof
 * higher than your feet (you'd hit it: you stop against it and fall on down its side); you land
 * on one once your feet reach it. How far you fell decides the landing. Returns false if you hit
 * a side this frame
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
	if (walker_area_height(area, (w->x + w->air_dx) >> WALKER_SUB_SHIFT, w->y >> WALKER_SUB_SHIFT) <= across)
		w->x += w->air_dx;
	else if (w->air_dx != 0)
		clear = 0;
	if (walker_area_height(area, w->x >> WALKER_SUB_SHIFT, (w->y + w->air_dy) >> WALKER_SUB_SHIFT) <= across)
		w->y += w->air_dy;
	else if (w->air_dy != 0)
		clear = 0;

	// Landing: on the highest roof under you lower than where you fell from (not the one you
	// stepped off, still under your middle for the first frame or two)
	under = roof_at(area, w->x >> WALKER_SUB_SHIFT, w->y >> WALKER_SUB_SHIFT, w->fall_from - 1);
	if (under >= 0 && feet <= area->heights[under])
	{
		const int drop = w->fall_from - area->heights[under];

		w->falling = 0;
		w->fall_speed = 0;
		w->z = area->heights[under] * WALKER_SUB;
		// The eye dips with the landing: an eighth of the drop, up to 8 (12 for a long drop)
		w->dip = (unsigned char)((drop >> 3) > 8 ? 8 : drop >> 3);
		if (drop > WALKER_CLEAN_DROP)
		{
			w->stagger = WALKER_STAGGER_FRAMES;
			w->dip = 12;
		}
	}
	return clear;
}

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
			w->push = 0;
			return 1;
		}
	}
	return 0; // still falling: over nothing, or too far down
}

/*
 * On a roof: each axis on its own (one that's blocked is left out, and you slide along the edge
 * on the other). At a lip you stop, unless you're at a full run and the jump off it lands clean
 * (vault_off), or you've pushed into it long enough (step_off): then you go on over it. Stopped
 * head on (the blocked axis most of the way you're going: not sliding along a wall), a run's lost
 */
static void walk(walker *w, int dx, int dy, const walker_area *area, int pushing)
{
	const int full_run = w->forward >= WALKER_RUN_SPEED;
	const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
	int lip = 0, head_on = 0;

	if (dx != 0)
	{
		const int sx = step_axis(w, area, (w->x + dx) >> WALKER_SUB_SHIFT, w->y >> WALKER_SUB_SHIFT, full_run);

		if (sx == 1 || sx == 3)
			w->x += dx + (sx == 3 ? (dx > 0 ? WALKER_FOOT : -WALKER_FOOT) * WALKER_SUB : 0);
		else
		{
			head_on = 2 * ax >= ay;
			if (sx == 2)
			{
				lip = 1;
				if (full_run && head_on && vault_off(w, area))
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
		const int sy = step_axis(w, area, w->x >> WALKER_SUB_SHIFT, (w->y + dy) >> WALKER_SUB_SHIFT, full_run);

		if (sy == 1 || sy == 3)
			w->y += dy + (sy == 3 ? (dy > 0 ? WALKER_FOOT : -WALKER_FOOT) * WALKER_SUB : 0);
		else
		{
			head_on |= 2 * ay >= ax;
			if (sy == 2 && !lip)
			{
				lip = 1;
				if (full_run && 2 * ay >= ax && vault_off(w, area))
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

void walker_update(walker *w, walker_controls controls, const walker_area *area)
{
	int forward_wanted, side_wanted, dx, dy;
	const int forward_x = -walker_sin(w->yaw); // facing (x1024): north is -y, east -x (world.h)
	const int forward_y = -walker_cos(w->yaw);

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

	// The speeds wanted, then a frame towards them. Walking and strafing at once, each's cut to
	// 1 / sqrt 2 of itself (x 181 >> 8), so going diagonally isn't the quickest way about
	forward_wanted = controls.move > 0 ? WALKER_FORWARD_SPEED : controls.move < 0 ? -WALKER_BACK_SPEED : 0;
	side_wanted = controls.strafe * WALKER_STRAFE_SPEED;
	if (forward_wanted != 0 && side_wanted != 0)
	{
		forward_wanted = (forward_wanted * WALKER_DIAGONAL_X256) >> 8;
		side_wanted = (side_wanted * WALKER_DIAGONAL_X256) >> 8;
	}
	if (w->stagger > 0)
	{
		// A long drop's stagger: a creep at most, any way
		forward_wanted = clamp_speed(forward_wanted, WALKER_CREEP_SPEED);
		side_wanted = clamp_speed(side_wanted, WALKER_CREEP_SPEED);
		w->stagger--;
	}
	// Running: walking straight on, at walking speed already, you want more. Not strafing, not
	// staggering (the creep's clamp above), and not turning hard: a turn held up to its full speed
	if (controls.move > 0 && controls.strafe == 0 && w->stagger == 0 && w->forward >= WALKER_FORWARD_SPEED &&
		w->turn_speed < WALKER_TURN_MAX && w->turn_speed > -WALKER_TURN_MAX)
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
	if (w->dip > 0)
		w->dip--;
	// The eye catching up with your feet after a step: a quarter of the way a frame, at least 1
	if (w->lag != 0)
	{
		const int step = w->lag >> 2; // rounds down: -1 for -1..-4, never 0 below 0

		w->lag = (signed char)(w->lag - (step != 0 ? step : 1));
	}
	// Straightening up: an eighth of the way a frame, at least 1 (most of it in ~half a second)
	if (w->eye < WALKER_EYE)
	{
		const int step = (WALKER_EYE - w->eye) >> 3;

		w->eye = (unsigned char)(w->eye + (step > 0 ? step : 1));
	}

	// The move: forwards along where you face, and sideways along your right, which is facing a
	// quarter turn on: (-cos, sin), which is (forward_y, -forward_x). Facing north, your right is
	// east, -x. x1024 x the speeds x256, >> 10: x256 again
	dx = (forward_x * w->forward + forward_y * w->side) >> 10;
	dy = (forward_y * w->forward - forward_x * w->side) >> 10;
	// No area at all: nowhere's off it, and it's all at height 0
	if (area == 0 || area->count == 0)
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
