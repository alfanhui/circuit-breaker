#include "attract_text.h"

/*
 * A letter's path, in font grid units: x 0..6 rightwards, y 0..10 upwards (its baseline is 0).
 * The first point is where the cycle arrives, riding right, from the gap before the letter
 * (x -1); pen says whether the trail is laid on the way to each point after that. Every path
 * is drawn so the cycle never has to turn straight back on itself: pen-up moves go round
 * (sometimes 1 unit outside the letter) instead.
 */
typedef struct
{
	signed char x;
	signed char y;
	unsigned char pen;
} glyph_point;

#define END {0, 0, 2} // pen 2 marks the end of a path

// S: from the bottom left, along, up the right, back, up the left, along the top
static const glyph_point glyph_s[] = {
	{-1, 0, 0}, {0, 0, 0}, {6, 0, 1}, {6, 5, 1}, {0, 5, 1}, {0, 10, 1}, {6, 10, 1}, END};
// E: the middle bar, round the right (pen up) to the top, then top, spine and bottom in one go
static const glyph_point glyph_e[] = {
	{-1, 5, 0}, {0, 5, 0}, {5, 5, 1}, {7, 5, 0}, {7, 10, 0}, {6, 10, 0}, {0, 10, 1}, {0, 0, 1}, {6, 0, 1}, END};
// G: in above the top, round to its right end, then the C and the spur
static const glyph_point glyph_g[] = {{-1, 11, 0}, {7, 11, 0}, {7, 10, 0}, {6, 10, 0}, {0, 10, 1}, {0, 0, 1},
									  {6, 0, 1},   {6, 4, 1},	{3, 4, 1},	END};
// A: the bar, round underneath (pen up), then up, across and down
static const glyph_point glyph_a[] = {{-1, 5, 0}, {0, 5, 0}, {6, 5, 1}, {7, 5, 0},	{7, -1, 0}, {0, -1, 0},
									  {0, 0, 0},  {0, 10, 1}, {6, 10, 1}, {6, 0, 1}, END};
// T: the top, round above it (pen up), then down the stem
static const glyph_point glyph_t[] = {{-1, 10, 0}, {0, 10, 0}, {6, 10, 1}, {7, 10, 0}, {7, 11, 0},
									  {3, 11, 0},  {3, 10, 0}, {3, 0, 1},	 END};
// U: down, along, up
static const glyph_point glyph_u[] = {{-1, 11, 0}, {0, 11, 0}, {0, 10, 0}, {0, 0, 1}, {6, 0, 1}, {6, 10, 1}, END};
// R: a P (up, across, down, back to the spine), then a stepped leg from the middle of its bar
static const glyph_point glyph_r[] = {{-1, -1, 0}, {0, -1, 0}, {0, 0, 0}, {0, 10, 1}, {6, 10, 1}, {6, 5, 1},
									  {0, 5, 1},   {-1, 5, 0}, {-1, 6, 0}, {3, 6, 0}, {3, 5, 0},  {3, 2, 1},
									  {6, 2, 1},   {6, 0, 1},  END};
// N: the pixel-font zig-zag: up, half across, down, half across, up
static const glyph_point glyph_n[] = {{-1, -1, 0}, {0, -1, 0}, {0, 0, 0}, {0, 10, 1}, {3, 10, 1},
									  {3, 0, 1},   {6, 0, 1},  {6, 10, 1}, END};
// H: up the left, over the top (pen up) and down the right, round underneath to the bar
static const glyph_point glyph_h[] = {{-1, -1, 0}, {0, -1, 0}, {0, 0, 0},	{0, 10, 1}, {0, 11, 0}, {6, 11, 0},
									  {6, 10, 0},  {6, 0, 1},  {6, -1, 0}, {-1, -1, 0}, {-1, 5, 0}, {0, 5, 0},
									  {6, 5, 1},   END};
// -: a short bar across the middle
static const glyph_point glyph_dash[] = {{-1, 5, 0}, {1, 5, 0}, {5, 5, 1}, END};
// 2: along the top, down, back, down, along the bottom
static const glyph_point glyph_2[] = {
	{-1, 10, 0}, {0, 10, 0}, {6, 10, 1}, {6, 5, 1}, {0, 5, 1}, {0, 0, 1}, {6, 0, 1}, END};

