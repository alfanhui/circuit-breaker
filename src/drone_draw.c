#include <jo/jo.h>
#include "drone_draw.h"
#include "level.h"		   // level_beam_draw
#include "utils/perf.h"	   // the debug line's dm and ds
#include "object_fog_vdp1.h" // the fog, as the city has it (level_fog_capped)
#include "game/world.h"	   // WORLD_HEADING_DX, WORLD_HEADING_DY
#include "utils/fastmath.h" // ASR: shifts without libgcc's slow routines

/*
 * The watchdog's drone: a Tron light cycle that flies. Its look (docs/WORK_RAM.md, "The art pass",
 * taken early at the user's ask): bold, dark, few polygons, the light in its lines.
 *
 *              rotor             rotor          seen from above, its nose (the sensor) up the page:
 *              (   )-.  ^  .-(   )              a hull like a light cycle's, long and low, dark,
 *                     `/ \'                     glowing lines down its sides, and out to four
 *                      | |                      rotors; each rotor's a light cycle's wheel lying
 *                     ,| |.                     flat: a thick glowing tyre, a black disc, a grey
 *              (   )-'  v  `-(   )              hub, and the film's white dash that flashes
 *
 * Built once, in its own space: x to its right, y back (its nose at -y, facing north, as the
 * cycle's model: main.c), z up, in world units (a unit's ~2.3 cm), doubled (SIZE: 4.8 m nose to
 * tail, 7 m across the rotors). Only colours change from frame to frame: each rotor's dash flashes
 * (no point moves: as the CD block's disc), the sensor goes white while it sees you.
 *
 * Its cost (the debug line's row 7: dr its flying and looking, dm its model, ds its scan; % of a
 * frame on the master). First it was 110 points and 69 polygons, turned by jo's matrix: dm 12, and
 * ds as much again, where the city's already at its limit (51 fps on the roof it watches). So it
 * was lightened: the motors' boxes went (the rotors sit on the ends of its glowing arms), 66
 * points, 53 polygons, placed in the world here (a sine and cosine a frame, 4 multiplies a point).
 * Then the rotors became the film's wheels (disc, hub, dash: the user's ask): 98 points, 61.
 * Handing it and its scan to the slave SH-2 (slave_draw.h) made it worse out here: the master
 * waited ~17000 ticks a frame for the slave (mwait), down to 38 fps where it had been 60. The
 * slave's slow with the city's big near faces too (level.c: the towers are drawn by the master
 * out here for that), and why isn't known yet (docs/BACKLOG.md). So the master draws it.
 *
 * Faces that close a solid are drawn on one side (VDP1 skips those turned away): each is wound so
 * its outside's anticlockwise seen from outside (the cross product of its first two edges points
 * out), worked out once here from the part's middle, not by hand. The rotors are seen from above
 * and below: both sides.
 */

// Its colours. The hull near black (a light cycle's), its lines the watchdog's red-orange
#define HULL_TOP JO_COLOR_RGB(34, 38, 58)
#define HULL_SIDE JO_COLOR_RGB(20, 22, 36)
#define HULL_UNDER JO_COLOR_RGB(10, 10, 18)
#define GLOW JO_COLOR_RGB(255, 80, 30)
#define SENSOR JO_COLOR_RGB(255, 30, 30)
#define SENSOR_SEEN JO_COLOR_RGB(255, 255, 255)
#define TYRE JO_COLOR_RGB(240, 110, 20)	 // a rotor's tyre: a light cycle's wheel's glowing rim
#define DISC JO_COLOR_RGB(4, 4, 8)			 // inside the tyre, black (not 0: that's see-through)
#define HUB JO_COLOR_RGB(110, 116, 130)		 // the grey hub at its middle
#define DASH JO_COLOR_RGB(255, 255, 255)	 // the white dash, flashing in one place

// The beam (level_beam_draw): a pale cyan sheet with white edges; red once it has you
#define SCAN_SHEET JO_COLOR_RGB(80, 220, 255)
#define SCAN_EDGE JO_COLOR_RGB(220, 250, 255)
#define SCAN_SHEET_SEEN JO_COLOR_RGB(255, 40, 40)
#define SCAN_EDGE_SEEN JO_COLOR_RGB(255, 200, 200)

