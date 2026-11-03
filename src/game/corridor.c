#include "corridor.h"
#include "city.h"

// North and south walls run along x; east and west along y
static int runs_along_x(int side)
{
	return side == WALL_FLICKER_NORTH || side == WALL_FLICKER_SOUTH;
}

void corridor_init(corridor *c)
{
	c->open = 0;
	c->tile_count = 0;
}

world_point corridor_point(const corridor *c, int out, int across)
{
	// The sides are numbered as the headings (wall_flicker.h): riding into the side's wall is
	// riding out, so the heading's step is the way out (north is -y, east is -x: world.h)
	const int from_middle = c->wall + out;
	world_point p;

	if (runs_along_x(c->side))
	{
		p.x = (short)across;
		p.y = (short)(WORLD_HEADING_DY(c->side) * from_middle);
	}
	else
	{
		p.x = (short)(WORLD_HEADING_DX(c->side) * from_middle);
		p.y = (short)across;
	}
	return p;
}

int corridor_out(const corridor *c, world_point p)
{
	// Along the side's axis, outwards (the heading's step), less the wall's distance
	const int from_middle = runs_along_x(c->side) ? WORLD_HEADING_DY(c->side) * p.y : WORLD_HEADING_DX(c->side) * p.x;

	return from_middle - c->wall;
}

void corridor_faces(const corridor *c, world_point *from, world_point *to)
{
	from[0] = corridor_point(c, 0, c->across_min); // one side
	to[0] = corridor_point(c, CORRIDOR_LENGTH, c->across_min);
	from[1] = corridor_point(c, CORRIDOR_LENGTH, c->across_max); // the other
	to[1] = corridor_point(c, 0, c->across_max);
	// Across the far end: only its left half. The right half opens onto Work RAM's city (city.h),
	// the entry tower's roof level with the floor
	from[2] = corridor_point(c, CORRIDOR_LENGTH, c->across_min);
	to[2] = corridor_point(c, CORRIDOR_LENGTH, (c->across_min + c->across_max) >> 1);
}

// num / den rounded to the nearest whole number. Two tiles cut the same edge from opposite ends:
// truncated, their points came out a unit apart, leaving a hairline crack between them; rounded,
// both land on the same point
static int rounded_share(int num, int den)
{
	if (den < 0)
	{
		num = -num;
		den = -den;
	}
	return num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den);
}

/*
 * How far along an edge (delta long) its crossing is: delta x dc / (dc - dn), rounded. dc and dn
 * are 64-bit (corners thousands apart), and a 64-bit division is a library call on the SH-2 of
 * hundreds of cycles; one for each edge cut, every tile, every frame, it was much of the glass
 * floor's third of the frame. So both are brought down together until their difference fits in
 * 14 bits: delta (under 65536) x that stays inside 32 bits, and the division's an ordinary one.
 * What's lost is under one part in 8000 of the share: under a unit on an edge.
 * Each is brought down as a magnitude, its sign kept, so -v comes out as v does: two tiles cutting
 * one edge from opposite ends (the same sums, signs swapped) still land on the same point. And the
 * divisor's the two brought down, not their difference brought down (they can differ by 1). One
 * bit at a time: a 64-bit shift by a count that varies is a library call on the SH-2, by 1 it's two
 * instructions
 */
static int edge_share(int delta, long long dc, long long dn)
{
	long long size = dc - dn, c = dc < 0 ? -dc : dc, n = dn < 0 ? -dn : dn;

	size = size < 0 ? -size : size;
	while (size > 16383)
	{
		size >>= 1;
		c >>= 1;
		n >>= 1;
	}
	{
		const int ci = dc < 0 ? -(int)c : (int)c, ni = dn < 0 ? -(int)n : (int)n;

		return ci == ni ? 0 : rounded_share(delta * ci, ci - ni);
	}
}

// Which side of the line a -> b p is: twice the triangle's signed area. 64-bit: corners tens of
// thousands of units apart make products past 32 bits
static long long side_of(world_point a, world_point b, world_point p)
{
	return (long long)(b.x - a.x) * (p.y - a.y) - (long long)(b.y - a.y) * (p.x - a.x);
}

