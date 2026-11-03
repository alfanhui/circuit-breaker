#include "test.h"
#include "../src/game/corridor.h"

#define HALF 8000

// Round 5's: behind the west wall (+x), -500 to 500 along y
static void open_round_five(corridor *c)
{
	wall_flicker f;

	wall_flicker_set_round(&f, 4);
	corridor_open(c, &f, HALF);
}

// Whether p is inside the convex polygon (either way round), strictly
static int inside(const world_point *poly, int n, int x, int y)
{
	int sign = 0;

	for (int i = 0; i < n; ++i)
	{
		const world_point a = poly[i];
		const world_point b = poly[i + 1 < n ? i + 1 : 0];
		const long long cross = (long long)(b.x - a.x) * (y - a.y) - (long long)(b.y - a.y) * (x - a.x);
		const int s = cross > 0 ? 1 : cross < 0 ? -1 : 0;

		if (s == 0)
			return 0;
		if (sign == 0)
			sign = s;
		else if (s != sign)
			return 0;
	}
	return 1;
}

// Which tile covers (x, y): its index, -1 for none, -2 for more than one
static int tile_at(const corridor *c, int x, int y)
{
	int found = -1;

	for (int i = 0; i < c->tile_count; ++i)
		if (inside(c->tiles[i].corners, c->tiles[i].count, x, y))
			found = found == -1 ? i : -2;
	return found;
}

static int twice_area(const world_point *p, int n)
{
	long long a = 0;

	for (int i = 0; i < n; ++i)
	{
		const world_point q = p[i + 1 < n ? i + 1 : 0];

		a += (long long)p[i].x * q.y - (long long)q.x * p[i].y;
	}
	return (int)(a < 0 ? -a : a);
}

static void closed_until_it_opens(void)
{
	corridor c;

	corridor_init(&c);
	CHECK_EQ(0, c.open);
	CHECK_EQ(0, c.tile_count);
	open_round_five(&c);
	CHECK_EQ(1, c.open);
}

// (out, across) to the world: out is away from the middle through the side's wall
static void points_run_out_through_the_wall(void)
{
	corridor c;
	world_point p;

	open_round_five(&c); // west: +x
	p = corridor_point(&c, 100, -300);
	CHECK_EQ(HALF + 100, p.x);
	CHECK_EQ(-300, p.y);
	c.side = WALL_FLICKER_NORTH; // -y, across is x
	p = corridor_point(&c, 100, -300);
	CHECK_EQ(-300, p.x);
	CHECK_EQ(-HALF - 100, p.y);
	c.side = WALL_FLICKER_EAST; // -x
	p = corridor_point(&c, 100, 40);
	CHECK_EQ(-HALF - 100, p.x);
	CHECK_EQ(40, p.y);
	c.side = WALL_FLICKER_SOUTH; // +y
	p = corridor_point(&c, 100, 40);
	CHECK_EQ(40, p.x);
	CHECK_EQ(HALF + 100, p.y);
}

// And back: how far out a world point is, whichever side
static void out_is_from_the_wall(void)
{
	corridor c;

	open_round_five(&c);
	CHECK_EQ(100, corridor_out(&c, corridor_point(&c, 100, 20)));
	CHECK_EQ(-50, corridor_out(&c, corridor_point(&c, -50, 20))); // still in the arena
	for (int side = 0; side < 4; ++side)
	{
		c.side = (unsigned char)side;
		CHECK_EQ(1234, corridor_out(&c, corridor_point(&c, 1234, -77)));
	}
}

// Its walls: either side of the section, from the arena's wall to the far end, and across it
static void its_walls_frame_the_section(void)
{
	corridor c;
	world_point from[CORRIDOR_FACES], to[CORRIDOR_FACES];

	open_round_five(&c);
	corridor_faces(&c, from, to);
	CHECK_EQ(HALF, from[0].x);
	CHECK_EQ(-500, from[0].y);
	CHECK_EQ(HALF + CORRIDOR_LENGTH, to[0].x);
	CHECK_EQ(-500, to[0].y);
	CHECK_EQ(500, from[1].y);
	CHECK_EQ(500, to[1].y);
	CHECK_EQ(HALF + CORRIDOR_LENGTH, from[2].x);
	CHECK_EQ(HALF + CORRIDOR_LENGTH, to[2].x);
	CHECK_EQ(-500, from[2].y);
	CHECK_EQ(0, to[2].y); // only its left half: the right opens onto the city (city.h)
	CHECK_EQ(0, (CORRIDOR_LENGTH % 250)); // whole wall pieces
}