/*
 * Its size: every measure below is doubled as it's built (point). At first it was a car's size,
 * 3.5 m across the rotors, and from the entry roof, three towers off, a few pixels: a watchdog
 * that should loom. Doubled, 7 m across, it nearly fills the street it rides (200, 4.6 m: its long
 * way's along the street, its rotors over the roofs either side)
 */
#define SIZE 2
#define NOSE (DRONE_NOSE / SIZE) // the sensor: this far ahead of its middle (before SIZE)
#define ROTOR_INNER 25	// a rotor's tyre, inside and out: a thin glowing rim round a big black
#define ROTOR_OUTER 29	// disc (the user, 2026-11-14: the first, 18 to 28, read as all tyre)
#define ROTOR_HUB 5		// the hub's half width
#define DASH_FROM 9		// the dash, out from the middle: from here ...
#define DASH_TO 22		// ... to here, short of the tyre
#define DASH_WIDE 2		// and this either side: a strip
#define ROTOR_Z 10		// the rotors' height, over its middle
#define ROTOR_SEGMENTS 6 // a hexagon: 6 pieces of ring

/*
 * And a lighter model for far off (the user, 2026-11-14: 0650 to 0630 dropped to 30 fps, the drone
 * in view down row 6 the whole way: dm 11-13% of a frame). What costs is mostly the points: jo
 * turns and projects every point of a mesh in software, divisions and all, and a lighter far model
 * over 74 points was still 8-10%. Past DRONE_FAR a rotor's a few pixels across (58 units: 8 pixels
 * at 1200), its tyre's rim under one. Far, each rotor's a black square (one quad), its rim that
 * square's outline (a VDP1 polyline over the same 4 corners) and its dash a line (2 points): 6
 * points and 3 polygons, not 20 and 10. The hull and arms as near. 42 points (not 98), 33 polygons
 */
#define DRONE_FAR 1200
#define DRONE_SCAN_NEAR 2500 // further than this (along the floor), its scan line lights no roofs
#define POINTS (98 + 24) // the near model's, and the far rotors' squares and dash lines
#define POLYGONS 61
#define FAR_POLYGONS 33
static jo_pos3D_fixed points[POINTS]; // in the world, placed each frame (drone_draw)
static jo_3d_polygon polygons[POLYGONS + FAR_POLYGONS];
static jo_3d_polygon_attribute attributes[POLYGONS + FAR_POLYGONS];
static jo_3d_mesh model, far_model;
static int far_dash_polygon[4]; // the far model's dashes (whose colours flash), and its sensor
static int far_sensor_polygon;
#define FAR_POINTS 42 // the hull's 14, the arms' 4 hubs, and each rotor's square and dash (6)
static jo_pos3D_fixed far_points[FAR_POINTS];
static unsigned char far_map[FAR_POINTS]; // each far point's near point
static signed char far_index[POINTS];	   // and the other way (-1: not in the far model)
static int far_point_count;
static short where[POINTS][3]; // the points in its own space, whole units
static int point_count, polygon_count;

// The four rotors: right front, left front, right back, left back
static const signed char motors[4][2] = {{62, -34}, {-62, -34}, {62, 40}, {-62, 40}};
static int sensor_polygon, dash_polygon[4]; // the polygons whose colours change
static int side_corner[4];				 // the hull's side corners the arms start from (hull)

static int point(int x, int y, int z)
{
	x *= SIZE;
	y *= SIZE;
	z *= SIZE;
	where[point_count][0] = (short)x;
	where[point_count][1] = (short)y;
	where[point_count][2] = (short)z;
	return point_count++;
}