int corridor_clip(const world_point *poly, int n, const world_point quad[4], world_point *out)
{
	world_point a[CORRIDOR_CLIP_MAX]; // two buffers in turn: each side reads one, writes the other
	world_point b[CORRIDOR_CLIP_MAX];
	world_point *from = a, *to = b;
	long long area = 0;
	int x_lo = 32767, x_hi = -32768, y_lo = 32767, y_hi = -32768;
	int qx_lo = 32767, qx_hi = -32768, qy_lo = 32767, qy_hi = -32768;

	// A quick test of the two boxes first: most tiles are nowhere near the screen's edge
	for (int i = 0; i < n; ++i)
	{
		x_lo = poly[i].x < x_lo ? poly[i].x : x_lo;
		x_hi = poly[i].x > x_hi ? poly[i].x : x_hi;
		y_lo = poly[i].y < y_lo ? poly[i].y : y_lo;
		y_hi = poly[i].y > y_hi ? poly[i].y : y_hi;
	}
	for (int i = 0; i < 4; ++i)
	{
		qx_lo = quad[i].x < qx_lo ? quad[i].x : qx_lo;
		qx_hi = quad[i].x > qx_hi ? quad[i].x : qx_hi;
		qy_lo = quad[i].y < qy_lo ? quad[i].y : qy_lo;
		qy_hi = quad[i].y > qy_hi ? quad[i].y : qy_hi;
		area += side_of(quad[0], quad[i], quad[(i + 1) & 3]);
	}
	if (n < 3 || x_hi < qx_lo || x_lo > qx_hi || y_hi < qy_lo || y_lo > qy_hi || area == 0)
		return 0;

	for (int i = 0; i < n; ++i)
		from[i] = poly[i];
	// Each side of the quad keeps what's on its inner side: left of it if the quad goes round
	// anticlockwise (its area > 0), right of it if clockwise
	for (int e = 0; e < 4 && n >= 3; ++e)
	{
		const world_point p0 = quad[e];
		const world_point p1 = quad[(e + 1) & 3];
		// Each corner's side of this edge, worked out once (each is two corners' "cur" and "next"),
		// and whether they're all inside it: then the edge cuts nothing, and the corners stay
		long long sides[CORRIDOR_CLIP_MAX];
		int m = 0, outside = 0;

		for (int i = 0; i < n; ++i)
		{
			sides[i] = area > 0 ? side_of(p0, p1, from[i]) : -side_of(p0, p1, from[i]);
			outside += sides[i] < 0;
		}
		if (outside == 0)
			continue;
		if (outside == n)
			return 0; // all of it outside this edge
		for (int i = 0; i < n; ++i)
		{
			const int j = i + 1 < n ? i + 1 : 0;
			const world_point cur = from[i];
			const world_point next = from[j];
			const long long dc = sides[i];
			const long long dn = sides[j];

			if (dc >= 0)
				to[m++] = cur;
			// Only an edge from one side strictly to the other crosses it: a corner exactly on
			// the side is kept above, and taking it again here would put it in twice
			if ((dc > 0 && dn < 0) || (dc < 0 && dn > 0))
			{
				// The edge crosses the side: the point a share dc / (dc - dn) of the way along
				world_point p;

				p.x = (short)(cur.x + edge_share(next.x - cur.x, dc, dn));
				p.y = (short)(cur.y + edge_share(next.y - cur.y, dc, dn));
				to[m++] = p;
			}
		}
		n = m;
		{
			world_point *t = from;

			from = to;
			to = t;
		}
	}
	if (n < 3)
		return 0; // only touching: an edge or a point
	for (int i = 0; i < n; ++i)
		out[i] = from[i];
	return n;
}

// Which of the first columns' hexagons are laid: a scatter that's the same every time, from the
// tile's place (a multiply and a shift: 0..7)
static int scatter(int column, int row)
{
	const unsigned int h = (unsigned int)column * 73856093u ^ (unsigned int)(row + 64) * 19349663u;

	return (int)((h >> 13) & 7);
}

