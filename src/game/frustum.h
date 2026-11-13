#ifndef __FRUSTUM_H__
#define __FRUSTUM_H__

#include <stdbool.h>

/*
 * Cutting polygons in 3D to what the camera can see (docs/CULLING.md: "one clipper").
 *
 * Every 3D engine that looks stable does this one thing the same way for everything it draws:
 * each polygon is cut by a few planes, and only what's on the inside of all of them is drawn.
 *   - the near plane, a little in front of the eye: nothing behind or beside the camera reaches
 *     SGL (it projects such points onto the screen, mirrored: spikes)
 *   - the screen's four edges, as planes from the eye: nothing off the screen reaches VDP1 (it
 *     walks every off-screen pixel too, and wraps corners past 1023 pixels into spikes)
 *   - a portal's edges (a doorway between two rooms), as planes from the eye through each edge:
 *     what's in the next room is drawn only where it's seen through the opening
 * A plane cut is exact whatever the camera does (looking up, down, aside), so it needs none of the
 * special cases the floor-plan cuts had (wall_clip's cones, the caps, the bands looking down).
 *
 * Planes all pass at some distance from the eye: a point p is inside when
 *     normal . (p - eye) >= offset
 * with the normal 1024 long (x1024 fixed point) and offset the distance from the eye, x1024.
 * Sums: a point within 32000 of the eye x a normal's 1024, three times: ~10^8, inside 32 bits.
 *
 * Pure C, no SGL: unit-tested on the PC (tests/test_frustum.c). view.c builds the camera's
 * planes once a frame; level.c cuts each polygon. The cost is a few multiplies per corner and
 * plane, and one division only where an edge crosses a plane (the polygons at the screen's edges)
 */

#define FRUSTUM_ONE 1024	  // a normal's length
#define FRUSTUM_PLANES_MAX 12 // near, the screen's 4 edges, the guard band's 4, and a portal's 3
#define FRUSTUM_NEAR 0		  // the first plane is always the near one
/*
 * Two sets of edges. The screen's own (planes 1 .. 4) say what can be seen: wholly outside one,
 * a thing's not drawn. The guard band's (5 .. 8, FRUSTUM_GUARD_X16 sixteenths wider) are where
 * things are cut: VDP1 only needs its corners within its 1023 pixels, and walking a little past
 * the screen's edge costs it less than cutting costs the CPU. Cut at the screen's edges, nearly
 * every face touching one was cut, a division or more each: a fifth of the frame
 */
#define FRUSTUM_SCREEN 0x1F	   // the near plane and the screen's edges: planes 0 .. 4
#define FRUSTUM_GUARD 0x1E1	   // the near plane and the guard band's edges: planes 0, 5 .. 8
#define FRUSTUM_TOP_BOTTOM 0x18 // of the screen's edges, its top and bottom: planes 3 and 4
#define FRUSTUM_PORTAL_FIRST 9 // a portal's planes from here (frustum_add_edge)
#define FRUSTUM_GUARD_X16 32   // the guard band: twice the screen's reach (~320 pixels past its sides)
// A polygon gains at most one corner for each plane it's cut by
#define FRUSTUM_POLY_MAX (8 + FRUSTUM_PLANES_MAX)
// frustum_classify's answer when every corner is outside one plane: nothing of it can be seen
#define FRUSTUM_OUTSIDE 0x80000000u

typedef struct
{
	int x, y, z; // world units, +z up
} frustum_point;

typedef struct
{
	int x, y, z; // the normal, FRUSTUM_ONE long, pointing inside
	int offset;	 // inside from this distance from the eye, x FRUSTUM_ONE
} frustum_plane;

typedef struct
{
	frustum_point eye;
	int forward[3], right[3], up[3]; // the camera's directions, as given (x1024)
	int tan_side, tan_up;			 // the screen's edges, as given (frustum_slopes)
	int count;
	frustum_plane planes[FRUSTUM_PLANES_MAX];
} frustum;

/*
 * The camera's nine planes: near (near units in front of the eye), the screen's left, right,
 * top and bottom edges, and the guard band's. forward, right, up: the camera's directions, each FRUSTUM_ONE long (x1024).
 * tan_side, tan_up: how far aside and up the screen's edges are per unit in front, x1024 (on foot,
 * tan(37.5 degrees) and 0.75 of that: the window's 240 rows to its 320 columns). Pass them a little
 * wider than the screen: a cut exactly on its edge can leave a pixel's gap there from rounding
 */