// A polygon of 4 corners (a triangle repeats its last), VDP1 command `kind` (JO_VDP1_POLYGON,
// JO_VDP1_LINE), flat colour. Returns its index
static int polygon(int p0, int p1, int p2, int p3, unsigned int kind, unsigned char plane, jo_color colour)
{
	jo_3d_polygon *q = &polygons[polygon_count];
	jo_3d_polygon_attribute *a = &attributes[polygon_count];

	q->normal.x = JO_FIXED_0; // unlit
	q->normal.y = JO_FIXED_0;
	q->normal.z = JO_FIXED_1;
	q->vertices[0] = (unsigned short)p0;
	q->vertices[1] = (unsigned short)p1;
	q->vertices[2] = (unsigned short)p2;
	q->vertices[3] = (unsigned short)p3;
	a->plane = plane;
	a->sort = JO_3D_SORT_CENTER | ((kind >> 16) & 0x1c) | JO_3D_NO_OPTION;
	a->texture_id = JO_3D_NO_TEXTURE;
	a->draw_mode = (unsigned short)((JO_VDP1_COLOR_MODE_RGB | JO_VDP1_NO_GOURAUD) | ((kind >> 24) & 0xc0));
	a->color = colour;
	a->gouraud_table = 0;
	a->direction = (unsigned short)(kind & 0x3f);
	return polygon_count++;
}

/*
 * A face of a solid, one side, wound to face out from (cx, cy, cz), a point inside the solid: if
 * the cross product of its first two edges points in, it's listed the other way round. Integers:
 * the corners are within 100 of each other, so the products are well inside 32 bits
 */
static int face(int p0, int p1, int p2, int p3, int cx, int cy, int cz, jo_color colour)
{
	const int ax = where[p1][0] - where[p0][0], ay = where[p1][1] - where[p0][1], az = where[p1][2] - where[p0][2];
	const int bx = where[p2][0] - where[p0][0], by = where[p2][1] - where[p0][1], bz = where[p2][2] - where[p0][2];
	const int nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
	// Out from the middle to the face's own middle (halfway across its diagonal: kept doubled)
	const int ox = where[p0][0] + where[p2][0] - 2 * cx, oy = where[p0][1] + where[p2][1] - 2 * cy,
			  oz = where[p0][2] + where[p2][2] - 2 * cz;

	if (nx * ox + ny * oy + nz * oz < 0)
		return polygon(p0, p3, p2, p1, JO_VDP1_POLYGON, JO_3D_SINGLE_PLANE, colour);
	return polygon(p0, p1, p2, p3, JO_VDP1_POLYGON, JO_3D_SINGLE_PLANE, colour);
}

/*
 * The hull: a light cycle's, long and low. Two rings of four corners (top, right, bottom, left),
 * the front one wide and high, the back one a little narrower; a point at the nose and one at the
 * tail. 4 faces round the nose, 4 between the rings, 4 round the tail; glowing lines along its two
 * side edges and up to the nose; the sensor, a red square across the nose's tip
 */
static void hull(void)
{
	const int nose = point(0, -NOSE, 0);
	const int front[4] = {point(0, -30, 16), point(22, -30, 0), point(0, -30, -12), point(-22, -30, 0)};
	const int back[4] = {point(0, 50, 12), point(18, 50, 0), point(0, 50, -10), point(-18, 50, 0)};
	const int tail = point(0, 66, 2);
	for (int k = 0; k < 4; ++k)
	{
		const int next = (k + 1) & 3;
		// The faces between corners k and next: the two upper ones (either side of the top) lighter
		const jo_color colour = k == 0 || k == 3 ? HULL_TOP : HULL_UNDER;

		face(nose, front[k], front[next], front[next], 0, 0, 2, colour);
		face(front[k], back[k], back[next], front[next], 0, 0, 2, colour);
		face(back[k], tail, back[next], back[next], 0, 0, 2, colour);
	}
	// The glowing lines: down each side edge, and from each side up to the nose
	polygon(front[1], back[1], back[1], front[1], JO_VDP1_LINE, JO_3D_DUAL_PLANE, GLOW);
	polygon(front[3], back[3], back[3], front[3], JO_VDP1_LINE, JO_3D_DUAL_PLANE, GLOW);
	polygon(nose, front[1], front[1], nose, JO_VDP1_LINE, JO_3D_DUAL_PLANE, GLOW);
	polygon(nose, front[3], front[3], nose, JO_VDP1_LINE, JO_3D_DUAL_PLANE, GLOW);
	// The arms start from the side corners: right front, left front, right back, left back
	side_corner[0] = front[1];
	side_corner[1] = front[3];
	side_corner[2] = back[1];
	side_corner[3] = back[3];
	// The sensor: a square just ahead of the nose, facing forwards (seen from in front only)
	{
		const int a = point(-6, -NOSE - 2, -5), b = point(6, -NOSE - 2, -5), c = point(6, -NOSE - 2, 4),
				  d = point(-6, -NOSE - 2, 4);

		sensor_polygon = face(a, b, c, d, 0, 0, 0, SENSOR);
	}
}