#define LETTER_ADVANCE 9 // 6 wide and a gap of 3
#define SPACE_ADVANCE 6
#define START_X (-4) // where the cycle starts, riding right (font units, from the first letter)
#define START_Y 5

static const glyph_point *glyph_for(char c)
{
	switch (c)
	{
	case 'S':
		return glyph_s;
	case 'E':
		return glyph_e;
	case 'G':
		return glyph_g;
	case 'A':
		return glyph_a;
	case 'T':
		return glyph_t;
	case 'U':
		return glyph_u;
	case 'R':
		return glyph_r;
	case 'N':
		return glyph_n;
	case 'H':
		return glyph_h;
	case '-':
		return glyph_dash;
	case '2':
		return glyph_2;
	default:
		return 0; // a space (or anything without a glyph): just room
	}
}

// What's being built, in font units until each point is written out in world units
typedef struct
{
	attract_waypoint *out;
	int max;
	int count;
	world_point origin; // the world position of font point (0, 0)
	int x;				// where the cycle is now, in font units
	int y;
	int heading_x; // the way it's riding: -1 / 0 / 1 on each axis (font units, y up)
	int heading_y;
} builder;

// Write the point (x, y) (font units) as the next waypoint, in world units. Font right is world
// east (-x), font up is world north (-y): the spectator camera faces north
static void write_point(builder *b, int x, int y, int pen)
{
	attract_waypoint *w;

	if (b->count >= b->max)
	{
		b->count = b->max + 1; // too many: the build fails
		return;
	}
	w = &b->out[b->count++];
	w->p.x = (short)(b->origin.x - x * ATTRACT_TEXT_UNIT);
	w->p.y = (short)(b->origin.y - y * ATTRACT_TEXT_UNIT);
	w->pen = (unsigned char)pen;
	w->word_end = 0;
}

// Ride to (x, y) (in line with where it is), laying the trail or not. A point it's already at is
// skipped, so no leg has no length
static void ride_to(builder *b, int x, int y, int pen)
{
	if (x == b->x && y == b->y)
		return;
	b->heading_x = x > b->x ? 1 : x < b->x ? -1 : 0;
	b->heading_y = y > b->y ? 1 : y < b->y ? -1 : 0;
	b->x = x;
	b->y = y;
	write_point(b, x, y, pen);
}

// Pen up, from wherever it is to (x, y), arriving riding right (a letter's entry, or the end).
// It rides right to the gap just before, then up or down, then right into the point. If it's
// riding left now, it steps 1 unit up or down first, so it never turns straight back
static void ride_in_from_left(builder *b, int x, int y)
{
	if (b->heading_x < 0)
		ride_to(b, b->x, b->y + (y >= b->y ? 1 : -1), 0);
	ride_to(b, x - 1, b->y, 0);
	ride_to(b, x - 1, y, 0);
	ride_to(b, x, y, 0);
}

int attract_text_build(const char *text, world_point origin, attract_waypoint *waypoints, int max)
{
	builder b;
	int origin_x = 0; // the current letter's bottom left, in font units
	int last_pen = -1;

	b.out = waypoints;
	b.max = max;
	b.count = 0;
	b.origin = origin;
	b.x = START_X;
	b.y = START_Y;
	b.heading_x = 1;
	b.heading_y = 0;
	// The first waypoint: where it starts, riding right (and where it's put back after each message)
	write_point(&b, START_X, START_Y, 0);

	for (const char *c = text; *c != 0; ++c)
	{
		const glyph_point *g = glyph_for(*c);

		if (g == 0)
		{
			// A space: the word before it is done
			if (last_pen >= 0)
				b.out[last_pen].word_end = 1;
			origin_x += SPACE_ADVANCE;
			continue;
		}
		ride_in_from_left(&b, origin_x + g[0].x, g[0].y);
		for (int i = 1; g[i].pen != 2; ++i)
		{
			ride_to(&b, origin_x + g[i].x, g[i].y, g[i].pen);
			if (g[i].pen)
				last_pen = b.count - 1;
		}
		origin_x += LETTER_ADVANCE;
	}
	if (last_pen >= 0)
		b.out[last_pen].word_end = 1; // the last word
	// On to the start again, a whole period on, riding the same way
	ride_in_from_left(&b, START_X + ATTRACT_TEXT_PERIOD / ATTRACT_TEXT_UNIT, START_Y);
	return b.count <= max ? b.count : 0;
}