// A square: inside the quad it's untouched; half out, half of it's left; apart, nothing. The
// quad either way round
static void clip_keeps_what_is_inside(void)
{
	const world_point square[4] = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
	const world_point big[4] = {{-1000, -1000}, {1000, -1000}, {1000, 1000}, {-1000, 1000}};
	const world_point big_back[4] = {{-1000, 1000}, {1000, 1000}, {1000, -1000}, {-1000, -1000}};
	const world_point left[4] = {{-1000, -1000}, {50, -1000}, {50, 1000}, {-1000, 1000}};
	const world_point far[4] = {{5000, 5000}, {6000, 5000}, {6000, 6000}, {5000, 6000}};
	world_point out[CORRIDOR_CLIP_MAX];

	CHECK_EQ(4, corridor_clip(square, 4, big, out));
	CHECK_EQ(20000, twice_area(out, 4));
	CHECK_EQ(4, corridor_clip(square, 4, big_back, out));
	CHECK_EQ(20000, twice_area(out, 4));
	CHECK_EQ(4, corridor_clip(square, 4, left, out));
	CHECK_EQ(10000, twice_area(out, 4));
	CHECK_EQ(0, corridor_clip(square, 4, far, out));
}

// A slanted quad (as the screen's, seen on the floor) cutting a square corner to corner
static void clip_on_a_slant(void)
{
	const world_point square[4] = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
	// Its first side runs along x + y = 100; the other three are far off
	const world_point slant[4] = {{-900, 1000}, {1100, -1000}, {3100, 1000}, {1100, 3000}};
	world_point out[CORRIDOR_CLIP_MAX];
	int n = corridor_clip(square, 4, slant, out);

	CHECK_EQ(3, n); // what's left is the triangle (100, 0) (100, 100) (0, 100): half the square
	CHECK_EQ(10000, twice_area(out, n));
}

// Every tile lies in the corridor's floor: out from the arena's wall to the far end, between
// its walls. None was dropped for want of room
static void tiles_stay_in_the_corridor(void)
{
	corridor c;

	open_round_five(&c);
	CHECK(c.tile_count > 100);
	CHECK(c.tile_count < CORRIDOR_TILES_MAX);
	for (int i = 0; i < c.tile_count; ++i)
	{
		CHECK(c.tiles[i].count >= 3 && c.tiles[i].count <= CORRIDOR_TILE_CORNERS);
		CHECK(c.tiles[i].shade < CORRIDOR_SHADES);
		for (int k = 0; k < c.tiles[i].count; ++k)
		{
			CHECK(c.tiles[i].corners[k].x >= HALF && c.tiles[i].corners[k].x <= HALF + CORRIDOR_LENGTH);
			CHECK(c.tiles[i].corners[k].y >= -500 && c.tiles[i].corners[k].y <= 500);
		}
	}
}

// Past the scattered first columns the glass is whole but for its seams: no point on two tiles,
// and most on one. Neighbours (a step apart, on different tiles) are never the same shade. Up to
// the grey box (corridor_boxes): under its boxes, tiles are left out
static void past_the_tail_the_glass_is_whole(void)
{
	corridor c;
	const int start = CORRIDOR_TAIL_COLUMNS * CORRIDOR_HEX_STEP + CORRIDOR_HEX_R;
	int on_one = 0, samples = 0;

	open_round_five(&c);
	for (int across = -497; across < 500; across += 13)
	{
		int last = -1, gap = 0;

		for (int out = start + 3; out < corridor_boxes[0].out_min - CORRIDOR_HEX_R; out += 7)
		{
			const int t = tile_at(&c, HALF + out, across);

			samples++;
			CHECK(t != -2);
			if (t < 0)
			{
				gap++; // a seam
				continue;
			}
			on_one++;
			// Across a seam (a few steps: a seam's ~16 wide) the next tile is a neighbour
			if (last >= 0 && t != last && gap <= 4)
				CHECK(c.tiles[t].shade != c.tiles[last].shade);
			last = t;
			gap = 0;
		}
	}
	// The seams are a sixteenth in from each side: about an eighth of the floor (seven in eight
	// is on a tile), compared without dividing
	CHECK(on_one * 8 > samples * 6);
	CHECK(on_one < samples); // and there are seams
}