// An arm from the hull's side out to rotor m's middle: a glowing line (the blades' blur's over its
// end: mesh, it shows through)
static void arm(int m)
{
	const int hub = point(motors[m][0], motors[m][1], ROTOR_Z);

	polygon(side_corner[m], hub, hub, side_corner[m], JO_VDP1_LINE, JO_3D_DUAL_PLANE, GLOW);
}

/*
 * Rotor m: a light cycle's wheel lying on its side (the film's: Tron, 1982). A thick tyre in the
 * watchdog's orange, a hexagon ring of ROTOR_SEGMENTS pieces; inside it a black disc; at its
 * middle a grey hub; and a short white dash from the hub out towards the tyre. The dash doesn't
 * go round: it flashes on and off where it is (drone_draw), which is how the film shows a wheel
 * spinning too fast to follow. The dash points out and back, away from the hull (mirrored left
 * and right), as the film's points down and back.
 * The disc, hub and dash lie in one plane: VDP1 has no depth, and sorted by their middles they'd
 * swap about as the drone turns. So the hub and the dash are chained to the disc (JO_3D_SORT_
 * PREVIOUS: drawn straight after it, in the order added), always over it, from above or below.
 * Both sides: it's seen from below (the roofs under it) and from above (the lift, a tall roof).
 * 10 polygons, 20 points
 */
static void rotor(int m)
{
	// A hexagon's corners: cos and sin of 0, 60, 120 ... degrees, x1024
	static const short round[ROTOR_SEGMENTS][2] = {{1024, 0}, {512, 887}, {-512, 887}, {-1024, 0}, {-512, -887}, {512, -887}};
	const int x = motors[m][0], y = motors[m][1];
	const int side = x < 0 ? -1 : 1; // the dash points out this way, and back
	int inner[ROTOR_SEGMENTS], outer[ROTOR_SEGMENTS];

	for (int k = 0; k < ROTOR_SEGMENTS; ++k)
	{
		inner[k] = point(x + ((round[k][0] * ROTOR_INNER) >> 10), y + ((round[k][1] * ROTOR_INNER) >> 10), ROTOR_Z);
		outer[k] = point(x + ((round[k][0] * ROTOR_OUTER) >> 10), y + ((round[k][1] * ROTOR_OUTER) >> 10), ROTOR_Z);
	}
	for (int k = 0; k < ROTOR_SEGMENTS; ++k)
	{
		const int next = k + 1 < ROTOR_SEGMENTS ? k + 1 : 0;

		polygon(inner[k], outer[k], outer[next], inner[next], JO_VDP1_POLYGON, JO_3D_DUAL_PLANE, TYRE);
	}
	// The disc: the inside of the hexagon, two quads (corners 0-3, then 3-5 and back to 0)
	polygon(inner[0], inner[1], inner[2], inner[3], JO_VDP1_POLYGON, JO_3D_DUAL_PLANE, DISC);
	polygon(inner[3], inner[4], inner[5], inner[0], JO_VDP1_POLYGON, JO_3D_DUAL_PLANE, DISC);
	// The hub and the dash, chained after the disc
	{
		const int a = point(x - ROTOR_HUB, y, ROTOR_Z), b = point(x, y - ROTOR_HUB, ROTOR_Z), c = point(x + ROTOR_HUB, y, ROTOR_Z),
				  d = point(x, y + ROTOR_HUB, ROTOR_Z);
		/*
		 * Out and back at 45 degrees (0.7 of each), DASH_WIDE either side of that line: a strip, not
		 * a one-pixel line (the user, 2026-11-14: "the white strips need to be bolder"). Across it
		 * is 45 degrees the other way: (-0.7, 0.7) for a dash out to the right
		 */
		const int ax = (DASH_FROM * 724) >> 10, bx = (DASH_TO * 724) >> 10, w = (DASH_WIDE * 724) >> 10;
		const int p0 = point(x + side * (ax - w), y + ax + w, ROTOR_Z), p1 = point(x + side * (bx - w), y + bx + w, ROTOR_Z);
		const int p2 = point(x + side * (bx + w), y + bx - w, ROTOR_Z), p3 = point(x + side * (ax + w), y + ax - w, ROTOR_Z);
		int i = polygon(a, b, c, d, JO_VDP1_POLYGON, JO_3D_DUAL_PLANE, HUB);

		attributes[i].sort = (unsigned char)((attributes[i].sort & ~JO_3D_SORT_MASK) | JO_3D_SORT_PREVIOUS);
		i = polygon(p0, p1, p2, p3, JO_VDP1_POLYGON, JO_3D_DUAL_PLANE, DASH);
		attributes[i].sort = (unsigned char)((attributes[i].sort & ~JO_3D_SORT_MASK) | JO_3D_SORT_PREVIOUS);
		dash_polygon[m] = i;
	}
}

