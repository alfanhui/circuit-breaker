#include "minimap.h"

#define BANDS (MINIMAP_RANGE >> MINIMAP_BAND_SHIFT) // 64

// half_widths[k]: how far the circle reaches along a line k bands from the player. Measured at
// the band's far edge, where the circle is narrowest, so a line never pokes out of it
static short half_widths[BANDS];

// Whole-number square root, by trying bits from the top. Start-up only. n is at most
// MINIMAP_RANGE^2 (2^20 at a range of 1024), so the root fits in 12 bits and (root + bit)^2 can't overflow
static int isqrt(int n)
{
	int root = 0;

	for (int bit = 1 << 12; bit > 0; bit >>= 1)
		if ((root + bit) * (root + bit) <= n)
			root += bit;
	return root;
}

#define ROWS (2 * MINIMAP_EDGE + 1)

// The tilt, per map row (index: y + MINIMAP_EDGE, top to bottom): its screen y, and its width
// as a share of the near edge's, x1024
static signed char tilt_y[ROWS];
static short tilt_scale[ROWS];

void minimap_init(void)
{
	for (int k = 0; k < BANDS; ++k)
	{
		int c = (k + 1) << MINIMAP_BAND_SHIFT; // the band's far edge
		half_widths[k] = (short)isqrt(MINIMAP_RANGE * MINIMAP_RANGE - c * c);
	}
	// Each row's perspective (minimap.h). Depths are x1024. The near edge's depth over this row's
	// is the row's x scale (1024 at the near edge); its screen y is its height times that scale.
	// + half the divisor rounds to the nearest
	{
		const int near_depth = MINIMAP_TILT_DISTANCE * 1024 - MINIMAP_EDGE * MINIMAP_TILT_SIN;

		for (int r = 0; r < ROWS; ++r)
		{
			int y = r - MINIMAP_EDGE;
			int depth = MINIMAP_TILT_DISTANCE * 1024 - y * MINIMAP_TILT_SIN;
			int scale = (near_depth * 1024 + (depth >> 1)) / depth;
			int height = y * MINIMAP_TILT_COS; // x1024

			tilt_scale[r] = (short)scale;
			// height x1024 times scale x1024: >> 20, rounded either side of 0 alike
			tilt_y[r] = (signed char)(height < 0 ? -((-height * scale + (1 << 19)) >> 20)
												 : (height * scale + (1 << 19)) >> 20);
		}
	}
}

void minimap_tilt(int x, int y, int *screen_x, int *screen_y)
{
	int r = y + MINIMAP_EDGE;
	int width;

	if (r < 0)
		r = 0;
	if (r > ROWS - 1)
		r = ROWS - 1;
	*screen_y = tilt_y[r];
	// Narrowed by the row's share. Worked on the size and the sign put back after, so both
	// sides of the centre line round the same way and the map stays symmetrical
	width = ((x < 0 ? -x : x) * tilt_scale[r]) >> 10;
	*screen_x = x < 0 ? -width : width;
}

// sin of 0, 15, 30 .. 90 degrees, x1024: the rim's points are every 15 degrees, and the rest of
// the half-turn mirrors these (pure C here, so no SGL slSin)
static const short sin15[7] = {0, 265, 512, 724, 887, 989, 1024};

void minimap_gauge_point(int k, int radius, int *x, int *y)
{
	// Angle k * 15 degrees round from the bottom: x = r sin, y = r cos (+y is down: the bottom).
	// Past 90 degrees, sin mirrors and cos turns negative
	int s = sin15[k <= 6 ? k : 12 - k];
	int c = k <= 6 ? sin15[6 - k] : -sin15[k - 6];

	*x = (radius * s + 512) >> 10;
	*y = c < 0 ? -((radius * -c + 512) >> 10) : (radius * c + 512) >> 10;
}

int minimap_half_width(int across)
{
	if (across < 0)
		across = -across;
	if (across >= MINIMAP_RANGE)
		return 0;
	return half_widths[across >> MINIMAP_BAND_SHIFT];
}

/*
 * A point relative to the player, turned so forward is up, in pixels from the map's centre.
 * forward and right are the point's distances ahead and to the right, x1024 (wall_clip.c does
 * the same). One shift does >> 10 (back from x1024) and >> MINIMAP_SHIFT (world units to pixels), and
 * adding half a pixel's worth first rounds to the nearest pixel.
 */
static void to_map(int dx, int dy, int forward_x, int forward_y, signed char *x, signed char *y)
{
	int forward = dx * forward_x + dy * forward_y;
	int right = dx * forward_y - dy * forward_x;

	*x = (signed char)((right + (1 << (9 + MINIMAP_SHIFT))) >> (10 + MINIMAP_SHIFT));
	*y = (signed char)((-forward + (1 << (9 + MINIMAP_SHIFT))) >> (10 + MINIMAP_SHIFT)); // ahead is up the screen: -y
}

// Clips one N/S/E/W segment (relative to the player) to the circle and adds its line.
// Returns 1 if it added one
static int add_segment(int ax, int ay, int bx, int by, int forward_x, int forward_y, minimap_line *out)
{
	int half;

	if (ay == by) // runs east-west: clip x
	{
		half = minimap_half_width(ay);
		if (half == 0 || (ax < -half && bx < -half) || (ax > half && bx > half))
			return 0;
		ax = ax < -half ? -half : ax > half ? half : ax;
		bx = bx < -half ? -half : bx > half ? half : bx;
	}
	else if (ax == bx) // runs north-south: clip y
	{
		half = minimap_half_width(ax);
		if (half == 0 || (ay < -half && by < -half) || (ay > half && by > half))
			return 0;
		ay = ay < -half ? -half : ay > half ? half : ay;
		by = by < -half ? -half : by > half ? half : by;
	}
	else
		return 0; // diagonal: only the debug stress trail's head (X), not worth a division
	to_map(ax, ay, forward_x, forward_y, &out->x0, &out->y0);
	to_map(bx, by, forward_x, forward_y, &out->x1, &out->y1);
	return 1;
}

int minimap_build(const trail *const *trails, int trail_count, world_point eye, int forward_x, int forward_y,
				  minimap_line *out, int max_lines)
{
	int n = 0;
	world_point a;
	world_point b;

	for (int i = 0; i < trail_count; ++i)
	{
		int segments = trail_segment_count(trails[i]);

		for (int s = 0; s < segments && n < max_lines; ++s)
		{
			if (!trail_get_segment(trails[i], s, 0, &a, &b))
				continue;
			if (add_segment(a.x - eye.x, a.y - eye.y, b.x - eye.x, b.y - eye.y, forward_x, forward_y, &out[n]))
				out[n++].trail = (unsigned char)i;
		}
	}
	return n;
}