// Whether a tile (corners in (out, across)) lies wholly under the grey box's boxes, every corner
// under one or another: then it's never seen, and drawing it only costs VDP1 the pixels again
// (the boxes are drawn over it). A tile's wider than a step: most lie under two. (Boxes with a
// gap between them narrower than a tile could hide one wrongly; the grey box has none)
static int under_a_box(const corridor *c, const world_point *corners, int n)
{
	const int width = c->across_max - c->across_min;

	for (int i = 0; i < n; ++i)
	{
		int b;

		for (b = 0; b < CORRIDOR_BOXES; ++b)
		{
			const corridor_box *box = &corridor_boxes[b];

			if (corners[i].x >= box->out_min && corners[i].x <= box->out_max &&
				corners[i].y >= c->across_min + ((width * box->across_min) >> 4) &&
				corners[i].y <= c->across_min + ((width * box->across_max) >> 4))
				break;
		}
		if (b == CORRIDOR_BOXES)
			return 0; // this corner's on the open floor
	}
	return 1;
}

void corridor_open(corridor *c, const wall_flicker *f, int half)
{
	// The corridor's floor in (out, across): what every hexagon is cut to
	world_point floor[4];

	c->open = 1;
	c->side = f->side;
	c->across_min = f->along_min;
	c->across_max = (short)wall_flicker_along_max(f);
	c->wall = (short)half;
	c->tile_count = 0;
	floor[0].x = 0;
	floor[0].y = c->across_min;
	floor[1].x = CORRIDOR_LENGTH;
	floor[1].y = c->across_min;
	floor[2].x = CORRIDOR_LENGTH;
	floor[2].y = c->across_max;
	floor[3].x = 0;
	floor[3].y = c->across_max;

	// Column k is k steps out; its centres are m H across from the corridor's first wall, m
	// stepping by 2 and odd in the odd columns. (u, k), u = (m - k) / 2, are the hexagons' axial
	// coordinates: the 6 neighbours are u +-1, k +-1, and (u - 1, k + 1), (u + 1, k - 1), so
	// (u - k) mod 3 gives every neighbour a different shade
	for (int k = 0; k * CORRIDOR_HEX_STEP < CORRIDOR_LENGTH + CORRIDOR_HEX_R; ++k)
	{
		const int o = k * CORRIDOR_HEX_STEP;

		for (int m = (k & 1) - 2; (m - 1) * CORRIDOR_HEX_H < c->across_max - c->across_min; m += 2)
		{
			const int a = c->across_min + m * CORRIDOR_HEX_H;
			// Pointed along out, flat across, in order round it
			const world_point hex[6] = {
				{(short)(o + CORRIDOR_HEX_R), (short)a},
				{(short)(o + CORRIDOR_HEX_R / 2), (short)(a + CORRIDOR_HEX_H)},
				{(short)(o - CORRIDOR_HEX_R / 2), (short)(a + CORRIDOR_HEX_H)},
				{(short)(o - CORRIDOR_HEX_R), (short)a},
				{(short)(o - CORRIDOR_HEX_R / 2), (short)(a - CORRIDOR_HEX_H)},
				{(short)(o + CORRIDOR_HEX_R / 2), (short)(a - CORRIDOR_HEX_H)},
			};
			world_point cut[CORRIDOR_CLIP_MAX];
			corridor_tile *t;
			int n;

			// The grid tails off: in the first columns only some are laid, more each column. Laid
			// when scatter / 8 < (k + 1) / columns, compared multiplied out: 2 in 8 in the first,
			// all of them by the last
			if (k < CORRIDOR_TAIL_COLUMNS && scatter(k, m) * CORRIDOR_TAIL_COLUMNS >= (k + 1) * 8)
				continue;
			n = corridor_clip(hex, 6, floor, cut);
			if (n == 0 || c->tile_count >= CORRIDOR_TILES_MAX || under_a_box(c, cut, n))
				continue;
			t = &c->tiles[c->tile_count++];
			t->count = (unsigned char)n;
			t->centre = corridor_point(c, o, a);
			// (u - k) mod 3, kept positive: u can be -1. Once per tile, as it opens
			t->shade = (unsigned char)((((m - k) / 2 - k) % CORRIDOR_SHADES + CORRIDOR_SHADES) % CORRIDOR_SHADES);
			// Shrunk a little towards its middle, leaving a seam round it. The middle of what's
			// left after the cut (its corners' average), not the hexagon's centre: a tile cut by
			// the corridor's wall can have its centre outside, and would grow past the wall.
			// Divisions, but once per tile, as it opens
			{
				int mid_out = 0, mid_across = 0;

				for (int i = 0; i < n; ++i)
				{
					mid_out += cut[i].x;
					mid_across += cut[i].y;
				}
				mid_out /= n;
				mid_across /= n;
				for (int i = 0; i < n; ++i)
					t->corners[i] = corridor_point(c, cut[i].x - (cut[i].x - mid_out) / CORRIDOR_SEAM_PARTS,
												   cut[i].y - (cut[i].y - mid_across) / CORRIDOR_SEAM_PARTS);
			}
		}
	}
}