void drone_draw_init(void)
{
	point_count = polygon_count = 0;
	hull();
	for (int m = 0; m < 4; ++m)
	{
		arm(m);
		rotor(m);
	}
	model.data.points = points;
	model.data.polygons = polygons;
	model.data.attributes = attributes;
	model.data.point_count = (unsigned short)point_count;
	model.data.polygon_count = (unsigned short)polygon_count;

	/*
	 * The far model: its polygons after the near one's. The hull's (the first 17: its 12 faces, 4
	 * lines and the sensor) copied as they are; then each rotor's arm (copied), and its square, rim
	 * and dash, the rim and dash chained after the square (as the hub and dash are to the disc).
	 * Their points after the near model's (it doesn't use them). Then the far model's own points:
	 * only those its polygons use, each polygon's corners renumbered to them
	 */
	{
		const int near = polygon_count;
		int arm_polygon = 17; // the near model's: each rotor's arm, then its 10

		for (int i = 0; i < 17; ++i)
		{
			polygons[polygon_count] = polygons[i];
			attributes[polygon_count++] = attributes[i];
		}
		far_sensor_polygon = near + sensor_polygon; // (the sensor's among the 17: copied to the same place)
		for (int m = 0; m < 4; ++m, arm_polygon += 11)
		{
			// The square: corners at 45, 135 ... degrees, out at the tyre's outside (before SIZE, as
			// every measure). The dash: along its middle line (as rotor's), out and back
			const int x = motors[m][0], y = motors[m][1], r = (ROTOR_OUTER * 724) >> 10;
			const int side = x < 0 ? -1 : 1;
			const int from = (DASH_FROM * 724) >> 10, to = (DASH_TO * 724) >> 10;
			const int s0 = point(x + r, y + r, ROTOR_Z), s1 = point(x - r, y + r, ROTOR_Z), s2 = point(x - r, y - r, ROTOR_Z),
					  s3 = point(x + r, y - r, ROTOR_Z);
			const int d0 = point(x + side * from, y + from, ROTOR_Z), d1 = point(x + side * to, y + to, ROTOR_Z);
			int i;

			polygons[polygon_count] = polygons[arm_polygon];
			attributes[polygon_count++] = attributes[arm_polygon];
			polygon(s0, s1, s2, s3, JO_VDP1_POLYGON, JO_3D_DUAL_PLANE, DISC);
			i = polygon(s0, s1, s2, s3, JO_VDP1_POLYLINE, JO_3D_DUAL_PLANE, TYRE);
			attributes[i].sort = (unsigned char)((attributes[i].sort & ~JO_3D_SORT_MASK) | JO_3D_SORT_PREVIOUS);
			i = polygon(d0, d1, d1, d0, JO_VDP1_LINE, JO_3D_DUAL_PLANE, DASH);
			attributes[i].sort = (unsigned char)((attributes[i].sort & ~JO_3D_SORT_MASK) | JO_3D_SORT_PREVIOUS);
			far_dash_polygon[m] = i;
		}
		// Its own points: each near point a far polygon uses, numbered as first met
		for (int i = 0; i < POINTS; ++i)
			far_index[i] = -1;
		for (int i = near; i < polygon_count; ++i)
			for (int k = 0; k < 4; ++k)
			{
				const int p = polygons[i].vertices[k];

				if (far_index[p] < 0)
				{
					far_map[far_point_count] = (unsigned char)p;
					far_index[p] = (signed char)far_point_count++;
				}
				polygons[i].vertices[k] = (unsigned short)far_index[p];
			}
		far_model.data.points = far_points;
		far_model.data.polygons = &polygons[near];
		far_model.data.attributes = &attributes[near];
		far_model.data.point_count = (unsigned short)far_point_count;
		far_model.data.polygon_count = (unsigned short)(polygon_count - near);
	}
	model.data.point_count = (unsigned short)(point_count - 24); // not the far rotors' (the last 24)
}

