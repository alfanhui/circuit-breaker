#include "target.h"

/*
 * atan, for 0 .. 1 in 64 steps: ATAN_TABLE[i] is atan(i / 64) as an ANGLE (0x2000 is 45 degrees).
 * Worked out once on the PC (round(atan(i / 64) x 0x8000 / pi)): no floats on the Saturn
 */
static const short ATAN_TABLE[65] = {
	0, 163, 326, 489, 651, 813, 975, 1136, 1297, 1457, 1617, 1775, 1933, 2090, 2246, 2401, 2555,
	2708, 2860, 3010, 3159, 3307, 3453, 3599, 3742, 3884, 4025, 4164, 4302, 4438, 4572, 4705, 4836,
	4966, 5094, 5220, 5344, 5467, 5589, 5708, 5826, 5943, 6058, 6171, 6282, 6392, 6500, 6607, 6712,
	6815, 6917, 7018, 7117, 7214, 7310, 7405, 7498, 7589, 7679, 7768, 7856, 7942, 8026, 8110, 8192};

// atan(n / d) for 0 <= n <= d, d > 0: 0 .. 0x2000. The ratio x4096, then the table, its steps
// shared out in a straight line. One division: per target, never per element in a loop
static int atan_ratio(int n, int d)
{
	const int t = (n << 12) / d; // 0 .. 4096
	const int i = t >> 6, part = t & 63;

	if (i >= 64)
		return ATAN_TABLE[64];
	return ATAN_TABLE[i] + (((ATAN_TABLE[i + 1] - ATAN_TABLE[i]) * part) >> 6);
}

// The ANGLE (0 .. 0xFFFF) whose sine goes with s and cosine with c: a whole-circle atan from the
// table's eighth of it, by the signs and which of the two is bigger
static int atan2_angle(int s, int c)
{
	const int as = s < 0 ? -s : s, ac = c < 0 ? -c : c;
	int a;

	if (as == 0 && ac == 0)
		return 0;
	a = as <= ac ? atan_ratio(as, ac) : 0x4000 - atan_ratio(ac, as); // 0 .. 0x4000 from +c
	if (c < 0)
		a = 0x8000 - a;
	if (s < 0)
		a = -a;
	return a & 0xFFFF;
}

// An angle's difference, the short way round: -0x8000 .. 0x7FFF
static int turn_between(int to, int from)
{
	return (short)(to - from);
}

void target_list_clear(target_list *list)
{
	list->count = 0;
}

int target_list_add(target_list *list, int x, int y, int z, target_kind kind)
{
	target_marker *m;

	if (list->count >= TARGETS_MAX)
		return 0;
	m = &list->markers[list->count++];
	m->x = (short)x;
	m->y = (short)y;
	m->z = (short)z;
	m->kind = (unsigned char)kind;
	return 1;
}

void target_lock_init(target_lock *lock)
{
	lock->locked = TARGET_NONE;
	lock->blind = 0;
}

int target_bearing(int dx, int dy)
{
	// Facing yaw, you go along (-sin yaw, -cos yaw) (walker.h): so sin yaw goes with -dx and
	// cos yaw with -dy
	return atan2_angle(-dx, -dy);
}

int target_pitch(int dz, int dx, int dy)
{
	// How far away along the floor, without a square root: the larger plus 3/8 of the smaller
	// (within ~7% of the true length: a degree or two of pitch, at most)
	const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
	const int along = ax > ay ? ax + ((3 * ay) >> 3) : ay + ((3 * ax) >> 3);

	return (short)atan2_angle(dz, along); // up and down are within a quarter turn: signed
}

// Whether a rectangle's in the way of a line from (x0, y0) to (x1, y1) on the floor. No division:
// first their boxes must overlap; then the rectangle's corners mustn't all be on one side of the
// line (the cross product's sign says which side). Products stay well inside 32 bits: the line's
// no longer than TARGET_RANGE, the corners within a level's 32768
static int crosses(const world_rect *r, int x0, int y0, int x1, int y1)
{
	const int dx = x1 - x0, dy = y1 - y0;
	int left = 0, right = 0;

	if ((x0 < x1 ? x0 : x1) > r->x_max || (x0 > x1 ? x0 : x1) < r->x_min || (y0 < y1 ? y0 : y1) > r->y_max ||
		(y0 > y1 ? y0 : y1) < r->y_min)
		return 0;
	for (int corner = 0; corner < 4; ++corner)
	{
		const int cx = (corner & 1) ? r->x_max : r->x_min;
		const int cy = (corner & 2) ? r->y_max : r->y_min;
		const int side = dx * (cy - y0) - dy * (cx - x0);

		left |= side > 0;
		right |= side < 0;
	}
	return left == right; // corners both sides, or one on the line
}

static int holds(const world_rect *r, int x, int y)
{
	return x >= r->x_min && x <= r->x_max && y >= r->y_min && y <= r->y_max;
}

/*
 * In sight: no roof crosses the line between you and it that's taller than the lower of the two
 * (an eye on a platform looking down at a crate: the platform's edge doesn't hide it; a tower
 * between does). The roofs you stand on and it stands on are left out. A bar (a roof with a
 * bottom) high enough to pass under doesn't hide what's at its foot. A rough rule (it doesn't
 * follow the line's height across the roof: that's a division per roof), but the level's boxes
 * are plain: change it when one fools it
 */
