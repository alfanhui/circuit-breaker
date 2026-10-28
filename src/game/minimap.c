#include "minimap.h"

#define BANDS (MINIMAP_RANGE >> MINIMAP_BAND_SHIFT) // 64

// half_widths[k]: how far the circle reaches along a line k bands from the player. Measured at
// the band's far edge, where the circle is narrowest, so a line never pokes out of it
static short half_widths[BANDS];

// Whole-number square root, by trying bits from the top. Start-up only. n is at most
// MINIMAP_RANGE^2 (2^22), so the root fits in 12 bits and (root + bit)^2 can't overflow
static int isqrt(int n)
{
	int root = 0;

	for (int bit = 1 << 12; bit > 0; bit >>= 1)
		if ((root + bit) * (root + bit) <= n)
			root += bit;
	return root;
}

void minimap_init(void)
{
	for (int k = 0; k < BANDS; ++k)
	{
		int c = (k + 1) << MINIMAP_BAND_SHIFT; // the band's far edge
		half_widths[k] = (short)isqrt(MINIMAP_RANGE * MINIMAP_RANGE - c * c);
	}
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
 * the same). >> 16 is >> 10 (back from x1024) and >> 6 (world units to pixels) at once, and
 * adding half of 1 << 16 first rounds to the nearest pixel.
 */
static void to_map(int dx, int dy, int forward_x, int forward_y, signed char *x, signed char *y)
{
	int forward = dx * forward_x + dy * forward_y;
	int right = dx * forward_y - dy * forward_x;

	*x = (signed char)((right + (1 << 15)) >> 16);
	*y = (signed char)((-forward + (1 << 15)) >> 16); // ahead is up the screen: -y
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