/*
 * Before the city's drawn (main.c): the roofs the nearest drone's scan line lights (game/drone.h,
 * drone_scan_lit), handed to level.c, which colours each one's lit part straight after drawing
 * that roof (level_scan_set). Cyan, red once it has you. Nearest along the floor (level_reach)
 */
void drone_draw_prepare(const drone *drones, int count, const drone_box *boxes, int box_count)
{
	drone_box lit[DRONE_LIT_MAX];
	const int frc = jo_time_get_frc();
	const drone *nearest = &drones[0];
	int nearest_reach = level_reach(drones[0].x, drones[0].y);
	int lit_count;

	for (int i = 1; i < count; ++i)
	{
		const int reach = level_reach(drones[i].x, drones[i].y);

		if (reach < nearest_reach)
		{
			nearest = &drones[i];
			nearest_reach = reach;
		}
	}
	/*
	 * Only near: further off the lit line's a few pixels on a roof, and finding it walks the 256
	 * towers, and again for each lit part's line of sight (drone_scan_lit). At the SCSP, far from
	 * every patrol, it was still worked out every frame (2026-11-17, `ds`)
	 */
	lit_count = nearest_reach <= DRONE_SCAN_NEAR ? drone_scan_lit(nearest, boxes, box_count, lit) : 0;
	level_scan_set(lit, lit_count, nearest->seen ? SCAN_SHEET_SEEN : SCAN_SHEET);
	perf_part_add(PERF_DRONE_SCAN, frc);
}