int target_in_sight(const walker_area *area, const target_eye *eye, const target_marker *m)
{
	const int low = eye->z < m->z ? eye->z : m->z;

	if (area == 0)
		return 1;
	for (int i = 0; i < area->count; ++i)
	{
		const world_rect *r = &area->rects[i];

		if (area->heights[i] <= low || area->bottoms[i] > low)
			continue;
		if (holds(r, eye->x, eye->y) || holds(r, m->x, m->y))
			continue;
		if (crosses(r, eye->x, eye->y, m->x, m->y))
			return 0;
	}
	return 1;
}

// Within range and the body's reach (not sight: that's dearer, tested last)
static int in_reach(const target_eye *eye, const target_marker *m)
{
	const int dx = m->x - eye->x, dy = m->y - eye->y;
	const int far = dx * dx + dy * dy; // squared: no square root

	if (far > TARGET_RANGE * TARGET_RANGE || far < 64 * 64)
		return 0; // too far, or underfoot (no way to face it)
	{
		const int off = turn_between(target_bearing(dx, dy), eye->yaw);

		return off <= TARGET_REACH && off >= -TARGET_REACH;
	}
}

static int can_lock(const walker_area *area, const target_eye *eye, const target_marker *m)
{
	return in_reach(eye, m) && target_in_sight(area, eye, m);
}

// Whether an enemy can be locked onto now: then nothing else can
static int enemy_about(const target_list *list, const target_eye *eye, const walker_area *area)
{
	for (int i = 0; i < list->count; ++i)
		if (list->markers[i].kind == TARGET_ENEMY && can_lock(area, eye, &list->markers[i]))
			return 1;
	return 0;
}

// A marker's turn from where the view looks now: + right
static int off_view(const target_eye *eye, const target_marker *m)
{
	return turn_between(target_bearing(m->x - eye->x, m->y - eye->y), eye->view_yaw);
}

int target_press_side(target_lock *lock, const target_list *list, const target_eye *eye, const walker_area *area, int side)
{
	const int enemies_only = enemy_about(list, eye, area);
	int best = TARGET_NONE, best_off = 0x10000;

	if (lock->locked != TARGET_NONE)
	{
		target_lock_init(lock); // pressed again: let go
		return 0;
	}
	for (int i = 0; i < list->count; ++i)
	{
		const target_marker *m = &list->markers[i];
		int off;

		if (enemies_only && m->kind != TARGET_ENEMY)
			continue;
		if (!can_lock(area, eye, m))
			continue;
		off = off_view(eye, m);
		// On the side pressed (one dead ahead is on both); an enemy, on either
		if (!enemies_only && off * side < 0)
			continue;
		off = off < 0 ? -off : off;
		if (off < best_off)
		{
			best = i;
			best_off = off;
		}
	}
	lock->locked = (signed char)best;
	lock->blind = 0;
	return best != TARGET_NONE;
}

void target_press_switch(target_lock *lock, const target_list *list, const target_eye *eye, const walker_area *area, int dir)
{
	const target_marker *now;
	int from, best = TARGET_NONE, best_step = 0x10000;
	int enemies_only;

	if (lock->locked == TARGET_NONE || dir == 0)
		return;
	now = &list->markers[lock->locked];
	from = target_bearing(now->x - eye->x, now->y - eye->y);
	enemies_only = enemy_about(list, eye, area);
	for (int i = 0; i < list->count; ++i)
	{
		const target_marker *m = &list->markers[i];
		int step;

		if (i == lock->locked || (enemies_only && m->kind != TARGET_ENEMY) || !can_lock(area, eye, m))
			continue;
		// How far round from the locked one, the way pressed: the nearest that way next
		step = turn_between(target_bearing(m->x - eye->x, m->y - eye->y), from) * dir;
		if (step > 0 && step < best_step)
		{
			best = i;
			best_step = step;
		}
	}
	if (best != TARGET_NONE)
	{
		lock->locked = (signed char)best;
		lock->blind = 0;
	}
}

void target_frame(target_lock *lock, const target_list *list, const target_eye *eye, const walker_area *area)
{
	const target_marker *m;

	if (lock->locked == TARGET_NONE)
		return;
	if (lock->locked >= list->count)
	{
		target_lock_init(lock); // the list changed under it (a new level): nothing to hold
		return;
	}
	m = &list->markers[lock->locked];
	if (!in_reach(eye, m))
	{
		target_lock_init(lock); // run past it, or out of range: let go at once
		return;
	}
	if (target_in_sight(area, eye, m))
		lock->blind = 0;
	else if (++lock->blind > TARGET_BLIND_FRAMES)
		target_lock_init(lock);
}

int target_aim(const target_lock *lock, const target_list *list, const target_eye *eye, int *head_yaw, int *head_pitch)
{
	const target_marker *m;
	int dx, dy;

	if (lock->locked == TARGET_NONE)
		return 0;
	m = &list->markers[lock->locked];
	dx = m->x - eye->x;
	dy = m->y - eye->y;
	*head_yaw = turn_between(target_bearing(dx, dy), eye->yaw);
	*head_pitch = target_pitch(m->z - eye->z, dx, dy);
	return 1;
}