void frustum_set_camera(frustum *f, frustum_point eye, const int forward[3], const int right[3], const int up[3],
						int tan_side, int tan_up, int near);

/*
 * One more plane, through the eye and the line from a to b (a portal's edge): its inside is the
 * side that point `inside` is on. Returns the plane's bit for the planes masks below, or 0 if no
 * room is left or the eye's on that line (no plane: nothing's cut by it). A division or three,
 * once a frame for each plane
 */
unsigned frustum_add_edge(frustum *f, frustum_point a, frustum_point b, frustum_point inside);

// How far point p is inside plane i (negative: outside), x FRUSTUM_ONE
int frustum_distance(const frustum *f, int i, frustum_point p);

/*
 * Which of the planes in `planes` (bit i: plane i) the n corners need cutting by: the planes some
 * corner's outside of. FRUSTUM_OUTSIDE if every corner's outside one of them (nothing of it can be
 * seen); 0 if every corner's inside all of them (draw it as it is, no cut). Corners of a convex
 * shape (a box's eight): if its corners are all inside, so is all of it
 */
unsigned frustum_classify(const frustum *f, const frustum_point *corners, int n, unsigned planes);

/*
 * The same for a box lined up with the world's axes (centre, and half its size along x, y and z; a
 * face is a box with no thickness one way), exactly as its eight corners would come out, for a
 * fraction of the sums. Each plane's nearest and furthest corner are the centre's distance less
 * and plus |normal x| x half x + |normal y| x half y + |normal z| x half z: one distance and three
 * multiplies a plane, not eight distances. Eight corners against eight planes was most of the
 * boxes' time
 */
unsigned frustum_classify_box(const frustum *f, frustum_point centre, frustum_point half, unsigned planes);

/*
 * Cuts a convex polygon (n corners in order round it, at most 8) by the planes in `planes`, in
 * plane order: Sutherland-Hodgman, the polygon cut by each in turn. Writes what's left, in the
 * same order round, to out (room for FRUSTUM_POLY_MAX) and returns its corners: 0 if less than a
 * triangle's left. Where an edge crosses a plane, the new corner's worked out from the edge's
 * inside end towards its outside one, whichever way round the polygon lists them: so two polygons
 * sharing an edge get the same new corner, and no crack opens between them
 */
int frustum_clip(const frustum *f, unsigned planes, const frustum_point *in, int n, frustum_point *out);

/*
 * The view in a mirror (level.c draws the corridor again, reflected, behind the glass you broke
 * through). A mirror standing across axis 0 (x) or 1 (y) at that coordinate: every point there,
 * p, is seen where its reflection is, p' (that axis: 2 x mirror - p). Drawing the world reflected is
 * the same as cutting the world as it is to the view of an eye reflected in the mirror, then
 * reflecting what's left: so the reflected view is this one with the eye, its directions and every
 * plane's normal flipped on that axis. The planes are measured from the eye, so their offsets stay.
 * A point p is inside out's planes exactly as far as its reflection is inside in's: no division
 */
void frustum_reflect(const frustum *in, int axis, int mirror, frustum *out);

/*
 * How steeply anything on the screen can be above or below the eye: the most a ray through the
 * screen climbs (up, 0 or more) and dives (down, 0 or less) per unit it goes along the floor,
 * x FRUSTUM_SLOPE_ONE. So a point D along the floor from the eye (or nearer) can only be seen
 * between eye.z + D x down and eye.z + D x up: level.c ends a far tower's sides there, with one
 * multiply, where cutting each by the screen's top and bottom in 3D took a division or two a
 * side. Taken at the screen's corners and its edges' middles (a level camera's steepest ray is its
 * top edge's middle), an eighth added for what's between them. FRUSTUM_STEEP (and its negative)
 * when the view reaches nearly straight up (or down): no limit that way.
 * Eight divisions and square roots, once a frame
 */
#define FRUSTUM_SLOPE_ONE 256
#define FRUSTUM_STEEP (16 * FRUSTUM_SLOPE_ONE)
void frustum_slopes(const frustum *f, int *up, int *down);

// Integer square root (rounded down): the planes' lengths
int frustum_isqrt(unsigned int v);

#endif