// The grid tails off: the first column's mostly grid, and there's more glass along the tail
static void the_grid_tails_off(void)
{
	corridor c;
	int covered[CORRIDOR_TAIL_COLUMNS + 1] = {0};
	int samples[CORRIDOR_TAIL_COLUMNS + 1] = {0};

	open_round_five(&c);
	for (int out = 3; out < (CORRIDOR_TAIL_COLUMNS + 1) * CORRIDOR_HEX_STEP; out += 11)
	{
		// Which column's stretch of floor: each column owns its step, centred on it
		int column = 0;

		while ((column + 1) * CORRIDOR_HEX_STEP - CORRIDOR_HEX_STEP / 2 <= out)
			column++;
		if (column > CORRIDOR_TAIL_COLUMNS)
			break;
		for (int across = -495; across < 500; across += 17)
		{
			samples[column]++;
			covered[column] += tile_at(&c, HALF + out, across) >= 0;
		}
	}
	// Shares, compared without dividing: a / b < c / d is a x d < c x b
	CHECK(covered[0] * samples[CORRIDOR_TAIL_COLUMNS] < covered[CORRIDOR_TAIL_COLUMNS] * samples[0]);
	CHECK(covered[0] < samples[0]);			// some grid shows in the first
	{
		// More glass in the tail's second half than its first (a column's only ~5 tiles: compared
		// in halves, the scatter's luck evens out)
		int first_covered = 0, first_samples = 0, second_covered = 0, second_samples = 0;

		for (int k = 0; k < CORRIDOR_TAIL_COLUMNS; ++k)
		{
			if (k * 2 < CORRIDOR_TAIL_COLUMNS)
			{
				first_covered += covered[k];
				first_samples += samples[k];
			}
			else
			{
				second_covered += covered[k];
				second_samples += samples[k];
			}
		}
		CHECK(first_covered * second_samples < second_covered * first_samples);
	}
}

// On foot in the grey box: from where the cycle stops, holding forward up the corridor's left half
// builds to a run, vaults the crate (up onto it, off its end), climbs the stairs a step at a time
// onto the platform (160), and jumps off its
// end onto the lower roof (64), a clean landing, running on to the tower (a wall). On the right
// half, off the platform, it's a long drop to the floor: no jump, but pushing on drops you. Facing
// out is facing west here (+x: a three-quarter turn)
static void the_grey_box_climbs_and_drops(void)
{
	corridor c;
	walker_area area;
	walker w;
	world_point at;
	walker_controls ahead = {0, 1, 0, 0, 0};
	int frames;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	at = corridor_point(&c, 2300, c.across_min + ((c.across_max - c.across_min) >> 2)); // the left half
	walker_init(&w, at, 0xC000);
	for (frames = 0; frames < 1200 && !w.falling; ++frames)
		walker_update(&w, ahead, &area);
	CHECK(w.falling); // up the crate (40), and off its end
	CHECK_EQ(40, walker_height(&w)); // leaving from its top (the jump starts next frame)
	CHECK(corridor_out(&c, walker_position(&w)) >= 3054 && corridor_out(&c, walker_position(&w)) <= 3060);
	for (frames = 0; frames < 300 && w.falling; ++frames)
		walker_update(&w, ahead, &area);
	CHECK_EQ(0, walker_height(&w)); // down on the floor, running on
	CHECK_EQ(0, w.stagger);
	CHECK_EQ(WALKER_RUN_SPEED, w.forward);
	for (frames = 0; frames < 1200 && !w.falling; ++frames)
		walker_update(&w, ahead, &area);
	CHECK(w.falling); // up the stairs, and off the platform's end
	CHECK_EQ(WALKER_RUN_SPEED, w.forward);
	CHECK(corridor_out(&c, walker_position(&w)) >= 3894 && corridor_out(&c, walker_position(&w)) <= 3900);
	for (frames = 0; frames < 300 && w.falling; ++frames)
		walker_update(&w, ahead, &area);
	CHECK_EQ(64, walker_height(&w)); // a clean landing on the lower roof
	CHECK(!w.falling);
	CHECK_EQ(0, w.stagger);
	for (frames = 0; frames < 300 && w.push == 0; ++frames)
		walker_update(&w, ahead, &area);
	CHECK(corridor_out(&c, walker_position(&w)) <= 4200 - WALKER_RADIUS); // the tower: a wall
	CHECK_EQ(64, walker_height(&w));

	// The right half: off the platform to the floor, 160 down
	at = corridor_point(&c, 3700, c.across_max - ((c.across_max - c.across_min) >> 2));
	walker_init_at(&w, at, 160, 0xC000);
	for (frames = 0; frames < 400 && (w.falling || walker_height(&w) == 160); ++frames)
		walker_update(&w, ahead, &area);
	CHECK_EQ(0, walker_height(&w));
	CHECK(w.stagger > 0);
}

void run_corridor_tests(void)
{
	RUN(the_grey_box_climbs_and_drops);
	RUN(closed_until_it_opens);
	RUN(points_run_out_through_the_wall);
	RUN(out_is_from_the_wall);
	RUN(its_walls_frame_the_section);
	RUN(clip_keeps_what_is_inside);
	RUN(clip_on_a_slant);
	RUN(tiles_stay_in_the_corridor);
	RUN(past_the_tail_the_glass_is_whole);
	RUN(the_grid_tails_off);
}