const corridor_box corridor_boxes[CORRIDOR_BOXES] = {
	// The stairs: twenty steps 12 deep and 8 high (28 cm and 18 cm), up the left half
	{3360, 3372, 0, 8, 8},
	{3372, 3384, 0, 8, 16},
	{3384, 3396, 0, 8, 24},
	{3396, 3408, 0, 8, 32},
	{3408, 3420, 0, 8, 40},
	{3420, 3432, 0, 8, 48},
	{3432, 3444, 0, 8, 56},
	{3444, 3456, 0, 8, 64},
	{3456, 3468, 0, 8, 72},
	{3468, 3480, 0, 8, 80},
	{3480, 3492, 0, 8, 88},
	{3492, 3504, 0, 8, 96},
	{3504, 3516, 0, 8, 104},
	{3516, 3528, 0, 8, 112},
	{3528, 3540, 0, 8, 120},
	{3540, 3552, 0, 8, 128},
	{3552, 3564, 0, 8, 136},
	{3564, 3576, 0, 8, 144},
	{3576, 3588, 0, 8, 152},
	{3588, 3600, 0, 8, 160},
	// The platform, across the whole corridor: two halves, so the left's front shows only above
	// the top step (one box across, its front was drawn all the way down behind the stairs)
	{3600, 3900, 0, 8, 160},
	{3600, 3900, 8, 16, 160},
	{3900, 4200, 0, 8, 64},  // a clean drop from it, on the left (96)
	{4200, 4400, 0, 8, 400}, // a tower beyond: a wall
	// Last, so the stairs stay first: a crate before them, where you stand up (a quarter across).
	// Waist-high: walking, your foot meets it; at a run you vault onto it and jump off its end
	{3000, 3060, 2, 6, 40},
};

void corridor_box_corners(const corridor *c, const corridor_box *box, world_point *a, world_point *b)
{
	const int width = c->across_max - c->across_min;

	*a = corridor_point(c, box->out_min, c->across_min + ((width * box->across_min) >> 4));
	*b = corridor_point(c, box->out_max, c->across_min + ((width * box->across_max) >> 4));
}

// A wall round the corridor: a roof far too tall to step onto, from (out, across) to (out, across)
#define CORRIDOR_WALL_HEIGHT 3000
static void add_wall(const corridor *c, walker_area *area, int out0, int across0, int out1, int across1)
{
	const world_point a = corridor_point(c, out0, across0);
	const world_point b = corridor_point(c, out1, across1);

	walker_area_add_roof(area, a.x, a.y, b.x, b.y, CORRIDOR_WALL_HEIGHT);
}

void corridor_walker_area(const corridor *c, walker_area *area)
{
	const world_point from = corridor_point(c, 0, c->across_min);
	const world_point to = corridor_point(c, CORRIDOR_LENGTH, c->across_max);

	walker_area_clear(area);
	walker_area_add(area, from.x, from.y, to.x, to.y);
	add_wall(c, area, -100, c->across_min - 100, CORRIDOR_LENGTH + 100, c->across_min); // a side
	add_wall(c, area, -100, c->across_max, CORRIDOR_LENGTH + 100, c->across_max + 100); // the other
	// The far end: its left half (the right's the way out, onto the city's entry roof)
	add_wall(c, area, CORRIDOR_LENGTH, c->across_min, CORRIDOR_LENGTH + 100, (c->across_min + c->across_max) >> 1);
	add_wall(c, area, -100, c->across_min, 0, c->across_max); // the hole
	for (int i = 0; i < CORRIDOR_BOXES; ++i)
	{
		world_point a, b;

		corridor_box_corners(c, &corridor_boxes[i], &a, &b);
		walker_area_add_roof(area, a.x, a.y, b.x, b.y, corridor_boxes[i].height);
	}
	city_walker_area_add(c, area); // and beyond the far end, the city's towers
}