void drone_draw(const drone *d, unsigned int frame)
{
	// The hover: up and down 3, round every 64 frames, a triangle wave (no sine)
	static const signed char hover[8] = {0, 1, 2, 3, 3, 2, 1, 0};
	const int z = d->z + hover[(frame >> 3) & 7] - 1;
	// The dashes flash: on for 2 frames, off (the disc's black) for 2, the rotors either side of
	// it the other way about (a real quad's turn opposite ways), so the four don't blink together
	for (int m = 0; m < 4; ++m)
		attributes[dash_polygon[m]].color = attributes[far_dash_polygon[m]].color =
			((frame >> 1) + (m ^ (m >> 1))) & 1 ? DASH : DISC;
	attributes[sensor_polygon].color = attributes[far_sensor_polygon].color = d->seen ? SENSOR_SEEN : SENSOR;

	/*
	 * Only if it could be seen: in the view, not behind a nearer, taller tower (level_box_seen). jo
	 * transforms every point of a mesh drawn, on screen or not: at 0650, facing the yard with the
	 * drone nowhere in view, it was still 9-14% of a frame (dm), the city already at 110% (2026-11-14)
	 */
	const frustum_point centre = {d->x, d->y, z}, half = {95 * SIZE, 95 * SIZE, 20 * SIZE};

	if (level_box_seen(centre, half))
	{
		const int model_frc = jo_time_get_frc();
		jo_fixed s, c;
		int sin1024, cos1024;

		/*
		 * Placed in the world: turned about z by -yaw (its yaw is heading h at h << 14, game/drone.h;
		 * the model faces north, and turns the other way round to face h, as the cycle's: main.c,
		 * RIBBON_QUARTER_TURN), then moved to where it is. One sine and cosine (jo's table), x1024
		 */
		// Far off, the lighter model (DRONE_FAR, along the floor), and only its points placed
		const bool far = level_reach(d->x, d->y) > DRONE_FAR;
		jo_3d_mesh *mesh = far ? &far_model : &model;

		jo_sincos_angle((jo_angle)(-(int)d->yaw), &s, &c);
		sin1024 = s >> 6;
		cos1024 = c >> 6;
		for (int j = 0; j < (int)mesh->data.point_count; ++j)
		{
			const int i = far ? far_map[j] : j;
			const int x = where[i][0], y = where[i][1];

			mesh->data.points[j].x = jo_int2fixed(d->x + (ASR(x * cos1024 - y * sin1024, 10)));
			mesh->data.points[j].y = jo_int2fixed(d->y + (ASR(x * sin1024 + y * cos1024, 10)));
			mesh->data.points[j].z = jo_int2fixed(z + where[i][2]);
		}
		/*
		 * Fogged as the city is where it flies (the user, 2026-11-18: up the lift "the watchdogs are
		 * very visible - but the city paths which they patrol isn't. The balance is not right"):
		 * one level for all of it, its middle's, capped as the far roofs are (level_fog_capped).
		 * Each polygon's Gouraud table set to it (object_fog_set_quad: none at level 0, as near).
		 * The attributes are shared by the three drones: set again for each, before it's drawn
		 */
		{
			const int level = level_fog_capped(d->x, d->y);

			if (object_fog_enabled)
				for (int k = 0; k < (int)mesh->data.polygon_count; ++k)
					object_fog_set_quad(&mesh->data.attributes[k], level, level, level, level);
		}
		// Drawn here, by the master: handed to the slave it was worse (see above)
		jo_3d_mesh_draw(mesh);
		perf_part_add(PERF_DRONE_MODEL, model_frc);
	}
	// Its beam: the square pyramid (game/drone.h), the sides facing you see-through, its edges solid
	{
		const int scan_frc = jo_time_get_frc();
		drone_beam beam;

		drone_beam_get(d, &beam);
		/*
		 * Only if it could be seen, as the model: the box round its five corners in the view and not
		 * behind a nearer, taller tower. Without this all three beams were classified, cut and sent
		 * every frame wherever they were: `ds` 26% of a frame at the SCSP, far from every patrol, and
		 * the benchmark's late frames 334 against 155 with no watchdogs (2026-11-17). Halves by
		 * shifts: no division
		 */
		int x_min = beam.apex.x, x_max = beam.apex.x, y_min = beam.apex.y, y_max = beam.apex.y;
		int z_min = beam.apex.z, z_max = beam.apex.z;

		for (int k = 0; k < 4; ++k)
		{
			const drone_point *p = &beam.base[k];

			x_min = p->x < x_min ? p->x : x_min;
			x_max = p->x > x_max ? p->x : x_max;
			y_min = p->y < y_min ? p->y : y_min;
			y_max = p->y > y_max ? p->y : y_max;
			z_min = p->z < z_min ? p->z : z_min;
			z_max = p->z > z_max ? p->z : z_max;
		}
		{
			const frustum_point beam_centre = {(x_min + x_max) >> 1, (y_min + y_max) >> 1, (z_min + z_max) >> 1};
			const frustum_point beam_half = {((x_max - x_min) >> 1) + 1, ((y_max - y_min) >> 1) + 1, ((z_max - z_min) >> 1) + 1};

			if (level_box_seen(beam_centre, beam_half))
				level_beam_draw(&beam, d->seen ? SCAN_SHEET_SEEN : SCAN_SHEET, d->seen ? SCAN_EDGE_SEEN : SCAN_EDGE);
		}
		perf_part_add(PERF_DRONE_SCAN, scan_frc);
	}
}
