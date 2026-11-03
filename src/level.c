#include "level.h"
#include "scene_memory.h"
#include "game/arena.h"
#include "game/surround.h"
#include "game/city.h"
#include "object_fog_vdp1.h"
#include "view.h"
#include "utils/perf.h"

// The wall textures (tools/make_wall.py) carry the floor's 125-unit grid up the wall with a
// glowing line where it meets the floor, so the wall reads as standing on the grid.
// Their rows are placed for these heights: change them together.
#define WALL_BOTTOM_Z (-8) // sinks a little below the floor (z = 0) to hide the seam
#define WALL_SPLIT_Z 250   // top of the low strip: the base glow and the first grid row
#define WALL_TOP_Z 1500	   // 12 grid rows; ~6x the old wall, for scale (the cycle is 50 tall)
#define WALL_BASE_TOP_Z 40 // iso cutaway walls: just the glowing base, lower than the cycle

// How far in front of the 1st person camera each strip's corners must be (see wall_clip.h).
// VDP1 corners must stay within 1023 pixels: 160 * (height above the 35 high eye) / distance.
#define LOW_MIN_FORWARD 40	 // 160 * (250 - 35) / 1023 = 34. Closer than the 96 a wall can get
#define HIGH_MIN_FORWARD 240 // 160 * (1500 - 35) / 1023 = 229. Closer, the strip is above the screen

/*
 * Each side is two strips of equal pieces, like the trail: column k is a bottom and a top point,
 * and piece j spans columns j and j + 1. The low strip (z -8..250) and the high strip (250..1500)
 * are separate quads so each can be clipped to its own safe distance from the camera: the tall
 * one has to stay further away. Only pieces inside the camera's view rectangle (view_cull.h)
 * are considered, so the points are rewritten every frame into small buffers whatever the
 * arena size, and resizing the arena needs no rebuild.
 *
 * Columns run left to right as seen from inside the arena, so every texture is upright
 * and unmirrored. SGL maps a texture's corners to Vertices[0..3] as
 * top-left, top-right, bottom-right, bottom-left.
 */

#define STRIP_PIECES (ARENA_WALL_COLUMNS_MAX - 1)
#define STRIP_POINTS (ARENA_WALL_COLUMNS_MAX * 2)

typedef struct
{
	jo_3d_mesh mesh;
	POINT points[STRIP_POINTS];
} wall_strip;

typedef struct
{
	bool along_x; // the side runs along the x axis (north and south walls)
	int fixed;	  // -1 or +1: which end of the other axis the wall stands on
	int dir;	  // -1 or +1: direction of the columns along the side
	wall_strip low;
	wall_strip high;
} wall_side;

// Piece j always uses columns j and j + 1 of whatever is in the buffer, so every strip shares
// one polygon table, and every low (or high) strip one attribute table: same texture throughout.
// Normals are unused: the walls are unlit.
static POLYGON piece_polygons[STRIP_PIECES];
static ATTR low_attributes[STRIP_PIECES];
static ATTR high_attributes[STRIP_PIECES];
static ATTR base_attributes[STRIP_PIECES];

// World north is -y and east is -x (see world.h). Facing a wall from inside, left to right is:
static wall_side sides[4] = {
	{.along_x = true, .fixed = -1, .dir = -1},	// north: west (+x) to east (-x)
	{.along_x = false, .fixed = -1, .dir = 1},	// east: north (-y) to south (+y)
	{.along_x = true, .fixed = 1, .dir = 1},	// south: east (-x) to west (+x)
	{.along_x = false, .fixed = 1, .dir = -1}, // west: south (+y) to north (-y)
};
// The flicker's side, after its section (draw_side): one mesh can't be filled twice in a frame
static wall_strip flicker_after_low;
static wall_strip flicker_after_high;

/*
 * The raised ground round the arena (game/surround.h): plain coloured quads, no texture, so
 * VDP1 just fills them (cheap, and a flat colour can't show the bending a texture would). Each
 * piece, cut to what's on the screen, is a convex polygon of up to 8 corners: a fan of up to 3
 * quads from its first corner, (0 1 2 3) (0 3 4 5) (0 5 6 7), a triangle as a quad with its last
 * corner twice. All of it is one mesh, rewritten and drawn once a frame.
 */
#define SURROUND_GROUND JO_COLOR_RGB(22, 18, 48)	 // darker than the floor's navy (26, 35, 68), a little violet
#define SURROUND_GLOW JO_COLOR_RGB(255, 110, 180) // the walls' top edge (tools/make_wall.py's GLOW)
#define SURROUND_QUADS_MAX (SURROUND_PIECES * 3)

static POINT surround_points[SURROUND_PIECES * SURROUND_CLIP_MAX];
static POLYGON surround_polygons[SURROUND_QUADS_MAX];
static ATTR surround_attributes[SURROUND_QUADS_MAX];
static jo_3d_mesh surround_mesh;

/*
 * The corridor's floor (game/corridor.h): its hexagons, each cut to what's on the screen, as
 * plain coloured fans like the surround's. Dark, matt glass: three shades of near black, no
 * texture, no shine; the floor fog fades them as it does everything. The arena's navy floor shows
 * through the seams between them as lines, as the grid's lines show on it. Room for every tile on the screen at once, most of them 2 quads (6 corners);
 * one that won't fit is left out (a gap far off, not a crash)
 */
#define CORRIDOR_POINTS_MAX 480
#define CORRIDOR_QUADS_MAX 192

static const jo_color glass_shades[CORRIDOR_SHADES] = {
	JO_COLOR_RGB(4, 4, 12),
	JO_COLOR_RGB(8, 7, 20),
	JO_COLOR_RGB(13, 11, 30),
};
// The arrays are in Work RAM's memory (corridor_memory, below)
static jo_3d_mesh corridor_mesh;

static int low_texture_id = 0;
static int high_texture_id = 0;
static int base_texture_id = 0;

void load_arena_textures(void)
{
	// 24-bit, 33 KB of VDP1 RAM between them (they replace the four debug BOX textures)
	low_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "WALL_LO.TGA", JO_COLOR_Transparent);
	high_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "WALL_HI.TGA", JO_COLOR_Transparent);
	base_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "WALLBASE.TGA", JO_COLOR_Transparent);
}

static void init_attributes(ATTR *attributes, int texture_id)
{
	jo_3d_mesh setter; // jo's texture setter works on a mesh: point one at the shared table

	setter.data.attbl = attributes;
	setter.data.nbPolygon = STRIP_PIECES;
	for (int j = 0; j < STRIP_PIECES; ++j)
	{
		// Same defaults as jo_3d_create_plane, so jo's texture setter behaves the same
		JO_ZERO(attributes[j].sort);
		JO_ZERO(attributes[j].atrb);
		JO_ZERO(attributes[j].gstb);
		attributes[j].flag = Dual_Plane;
		jo_3d_set_mesh_polygon_texture(&setter, texture_id, j);
		// Fog: VDP1 shades each corner by its own Gouraud colour (object_fog_vdp1.h)
		if (object_fog_enabled)
			attributes[j].atrb |= CL_Gouraud;
	}
}

static void init_strip(wall_strip *strip, ATTR *attributes)
{
	strip->mesh.data.pntbl = strip->points;
	strip->mesh.data.pltbl = piece_polygons;
	strip->mesh.data.attbl = attributes;
	strip->mesh.data.nbPoint = 0;
	strip->mesh.data.nbPolygon = 0;
}

void init_arena_walls(void)
{
	for (int j = 0; j < STRIP_PIECES; ++j)
	{
		piece_polygons[j].norm[X] = JO_FIXED_0;
		piece_polygons[j].norm[Y] = JO_FIXED_1;
		piece_polygons[j].norm[Z] = JO_FIXED_0;
		piece_polygons[j].Vertices[0] = j * 2 + 1; // left, top
		piece_polygons[j].Vertices[1] = j * 2 + 3; // right, top
		piece_polygons[j].Vertices[2] = j * 2 + 2; // right, bottom
		piece_polygons[j].Vertices[3] = j * 2;	   // left, bottom
	}
	init_attributes(low_attributes, low_texture_id);
	init_attributes(high_attributes, high_texture_id);
	init_attributes(base_attributes, base_texture_id);

	for (int s = 0; s < 4; ++s)
	{
		init_strip(&sides[s].low, low_attributes);
		init_strip(&sides[s].high, high_attributes);
	}
	init_strip(&flicker_after_low, low_attributes);
	init_strip(&flicker_after_high, high_attributes);

	// The surround: its corners and quads are filled in each frame (draw_arena_surround)
	for (int q = 0; q < SURROUND_QUADS_MAX; ++q)
	{
		surround_polygons[q].norm[X] = JO_FIXED_0; // unused: both faces are drawn, unlit
		surround_polygons[q].norm[Y] = JO_FIXED_0;
		surround_polygons[q].norm[Z] = JO_FIXED_1;
	}
	surround_mesh.data.pntbl = surround_points;
	surround_mesh.data.pltbl = surround_polygons;
	surround_mesh.data.attbl = surround_attributes;
	surround_mesh.data.nbPoint = 0;
	surround_mesh.data.nbPolygon = 0;
	// The corridor's floor's mesh is set up as its memory's taken (corridor_meshes_take)
}

// Works out a strip's corners for pieces [0, pieces) of columns, from z_bottom to z_top, into
// points (column k: its bottom, then its top) and each column's fog level. With a camera, the
// strip is first cut to the run of pieces that are in view and min_forward in front of it.
// Returns how many pieces are left (0: nothing to draw)
// Set while the corridor's walls are added (draw_corridor_walls): fill_strip cuts them to the cone
// in front as well as the min forward (wall_clip_strip_in_cone): no further aside than 1.5 x
// ahead, 240 pixels off the middle (the screen's edge is 160 off). The camera that's cut for
// faces where the head looks, so looking aside doesn't bring a cut into view
static bool corridor_cone = false;
#define CORRIDOR_CONE_REACH16 24

static int fill_strip(POINT *points, unsigned char *fog_levels, const world_point *columns, int pieces, int z_bottom,
					  int z_top, const wall_clip_camera *camera, int min_forward)
{
	int first = 0;
	int count = pieces;
	wall_clip_run run;

	if (camera != JO_NULL)
	{
		// The corridor's walls: you can stand beside them, so cut to the cone as well (wall_clip.h)
		run = corridor_cone ? wall_clip_strip_in_cone(columns, pieces, camera, min_forward, CORRIDOR_CONE_REACH16)
							: wall_clip_strip(columns, pieces, camera, min_forward);
		first = run.first;
		count = run.count;
	}
	if (count == 0)
		return 0;

	for (int i = 0; i <= count; ++i)
	{
		// The run's ends may have slid forward along the wall; the columns in between haven't
		world_point p = columns[first + i];
		if (camera != JO_NULL && i == 0)
			p = run.start;
		else if (camera != JO_NULL && i == count)
			p = run.end;

		points[i * 2][X] = jo_int2fixed(p.x);
		points[i * 2][Y] = jo_int2fixed(p.y);
		points[i * 2][Z] = jo_int2fixed(z_bottom);
		points[i * 2 + 1][X] = points[i * 2][X];
		points[i * 2 + 1][Y] = points[i * 2][Y];
		points[i * 2 + 1][Z] = jo_int2fixed(z_top);
		if (object_fog_enabled)
			fog_levels[i] = (unsigned char)object_fog_level(p.x, p.y);
	}
	return count;
}

// Draws pieces [0, pieces) of columns as one strip from z_bottom to z_top (fill_strip)
static void draw_strip(wall_strip *strip, const world_point *columns, int pieces, int z_bottom, int z_top,
					   const wall_clip_camera *camera, int min_forward)
{
	unsigned char fog_levels[ARENA_WALL_COLUMNS_MAX]; // per column: its top and bottom are equally far
	int count = fill_strip(strip->points, fog_levels, columns, pieces, z_bottom, z_top, camera, min_forward);

	if (count == 0)
		return;
	// Each piece's Gouraud table, its corners in Vertices order (init_arena_walls):
	// left top, right top, right bottom, left bottom. The attributes are shared by every strip of
	// this kind, but SGL reads them during jo_3d_mesh_draw below, before the next strip changes them
	if (object_fog_enabled)
		for (int j = 0; j < count; ++j)
			object_fog_set_quad(&strip->mesh.data.attbl[j], fog_levels[j], fog_levels[j + 1], fog_levels[j + 1],
								fog_levels[j]);
	strip->mesh.data.nbPoint = (count + 1) * 2;
	strip->mesh.data.nbPolygon = count;
	jo_3d_mesh_draw(&strip->mesh);
}

/*
 * The columns of one straight run of wall: an outer side, or one face of a block (arena.h).
 * along_x: the run lies along the x axis. wall: its line on the other axis. start, dir, pieces:
 * its first column, the way its columns run (left to right as seen from the side it faces, so
 * the texture's upright and unmirrored), and how many 250 pieces long it is. Only the pieces
 * inside view are kept, spaced by distance from the eye (250 apart nearby, longer far away).
 * Returns the number of pieces (columns - 1), 0 when none of it's in view
 */
static int run_columns(bool along_x, int wall, int start, int dir, int pieces, const world_rect *view,
					   const world_point *eye, world_point *columns)
{
	// The view rectangle on the run's own axes
	const int along_min = along_x ? view->x_min : view->y_min;
	const int along_max = along_x ? view->x_max : view->y_max;
	const int across_min = along_x ? view->y_min : view->x_min;
	const int across_max = along_x ? view->y_max : view->x_max;
	short along[ARENA_WALL_COLUMNS_MAX];
	int first;
	int count;

	if (wall < across_min || wall > across_max)
		return 0; // the wall's line isn't in view
	count = arena_visible_pieces_of(start, dir, pieces, along_min, along_max, &first);
	if (count == 0)
		return 0;
	count = arena_wall_columns(start, dir, first, count, along_x ? eye->x : eye->y, (along_x ? eye->y : eye->x) - wall,
							   along, ARENA_WALL_COLUMNS_MAX) - 1; // pieces = columns - 1
	for (int i = 0; i <= count; ++i)
	{
		columns[i].x = along_x ? along[i] : (short)wall;
		columns[i].y = along_x ? (short)wall : along[i];
	}
	return count;
}

/*
 * The blocks inside the arena (arena.h's layouts), as tall as its walls and textured the same,
 * and the flicker's pieces (below). Every face of every block, and every piece of the flicker,
 * goes into one mesh per band (the low strip, the high strip), filled and then drawn once: SGL
 * may still be reading a mesh after it's drawn, so one can't be refilled for the next face in the
 * same frame (SKILLS.md). Each face's pieces are appended with their own polygons (pointing at
 * their own columns) and attributes (copied from the band's texture).
 */
#define BLOCK_PIECES_MAX 112 // the rooms seen from inside one: ~60 pieces at full detail; the flicker, 10
// And each run's last column: a block's face, or one of the flicker's pieces (each a run of its own)
#define BLOCK_POINTS_MAX (2 * (BLOCK_PIECES_MAX + 4 * ARENA_BLOCKS_MAX + WALL_FLICKER_PIECES_MAX))

typedef struct
{
	jo_3d_mesh mesh;
	POINT points[BLOCK_POINTS_MAX];
	POLYGON polygons[BLOCK_PIECES_MAX];
	ATTR attributes[BLOCK_PIECES_MAX];
} block_band;

static block_band block_low;
static block_band block_high;

/*
 * Set while the corridor's walls are added (draw_corridor_walls): block_band_add lists each
 * piece's corners starting from a bottom one, so VDP1 fills it with upright lines, not lying ones.
 * VDP1 fills a quad with lines from its first-to-last edge (Vertices 0 to 3) across to its second
 * to third (1 to 2), one line for every pixel of the longer of those two edges. Listed top first
 * (as the arena's walls are, so their textures are upright), those edges are the piece's two
 * columns, and a piece beside you is very tapered: its near column 800 pixels tall, its far one
 * 50. That's 800 lines, each the piece's whole width, crammed into 50 pixels at the far end: the
 * same pixels filled over and over (49 fps looking at a wall). Bottom first, the edges are its
 * top and bottom, both about as long as the piece is wide: a line per pixel across, each just
 * its column's height, so each pixel's filled about once. The trails list theirs this way too.
 * Only for untextured pieces: a texture would lie on its side
 */
static bool upright_lines = false;

/*
 * Set while the boxes' sides are added (add_grey_box_sides): each piece's fog is one level, its
 * nearer column's, drawn as its colour darkened (depth_fog_darken), not a Gouraud table. A table
 * for each piece was written into VDP1 RAM while VDP1 drew: with the stairs far down the corridor
 * fogged, the sides were a seventh of the frame. A box's side is short, so one level's enough; the
 * corridor's long walls keep their tables (one level a 250 piece would step along them)
 */
static bool flat_fog = false;

static void block_band_begin(block_band *band)
{
	band->mesh.data.pntbl = band->points;
	band->mesh.data.pltbl = band->polygons;
	band->mesh.data.attbl = band->attributes;
	band->mesh.data.nbPoint = 0;
	band->mesh.data.nbPolygon = 0;
}

// One face's strip, appended to the band, textured as `texture` (one of the bands' attribute
// tables: low, high or base). Stops at a piece that wouldn't fit: a gap far off, not a crash
static void block_band_add(block_band *band, const ATTR *texture, const world_point *columns, int pieces, int z_bottom,
						   int z_top, const wall_clip_camera *camera, int min_forward)
{
	unsigned char fog_levels[ARENA_WALL_COLUMNS_MAX];
	const int base = band->mesh.data.nbPoint;
	int polygon = band->mesh.data.nbPolygon;
	int count;

	if (polygon + pieces > BLOCK_PIECES_MAX)
		pieces = BLOCK_PIECES_MAX - polygon;
	if (pieces <= 0)
		return;
	count = fill_strip(&band->points[base], fog_levels, columns, pieces, z_bottom, z_top, camera, min_forward);
	for (int j = 0; j < count; ++j, ++polygon)
	{
		POLYGON *q = &band->polygons[polygon];

		q->norm[X] = JO_FIXED_0; // unused: the walls are unlit
		q->norm[Y] = JO_FIXED_1;
		q->norm[Z] = JO_FIXED_0;
		band->attributes[polygon] = *texture;
		if (upright_lines)
		{
			// The same corners, the same way round, starting a corner earlier: left bottom, left
			// top, right top, right bottom (upright_lines, above)
			q->Vertices[0] = (Uint16)(base + j * 2);
			q->Vertices[1] = (Uint16)(base + j * 2 + 1);
			q->Vertices[2] = (Uint16)(base + j * 2 + 3);
			q->Vertices[3] = (Uint16)(base + j * 2 + 2);
			if (object_fog_enabled && flat_fog)
			{
				const int level = fog_levels[j] < fog_levels[j + 1] ? fog_levels[j] : fog_levels[j + 1];

				band->attributes[polygon].colno = object_fog_flat_colour(texture->colno, level);
			}
			else if (object_fog_enabled) // the fog's shades in the same corners' order
				object_fog_set_quad(&band->attributes[polygon], fog_levels[j], fog_levels[j], fog_levels[j + 1],
									fog_levels[j + 1]);
			continue;
		}
		// As init_arena_walls: left top, right top, right bottom, left bottom
		q->Vertices[0] = (Uint16)(base + j * 2 + 1);
		q->Vertices[1] = (Uint16)(base + j * 2 + 3);
		q->Vertices[2] = (Uint16)(base + j * 2 + 2);
		q->Vertices[3] = (Uint16)(base + j * 2);
		if (object_fog_enabled)
			object_fog_set_quad(&band->attributes[polygon], fog_levels[j], fog_levels[j + 1], fog_levels[j + 1],
								fog_levels[j]);
	}
	if (count > 0)
		band->mesh.data.nbPoint = (Uint16)(base + (count + 1) * 2);
	band->mesh.data.nbPolygon = (Uint16)polygon;
}

/*
 * Upright lines aren't always fewer, though. A box's front facing you is wide and short on the
 * screen (a stair's, near: ~480 pixels across, ~20 tall): upright, that's 480 lines of 20 pixels;
 * lying, 20 lines of 480. The same pixels, but VDP1 sets each line up whatever its length, and the
 * stairs' fronts upright took it to two frames (30 fps; lying, 60). So after the tops are capped
 * (cap_band_tops: the heights as drawn), each piece added since first_polygon is looked at again,
 * and turned back to lying lines (listed top first, as block_band_add does without upright_lines)
 * if all of these hold:
 *   - it's wider on the screen than its taller column (lying is fewer lines)
 *   - upright, it'd be more than LAY_FLAT_LINES lines. Small pieces laid flat made the frame
 *     slower, not quicker: everything far off laid flat took the corridor's start from 60 fps to
 *     54. (Why, isn't known: measured in Mednafen, check on the Saturn: HARDWARE_TESTS.md)
 *   - it's square on to you: its two columns within 5:4 of each other on the screen. A piece
 *     running away from you, laid flat, has lines slanting from its tall end to its short one,
 *     and VDP1 fills a slanting line's steps with extra pixels
 * Sizes on the screen, with no division: a column h tall at d in front is h x F / d pixels (F, the
 * focal length: 160, the screen's half width one unit in front), and the piece's ends, s0 and s1
 * aside, are |s1 / d1 - s0 / d0| x F apart. Times d0 x d1 / F, both: column 0's h0 x d1, column
 * 1's h1 x d0, the width |s1 x d0 - s0 x d1|, and LAY_FLAT_LINES (80, F / 2) is d0 x d1 / 2.
 * Only pieces within LAY_FLAT_REACH in front: further, none is that big, and the sums stay well
 * inside 32 bits (s is at most 1.5 x d, the cone: each product under 3 x 4096^2).
 * And only faces across your view: a face running along it (more than 60 degrees from square on:
 * the camera's forward along the face over half) has no piece square on to you, so its pieces
 * aren't looked at at all. Looked at piece by piece, the corridor's walls beside you (a hundred
 * pieces and more) cost ~6% of the frame for nothing. along_x: the face lies along x
 */
#define LAY_FLAT_LINES 80
#define LAY_FLAT_REACH 4096
static void lay_wide_pieces_flat(block_band *band, int first_polygon, bool along_x, const wall_clip_camera *camera)
{
	const int along = along_x ? camera->forward_x : camera->forward_y; // x1024

	if (along > 512 || along < -512)
		return;
	for (int p = first_polygon; p < (int)band->mesh.data.nbPolygon; ++p)
	{
		POLYGON *q = &band->polygons[p];
		// Upright order (block_band_add): left bottom, left top, right top, right bottom
		const int lb = q->Vertices[0], lt = q->Vertices[1], rt = q->Vertices[2], rb = q->Vertices[3];
		const int dx0 = jo_fixed2int(band->points[lb][X]) - camera->eye.x;
		const int dy0 = jo_fixed2int(band->points[lb][Y]) - camera->eye.y;
		const int dx1 = jo_fixed2int(band->points[rb][X]) - camera->eye.x;
		const int dy1 = jo_fixed2int(band->points[rb][Y]) - camera->eye.y;
		// How far in front of the camera each column is (d), and aside (s)
		const int d0 = (dx0 * camera->forward_x + dy0 * camera->forward_y) >> 10;
		const int d1 = (dx1 * camera->forward_x + dy1 * camera->forward_y) >> 10;
		int s0, s1, tall0, tall1, taller, shorter, wide;

		if (d0 <= 0 || d1 <= 0 || d0 > LAY_FLAT_REACH || d1 > LAY_FLAT_REACH)
			continue; // at the camera's spot (looking down), or far off: leave it upright
		// A quick test first: one square on to you has both ends about as far in front (its
		// columns' heights on the screen go as 1 / d)
		if ((d0 > d1 ? d0 : d1) * 4 > (d0 > d1 ? d1 : d0) * 5)
			continue;
		s0 = (dx0 * camera->forward_y - dy0 * camera->forward_x) >> 10;
		s1 = (dx1 * camera->forward_y - dy1 * camera->forward_x) >> 10;
		tall0 = (jo_fixed2int(band->points[lt][Z]) - jo_fixed2int(band->points[lb][Z])) * d1;
		tall1 = (jo_fixed2int(band->points[rt][Z]) - jo_fixed2int(band->points[rb][Z])) * d0;
		taller = tall0 > tall1 ? tall0 : tall1;
		shorter = tall0 > tall1 ? tall1 : tall0;
		wide = s1 * d0 - s0 * d1;
		if (wide < 0)
			wide = -wide;
		if (taller >= wide || wide * 2 <= d0 * d1 || taller * 4 > shorter * 5)
			continue; // taller than wide, too small to gain, or tapered: upright
		// Lying: left top, right top, right bottom, left bottom (the same way round)
		q->Vertices[0] = (Uint16)lt;
		q->Vertices[1] = (Uint16)rt;
		q->Vertices[2] = (Uint16)rb;
		q->Vertices[3] = (Uint16)lb;
		// The fog's shades follow the corners round (a piece fogged by its colour, flat_fog, or
		// with no fog at all, has no Gouraud to follow)
		if (object_fog_enabled && (band->attributes[p].atrb & CL_Gouraud))
		{
			const int left = object_fog_level(dx0 + camera->eye.x, dy0 + camera->eye.y);
			const int right = object_fog_level(dx1 + camera->eye.x, dy1 + camera->eye.y);

			object_fog_set_quad(&band->attributes[p], left, right, right, left);
		}
	}
}

static void block_band_draw(block_band *band)
{
	if (band->mesh.data.nbPolygon > 0)
		jo_3d_mesh_draw(&band->mesh);
}

// One run of a side: its pieces inside view (whose along range may be cut short of the whole
// view, either side of the flicker), drawn into the low and high strips given
static void draw_side_run(const wall_side *side, wall_strip *low, wall_strip *high, const world_rect *view,
						  const world_point *eye, const wall_clip_camera *camera, bool cutaway, int rise)
{
	const int half = arena_half();
	const int grid = arena_grid_half();
	world_point columns[ARENA_WALL_COLUMNS_MAX];
	// The wall's line on the other axis, and column 0 at the side's left end: on the grid from the
	// middle, at or past the corner (they're the same until the last round closes in)
	const int count = run_columns(side->along_x, side->fixed * half, -side->dir * grid, side->dir,
								  arena_grid_pieces(), view, eye, columns);

	if (count == 0)
		return;
	// Closing in, the corners are inside the grid's ends: the end pieces are cut back to them
	// (less than a piece each). The columns between stay where they are, so as the wall slides in
	// its texture slides with it, not along it
	if (grid != half)
		for (int i = 0; i <= count; ++i)
		{
			short *along = side->along_x ? &columns[i].x : &columns[i].y;

			if (*along > half)
				*along = (short)half;
			else if (*along < -half)
				*along = (short)-half;
		}
	if (cutaway)
	{
		// Just the glowing base, using the low strip's buffers with the base texture
		low->mesh.data.attbl = base_attributes;
		draw_strip(low, columns, count, WALL_BOTTOM_Z, WALL_BASE_TOP_Z, camera, LOW_MIN_FORWARD);
		low->mesh.data.attbl = low_attributes;
		return;
	}
	// Rising: both strips squashed to the same share of their height, so the whole wall grows up
	// out of the floor, its grid rows spreading apart as it does (VDP1 stretches each texture over
	// its quad). A multiply and a shift a side, not a vertex
	{
		int split = (WALL_SPLIT_Z * rise) >> 8;
		int top = (WALL_TOP_Z * rise) >> 8;

		draw_strip(low, columns, count, WALL_BOTTOM_Z, split, camera, LOW_MIN_FORWARD);
		draw_strip(high, columns, count, split, top, camera, HIGH_MIN_FORWARD);
	}
}

/*
 * The flicker (game/wall_flicker.h): a section of one side whose pieces glitch. That side is
 * drawn as three: the run before the section (its own strips), the run after it (the strips
 * below: one mesh can't be filled twice in a frame), and the section's pieces one by one,
 * appended to the blocks' bands (draw_blocks), so each can be left out (hidden: a hole, the
 * outside showing through) or drawn as a glow (flashed: the base texture, its glowing line,
 * stretched up the whole wall). Full detail at any distance: round 5's must be seen from afar
 */

// The along range of view on a side's axis
static void view_along(const wall_side *side, const world_rect *view, int *along_min, int *along_max)
{
	*along_min = side->along_x ? view->x_min : view->y_min;
	*along_max = side->along_x ? view->x_max : view->y_max;
}

// view, with its along range on side's axis cut to [along_min, along_max]
static world_rect view_cut(const wall_side *side, const world_rect *view, int along_min, int along_max)
{
	world_rect cut = *view;

	if (side->along_x)
	{
		cut.x_min = (short)along_min;
		cut.x_max = (short)along_max;
	}
	else
	{
		cut.y_min = (short)along_min;
		cut.y_max = (short)along_max;
	}
	return cut;
}

static void add_flicker_section(const wall_side *side, const wall_flicker *flicker, const world_rect *view,
								const wall_clip_camera *camera, bool cutaway, int rise)
{
	const int wall = side->fixed * arena_half();
	const int split = (WALL_SPLIT_Z * rise) >> 8;
	const int top = (WALL_TOP_Z * rise) >> 8;
	const int across_min = side->along_x ? view->y_min : view->x_min;
	const int across_max = side->along_x ? view->y_max : view->x_max;
	int along_min, along_max;

	if (wall < across_min || wall > across_max)
		return; // the wall's line isn't in view
	view_along(side, view, &along_min, &along_max);
	for (int i = 0; i < flicker->count; ++i)
	{
		const int a = flicker->along_min + i * WALL_FLICKER_PIECE;
		const int b = a + WALL_FLICKER_PIECE;
		world_point columns[2];

		if ((flicker->hidden >> i) & 1)
			continue; // dropped out this step
		if (b < along_min || a > along_max)
			continue;
		// Left to right as seen from inside the arena, as the rest of the side (dir)
		for (int k = 0; k < 2; ++k)
		{
			const int along = (side->dir > 0) == (k == 0) ? a : b;

			columns[k].x = (short)(side->along_x ? along : wall);
			columns[k].y = (short)(side->along_x ? wall : along);
		}
		if (cutaway)
			block_band_add(&block_low, &base_attributes[0], columns, 1, WALL_BOTTOM_Z, WALL_BASE_TOP_Z, camera,
						   LOW_MIN_FORWARD);
		else if ((flicker->flashed >> i) & 1)
			block_band_add(&block_low, &base_attributes[0], columns, 1, WALL_BOTTOM_Z, top, camera, HIGH_MIN_FORWARD);
		else
		{
			block_band_add(&block_low, &low_attributes[0], columns, 1, WALL_BOTTOM_Z, split, camera, LOW_MIN_FORWARD);
			block_band_add(&block_high, &high_attributes[0], columns, 1, split, top, camera, HIGH_MIN_FORWARD);
		}
	}
}

// A side, with the flicker's section in it if it's on this side
static void draw_side(wall_side *side, int s, const world_rect *view, const world_point *eye,
					  const wall_clip_camera *camera, bool cutaway, int rise, const wall_flicker *flicker)
{
	int along_min, along_max;
	int section_min, section_max;

	if (flicker == JO_NULL || flicker->count == 0 || flicker->side != s)
	{
		draw_side_run(side, &side->low, &side->high, view, eye, camera, cutaway, rise);
		return;
	}
	// Either side of the section: 1 short of its ends, so the piece each end falls on (a piece
	// is picked by where a point lies: arena_visible_pieces) stays the section's
	view_along(side, view, &along_min, &along_max);
	section_min = flicker->along_min;
	section_max = wall_flicker_along_max(flicker);
	if (along_min <= section_min - 1)
	{
		const world_rect before = view_cut(side, view, along_min, along_max < section_min - 1 ? along_max : section_min - 1);

		draw_side_run(side, &side->low, &side->high, &before, eye, camera, cutaway, rise);
	}
	if (along_max >= section_max + 1)
	{
		const world_rect after = view_cut(side, view, along_min > section_max + 1 ? along_min : section_max + 1, along_max);

		draw_side_run(side, &flicker_after_low, &flicker_after_high, &after, eye, camera, cutaway, rise);
	}
	add_flicker_section(side, flicker, view, camera, cutaway, rise);
}

// One face of a block: its strips added to the bands (as draw_side draws an outer side)
static void add_block_face(bool along_x, int wall, int start, int dir, int length, const world_rect *view,
						   const world_point *eye, const wall_clip_camera *camera, bool cutaway, int rise)
{
	world_point columns[ARENA_WALL_COLUMNS_MAX];
	// A face is whole 250 pieces (arena.c lays them out so). A division, but once per face, a
	// handful a frame, as arena_visible_pieces does once per side
	const int count = run_columns(along_x, wall, start, dir, length / (2 * ARENA_PIECE_HALF), view, eye, columns);

	if (count == 0)
		return;
	if (cutaway)
	{
		block_band_add(&block_low, &base_attributes[0], columns, count, WALL_BOTTOM_Z, WALL_BASE_TOP_Z, camera,
					   LOW_MIN_FORWARD);
		return;
	}
	{
		int split = (WALL_SPLIT_Z * rise) >> 8;
		int top = (WALL_TOP_Z * rise) >> 8;

		block_band_add(&block_low, &low_attributes[0], columns, count, WALL_BOTTOM_Z, split, camera,
					   LOW_MIN_FORWARD);
		block_band_add(&block_high, &high_attributes[0], columns, count, split, top, camera, HIGH_MIN_FORWARD);
	}
}

/*
 * Every block's faces. In 1st person (a camera) only the faces turned towards the eye: the
 * others are hidden behind the block, so they'd be drawn for nothing (at most 2 of 4 a block).
 * In the iso and debug cameras the eye isn't the camera, so all four are drawn. Iso (cutaway)
 * draws them all as the glowing base, as it does the near outer walls: full height they'd hide
 * the cycles round them. Each face's columns run left to right as seen from outside the block,
 * as an outer side's are seen from inside the arena (north is -y, east -x: world.h)
 */
static void draw_blocks(const world_rect *view, const world_point *eye, const wall_clip_camera *camera, bool cutaway,
						int rise)
{
	int count;
	const world_rect *blocks = arena_blocks(&count);
	const bool cull = camera != JO_NULL;

	for (int i = 0; i < count; ++i)
	{
		const world_rect *b = &blocks[i];
		const int width = b->x_max - b->x_min;
		const int depth = b->y_max - b->y_min;

		// North face (y_min), seen from the north: east (-x) on the left, as the south side's
		if (!cull || eye->y < b->y_min)
			add_block_face(true, b->y_min, b->x_min, 1, width, view, eye, camera, cutaway, rise);
		// South face (y_max), seen from the south: west (+x) on the left, as the north side's
		if (!cull || eye->y > b->y_max)
			add_block_face(true, b->y_max, b->x_max, -1, width, view, eye, camera, cutaway, rise);
		// East face (x_min), seen from the east: south (+y) on the left, as the west side's
		if (!cull || eye->x < b->x_min)
			add_block_face(false, b->x_min, b->y_max, -1, depth, view, eye, camera, cutaway, rise);
		// West face (x_max), seen from the west: north (-y) on the left, as the east side's
		if (!cull || eye->x > b->x_max)
			add_block_face(false, b->x_max, b->y_min, 1, depth, view, eye, camera, cutaway, rise);
	}
}

/*
 * The corridor's walls (game/corridor.h): the inner faces of its two sides and its far end.
 * Only the inner faces: the outer ones are never seen (from the arena, the arena's wall hides
 * them). Not the arena's walls: no grid, no pink. Near-black glass, a little red, standing on a
 * base that glows blood red and pulses slowly, like a warning light, or something breathing.
 * Untextured (a flat colour each), so VDP1 just fills them.
 * A third of the arena's height (CORRIDOR_WALL_RISE of 256). Full height, the two walls a few
 * hundred units either side of the camera filled most of the screen, and with the glass floor as
 * well VDP1 needed two vblanks a frame (30 fps lying in it). Low, the sky's open above you: it's
 * 60, and it reads as outside the arena
 */
#define CORRIDOR_WALL_RISE 85
/*
 * On foot (game/walker.h) you can walk right up to these walls: 40 from them. The arena's strips
 * start well in front of the camera (HIGH_MIN_FORWARD, 240) instead, as a tall piece nearer than
 * that reaches past VDP1's 1023 pixels above the screen, and VDP1 steps through every off-screen
 * pixel of it too (it halved the frame rate). Here, nothing that near was drawn: walking up to a
 * wall, it went missing, and the floor outside showed through.
 * So each column's top is cut off where it's sure to be above the screen: at most its distance
 * in front above the eye (a 45 degree slope up from the eye: the screen's top edge is 35 degrees
 * up), or twice that while looking up (63 degrees: up to 25 more, on foot). That's 160 pixels
 * above the middle at any distance (320 looking up), so a corner can't wrap however near, and the
 * walls can start just CORRIDOR_MIN_FORWARD in front (the base's foot, 43 under the eye, is then
 * 690 pixels down). A near piece's top slopes, off the screen: nobody sees the cut. Twice the
 * distance all the time cost a wall beside you 15 fps
 */
#define CORRIDOR_MIN_FORWARD 10
#define CORRIDOR_BODY JO_COLOR_RGB(20, 4, 10) // near black, a little red
#define CORRIDOR_PULSE_SHIFT 7				  // the base's pulse: 128 frames up, 128 down (4 s round)

/*
 * Looking down (on foot), a strip wholly below the eye is on the screen further aside than the
 * level cone reaches: in front of the camera's tilted view, a point `below` under the eye is
 * below x sin(pitch) further forward than it is level. So the cone's that much wider all along
 * (x16: below x sin x1024 >> 6). Nothing below the eye can be behind the camera looking down, so
 * no corner wraps. Cut to the level cone, the walls and boxes beside you went missing looking
 * down from up high, the white floor through the gap. `top`: the strip's top. 0 looking level
 * or up, or if any of the strip's above the eye
 */
static int corridor_cone_below(int top)
{
	const int pitch = view_look_pitch(); // + down
	const int below = -view_z - top;	 // view_z moves the world: the eye's height is -view_z

	if (pitch <= 0 || below <= 0)
		return 0;
	return (below * (slSin((ANGLE)pitch) >> 6)) >> 6;
}

/*
 * And how near in front it can start: looking down, nearer than CORRIDOR_MIN_FORWARD, as its
 * nearest point is still well in front of the tilted camera. That's its top: in front of the
 * camera by forward x cos(pitch) + below x sin(pitch). It must be far enough in front that its
 * deepest point (`deepest` under the eye) is within VDP1's 1023 pixels of the middle: deepest x
 * 160 / 1000 (the screen's half width, 160, is one unit in front at 90 degrees). A division, once
 * per strip: (that - below x sin) / cos. Never nearer than 1: a strip behind the camera's spot
 * isn't cut to the view at all (wall_clip.c works on the floor, level)
 */
static int corridor_near_below(int top, int bottom)
{
	const int pitch = view_look_pitch();
	const int eye = -view_z;
	const int below = eye - top;
	const int deepest = eye - bottom;
	int near, cos_p;

	if (pitch <= 0 || below <= 0)
		return CORRIDOR_MIN_FORWARD;
	cos_p = slCos((ANGLE)pitch) >> 6;
	near = ((((deepest * 164) >> 10) + 8) * 1024 - below * (slSin((ANGLE)pitch) >> 6)) / (cos_p > 64 ? cos_p : 64);
	return near < 1 ? 1 : near > CORRIDOR_MIN_FORWARD ? CORRIDOR_MIN_FORWARD : near;
}

// Where the corridor's wall body is split looking down: this far under the eye (corridor_cone_below
// for the part below it). Split at the eye itself, the part below gained nothing: its top was
// level with the eye, as near as anything above it
#define CORRIDOR_SPLIT_BELOW 64
#define CORRIDOR_SPLIT_NEAR 16

/*
 * The camera for a strip from bottom to top, wholly below the eye: looking down from above it, its
 * view reaches further aside (corridor_cone_below), as far as its bottom's seen aside, the deepest
 * point. Sized from its top, the strip's lower part ran out of width at the screen's corners: you
 * saw the white floor past a wall's foot. Its top's nearer the eye's height, so out at that width
 * it's further off the screen: no more than 6 x its depth under the eye, which keeps it within
 * ~1000 pixels (VDP1's reach). Writes to `low`, returns it, or the camera as it is
 */
static const wall_clip_camera *corridor_strip_camera(const wall_clip_camera *camera, int top, int bottom,
													 wall_clip_camera *low)
{
	const int eye = -view_z;
	const int top_below = eye - top;
	int deepest = eye - bottom;

	if (camera == JO_NULL || top_below <= 0)
		return camera;
	if (deepest > top_below * 6)
		deepest = top_below * 6;
	*low = *camera;
	low->side_extra16 = corridor_cone_below(eye - deepest);
	return low;
}

// A flat coloured wall quad's attribute, sorted by its centre as the walls are
static void set_corridor_wall_attribute(ATTR *a, jo_color colour)
{
	a->flag = Dual_Plane;
	a->sort = SORT_CEN | ((sprPolygon >> 16) & 0x1c) | No_Option;
	a->texno = No_Texture;
	a->atrb = (CL32KRGB | No_Gouraud) | ((sprPolygon >> 24) & 0xc0);
	a->colno = colour;
	a->gstb = 0;
	a->dir = sprPolygon & 0x3f;
}

// The base's glow this frame: from a deep blood red up to crimson and back, a triangle wave over
// the frames counted (frame), in steps of a shift: no division
static jo_color corridor_pulse(unsigned int frame)
{
	const int phase = (int)(frame & ((2u << CORRIDOR_PULSE_SHIFT) - 1)); // 0..255
	const int up = phase < (1 << CORRIDOR_PULSE_SHIFT) ? phase : (2 << CORRIDOR_PULSE_SHIFT) - 1 - phase; // 0..127

	return JO_COLOR_RGB(110 + up, (up * 30) >> CORRIDOR_PULSE_SHIFT, 20 + ((up * 30) >> CORRIDOR_PULSE_SHIFT));
}

// Cuts the tops of the points block_band_add has just added (from `first` on: bottom, top, bottom,
// top ...) to each column's distance in front of the camera above the eye (twice it looking up).
// Looking down, the screen's top edge comes down towards level (it's ~35 degrees above the
// middle): then only tan(35 - pitch) x the distance, and at least 4 over the eye. Left at the
// level reach looking down from up high, the wall beside you was drawn far above the screen's
// top, and VDP1 walked every pixel of it (30 fps). Never below the column's bottom
#define CAP_TOP_EDGE DEGtoANG(35.0)
static void cap_band_tops(block_band *band, int first, const wall_clip_camera *camera)
{
	const int eye_height = -view_z; // view_z moves the world: the eye's height is -view_z
	const int pitch = view_look_pitch(); // + down
	// The reach above the eye, per unit in front, x16
	const int reach16 = pitch < 0 ? 32 : pitch == 0 ? 16 : pitch >= CAP_TOP_EDGE ? 0 : (slTan((ANGLE)(CAP_TOP_EDGE - pitch)) >> 12) + 1;

	for (int i = first + 1; i < band->mesh.data.nbPoint; i += 2)
	{
		const int dx = jo_fixed2int(band->points[i][X]) - camera->eye.x;
		const int dy = jo_fixed2int(band->points[i][Y]) - camera->eye.y;
		const int forward = (dx * camera->forward_x + dy * camera->forward_y) >> 10;
		const int bottom = jo_fixed2int(band->points[i - 1][Z]);
		int highest = eye_height + 4 + ((reach16 * (forward > 0 ? forward : 0)) >> 4);

		if (highest <= bottom)
			highest = bottom + 1;
		if (jo_fixed2int(band->points[i][Z]) > highest)
			band->points[i][Z] = jo_int2fixed(highest);
	}
}

/*
 * And the bottoms: each column's bottom raised to just below the screen's bottom edge. That edge
 * is a plane through the eye, CAP_BOTTOM_EDGE below the camera's middle: tan(37 + pitch) x the
 * distance in front below the eye (pitch + down), and 4 more. 37 degrees, not 35: the picture may
 * be 240 rows, not 224 (jo says 240; Mednafen shows 224), and 120 / 160 is tan 37; on foot the
 * view's 75 degrees across, narrower, so the edge is nearer level still. Standing on the low roof
 * facing the tower 40 off, its front went 150 pixels below the screen (to the roof under you, 68
 * below the eye) and VDP1 walked them all: 30 fps. Looking down past 33 degrees the edge is
 * steeper than 70, and nothing's cut. Never above the column's top. Only for untextured pieces
 * (a texture would be squashed, not cut), and called after each block_band_add, for its strip.
 * A column's raised only if the ones either side of it have their bottoms at the plane or under
 * it. VDP1 draws a straight edge between two columns: from one raised to the plane to one at or
 * under it, the edge stays on or under the plane, off the screen. To one above it (its bottom on
 * the screen, left where it was), the edge cuts across the screen: a wedge of the wall went
 * missing, the floor showing through. A box's side gets a column where the plane crosses its
 * bottom (grey_side_crossing), so its pieces past that can be cut
 */
#define CAP_BOTTOM_EDGE DEGtoANG(37.0)

// The plane's slope below the eye, per unit in front, x16 (CAP_BOTTOM_EDGE + the pitch); -1 when
// it's too steep to cut anything. Looking up far enough, the edge is above level: nothing under
// the eye is seen, but a column's kept from 4 under it up (0)
static int bottom_cap_reach16(void)
{
	const int edge = CAP_BOTTOM_EDGE + view_look_pitch(); // + down

	if (edge >= DEGtoANG(70.0))
		return -1;
	return edge <= 0 ? 0 : (slTan((ANGLE)edge) >> 12) + 1;
}

// The plane's height `forward` in front of the camera
static int bottom_cap_at(int forward, int reach16)
{
	return -view_z - 4 - ((reach16 * (forward > 0 ? forward : 0)) >> 4); // -view_z: the eye's height
}

// The plane's height at column `point` (a band's point: its top's the next), but never up to its
// top: a column wholly below the plane keeps a sliver (VDP1 still has a quad to draw)
static int band_bottom_cap(const block_band *band, int point, const wall_clip_camera *camera, int reach16)
{
	const int dx = jo_fixed2int(band->points[point][X]) - camera->eye.x;
	const int dy = jo_fixed2int(band->points[point][Y]) - camera->eye.y;
	const int lowest = bottom_cap_at((dx * camera->forward_x + dy * camera->forward_y) >> 10, reach16);
	const int top = jo_fixed2int(band->points[point + 1][Z]);

	return lowest >= top ? top - 1 : lowest;
}

static void cap_band_bottoms(block_band *band, int first, const wall_clip_camera *camera)
{
	const int reach16 = bottom_cap_reach16();
	const int end = band->mesh.data.nbPoint;
	// Whether a column's bottom is at the plane or under it: then an edge to it from a column
	// raised to the plane stays on or below the plane. Past the strip's ends: nothing to keep to
	int lowest, next = 0, before_ok = 1;

	if (reach16 < 0 || first + 1 >= end)
		return;
	lowest = band_bottom_cap(band, first, camera, reach16);
	for (int i = first; i + 1 < end; i += 2)
	{
		const int bottom = jo_fixed2int(band->points[i][Z]);
		const int ok = lowest >= bottom;
		int after_ok = 1;

		if (i + 3 < end)
		{
			next = band_bottom_cap(band, i + 2, camera, reach16);
			after_ok = next >= jo_fixed2int(band->points[i + 2][Z]);
		}
		if (lowest > bottom && before_ok && after_ok)
			band->points[i][Z] = jo_int2fixed(lowest);
		before_ok = ok; // as it was built, not as it's now
		lowest = next;
	}
}

/*
 * Where along a box's side the plane meets the side's bottom: a column there lets the pieces past
 * it be cut (cap_band_bottoms), when the side's one piece reaches from where its bottom's on the
 * screen to where it isn't (the tower's front, 40 off, turned a little: 30 fps uncut). The plane's
 * height falls with the distance in front, and that's a straight line along the side: two
 * divisions, once a side (and only for a side the plane crosses). Returns the point along it, or
 * `from` if there's none inside
 */
static int grey_side_crossing(bool along_x, int line, int from, int to, int bottom, const wall_clip_camera *camera)
{
	const int reach16 = bottom_cap_reach16();
	const int ex = camera->eye.x, ey = camera->eye.y;
	const int f_from = (((along_x ? from : line) - ex) * camera->forward_x +
						((along_x ? line : from) - ey) * camera->forward_y) >> 10;
	const int f_to = (((along_x ? to : line) - ex) * camera->forward_x + ((along_x ? line : to) - ey) * camera->forward_y) >>
					 10;
	int crossing; // how far in front the plane's at the side's bottom

	if (reach16 <= 0 || -view_z - 4 <= bottom || f_from == f_to)
		return from; // the plane's level, or above the bottom right under the eye: no crossing
	crossing = ((-view_z - 4 - bottom) << 4) / reach16;
	if ((crossing <= f_from) == (crossing <= f_to))
		return from; // both ends the same side of it
	return from + ((to - from) * (crossing - f_from)) / (f_to - f_from);
}

/*
 * The grey box (corridor.h): plain boxes, flat grey. Tops light, sides darker, the two ways the
 * sides face in two shades, so a box's edges and corners read against each other in the dark
 */
#define GREY_TOP JO_COLOR_RGB(150, 150, 164)
#define GREY_SIDE_X JO_COLOR_RGB(96, 96, 110)  // sides facing along x
#define GREY_SIDE_Y JO_COLOR_RGB(70, 70, 84)   // and along y
// Every other box's sides a little lighter: stairs seen from below show only their fronts (their
// tops are above the eye), and one grey for them all read as one flat wall
#define GREY_SIDE_X_ALT JO_COLOR_RGB(116, 116, 130)
#define GREY_SIDE_Y_ALT JO_COLOR_RGB(88, 88, 102)
#define GREY_FACE_PIECE 500 // a side longer than this is cut into 2 pieces (4 past twice it)

/*
 * The boxes as the world has them, worked out once when the corridor's open (and again only if
 * it moves): each one's rectangle, its middle and a radius round it (half its width plus half its
 * depth: at least half its diagonal, with no square root), and how far down each of its sides is
 * seen (grey_side_bottom). Worked out every frame for 24 boxes, it was hundreds of conversions a
 * frame and the CPU ran over (30 fps).
 * The city's towers (city.h) are boxes too, after the corridor's: the same drawing, but standing
 * on the ground far below (CITY_GROUND), not on the corridor's floor. Each box keeps its height
 * and the height it stands on (floor)
 */
#define GREY_BOXES (CORRIDOR_BOXES + CITY_TOWERS)
#define GREY_X_MIN 0 // a box's sides, in grey_box.bottom
#define GREY_X_MAX 1
#define GREY_Y_MIN 2
#define GREY_Y_MAX 3
typedef struct
{
	world_rect rect;
	world_point centre;
	short radius;
	short bottom[4];
	short height, floor;
} grey_box;
static world_rect grey_corridor_floor; // the corridor's floor, for grey_side_bottom
static int grey_boxes_for = -1; // the corridor they're for: its wall and side (-1: none yet)

/*
 * The corridor's meshes and the grey box's, in Work RAM's memory (scene_memory.h): taken as the
 * breach starts, given back when the arena starts again. NULL meanwhile: nothing's drawn
 */
#define GREY_TOP_POINTS_MAX 200 // the corridor's boxes and the city's towers
#define GREY_TOP_QUADS_MAX 72
typedef struct
{
	POINT points[CORRIDOR_POINTS_MAX]; // the glass floor's
	POLYGON polygons[CORRIDOR_QUADS_MAX];
	ATTR attributes[CORRIDOR_QUADS_MAX];
	POINT top_points[GREY_TOP_POINTS_MAX]; // the grey box's tops (draw_corridor_boxes)
	POLYGON top_polygons[GREY_TOP_QUADS_MAX];
	ATTR top_attributes[GREY_TOP_QUADS_MAX];
	grey_box boxes[GREY_BOXES];
} corridor_memory;
static corridor_memory *corridor_meshes = JO_NULL;
// The arrays by their names, as when they were static
#define corridor_points (corridor_meshes->points)
#define corridor_polygons (corridor_meshes->polygons)
#define corridor_attributes (corridor_meshes->attributes)
#define grey_top_points (corridor_meshes->top_points)
#define grey_top_polygons (corridor_meshes->top_polygons)
#define grey_top_attributes (corridor_meshes->top_attributes)
#define grey_boxes (corridor_meshes->boxes)

void corridor_meshes_take(void)
{
	if (corridor_meshes != JO_NULL)
		return;
	corridor_meshes = (corridor_memory *)jo_malloc(sizeof(corridor_memory));
	if (corridor_meshes == JO_NULL)
	{
		jo_core_error("No memory for the corridor");
		return;
	}
	jo_memset(corridor_meshes, 0, sizeof(corridor_memory));
	grey_boxes_for = -1; // worked out again, into the new memory
	// The corridor's floor, as the surround's (init_arena_walls)
	for (int q = 0; q < CORRIDOR_QUADS_MAX; ++q)
	{
		corridor_polygons[q].norm[X] = JO_FIXED_0;
		corridor_polygons[q].norm[Y] = JO_FIXED_0;
		corridor_polygons[q].norm[Z] = JO_FIXED_1;
	}
	corridor_mesh.data.pntbl = corridor_points;
	corridor_mesh.data.pltbl = corridor_polygons;
	corridor_mesh.data.attbl = corridor_attributes;
	corridor_mesh.data.nbPoint = 0;
	corridor_mesh.data.nbPolygon = 0;
}

void corridor_meshes_release(void)
{
	if (corridor_meshes == JO_NULL)
		return;
	jo_free(corridor_meshes);
	corridor_meshes = JO_NULL;
}

// Whether rectangle r has a side against the line (along_x ? y = line : x = line) on its outside
// `out` (-1 or 1), all along from `from` to `to`
static bool grey_against(const world_rect *r, bool along_x, int line, int from, int to, int out)
{
	const int against = along_x ? (out > 0 ? r->y_min : r->y_max) : (out > 0 ? r->x_min : r->x_max);
	const int lo = along_x ? r->x_min : r->y_min, hi = along_x ? r->x_max : r->y_max;

	return against == line && lo <= from && hi >= to;
}

// How far down a box's side is seen: to what it stands on, or to the top of a box against it all
// along (the stairs: each step's front shows only above the step before; a box as high or higher
// hides it all). The corridor's floor counts as one 0 high: the city's entry stands against its
// end, and the entry's side facing you, all of it below the floor, isn't drawn. The side's on the
// line along_x ? y = line : x = line, from `from` to `to` along it, its outside towards `out`
static int grey_side_bottom(int box, bool along_x, int line, int from, int to, int out)
{
	int bottom = grey_boxes[box].floor;

	if (bottom < 0 && grey_against(&grey_corridor_floor, along_x, line, from, to, out))
		bottom = 0;
	for (int j = 0; j < GREY_BOXES; ++j)
		if (j != box && grey_boxes[j].height > bottom && grey_against(&grey_boxes[j].rect, along_x, line, from, to, out))
			bottom = grey_boxes[j].height;
	return bottom;
}

static void grey_boxes_work_out(const corridor *passage)
{
	const int key = passage->wall * 4 + passage->side;

	if (key == grey_boxes_for)
		return;
	grey_boxes_for = key;
	{
		const world_point a = corridor_point(passage, 0, passage->across_min);
		const world_point b = corridor_point(passage, CORRIDOR_LENGTH, passage->across_max);

		grey_corridor_floor.x_min = JO_MIN(a.x, b.x);
		grey_corridor_floor.x_max = JO_MAX(a.x, b.x);
		grey_corridor_floor.y_min = JO_MIN(a.y, b.y);
		grey_corridor_floor.y_max = JO_MAX(a.y, b.y);
	}
	for (int i = 0; i < GREY_BOXES; ++i)
	{
		world_point a, b;
		world_rect *r = &grey_boxes[i].rect;

		if (i < CORRIDOR_BOXES)
		{
			corridor_box_corners(passage, &corridor_boxes[i], &a, &b);
			grey_boxes[i].height = corridor_boxes[i].height;
			grey_boxes[i].floor = 0;
		}
		else
		{
			city_tower_corners(passage, &city_towers[i - CORRIDOR_BOXES], &a, &b);
			grey_boxes[i].height = city_towers[i - CORRIDOR_BOXES].height;
			grey_boxes[i].floor = CITY_GROUND;
		}
		r->x_min = JO_MIN(a.x, b.x);
		r->x_max = JO_MAX(a.x, b.x);
		r->y_min = JO_MIN(a.y, b.y);
		r->y_max = JO_MAX(a.y, b.y);
		grey_boxes[i].centre.x = (short)((r->x_min + r->x_max) >> 1);
		grey_boxes[i].centre.y = (short)((r->y_min + r->y_max) >> 1);
		grey_boxes[i].radius = (short)((r->x_max - r->x_min + r->y_max - r->y_min) >> 1);
	}
	for (int i = 0; i < GREY_BOXES; ++i)
	{
		const world_rect *r = &grey_boxes[i].rect;

		grey_boxes[i].bottom[GREY_X_MIN] = (short)grey_side_bottom(i, false, r->x_min, r->y_min, r->y_max, -1);
		grey_boxes[i].bottom[GREY_X_MAX] = (short)grey_side_bottom(i, false, r->x_max, r->y_min, r->y_max, 1);
		grey_boxes[i].bottom[GREY_Y_MIN] = (short)grey_side_bottom(i, true, r->y_min, r->x_min, r->x_max, -1);
		grey_boxes[i].bottom[GREY_Y_MAX] = (short)grey_side_bottom(i, true, r->y_max, r->x_min, r->x_max, 1);
	}
}

/*
 * The corridor's open end (city.h): the right half of its far end, a doorway onto Work RAM's city.
 * From inside, the city's only seen through it, and from the city, the corridor's insides only
 * through it: its walls are drawn behind everything (sorted by their furthest corner), so a tower
 * beyond them would be drawn over them, and the stairs over their outsides. So each frame:
 *   - inside the corridor, the towers are cut to the wedge the doorway makes from the eye, on the
 *     floor plan: from the eye through its two edges, out past it. A tower wholly outside it isn't
 *     drawn; a side partly in it is cut where it crosses the wedge's edges, and a top to the wedge
 *   - out in the city, the corridor's floor and its boxes aren't drawn at all (for now: seen
 *     through the doorway from outside, its insides are missing)
 *   - in the doorway itself (GREY_DOORWAY either side of it), both are drawn, uncut: the wedge
 *     there is too wide to mean anything
 * The sums: which side of a line through the eye a point is, two products. In ints: everything's
 * within a few thousand of the eye, so they're within ~10^8
 */
#define GREY_DOORWAY 40
typedef struct
{
	bool inside, outside;  // the eye in the corridor (the city cut to the doorway), or out in the city
	int eye_out, eye_height; // inside: how far out the eye is, and how high
	world_point eye, edge[2]; // the doorway's two edges, on the floor plan
	int sign[2];		   // which side of each edge's line (from the eye) is in the wedge
	world_point quad[4];   // the wedge as a quad, for corridor_clip: the doorway and far out past it
} grey_portal;
static grey_portal portal;

// Which side of the line a -> b point p is: twice the triangle's signed area
static int grey_side_of(world_point a, world_point b, world_point p)
{
	return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

// How far into the wedge a point is past edge e's line: negative outside
static int grey_portal_in(int e, world_point p)
{
	const int side = grey_side_of(portal.eye, portal.edge[e], p);

	return portal.sign[e] > 0 ? side : -side;
}

// Works out this frame's doorway from the eye. No camera (the iso view): everything drawn, uncut
static void grey_portal_set(const corridor *passage, const wall_clip_camera *camera)
{
	int out;

	portal.inside = portal.outside = false;
	if (camera == JO_NULL)
		return;
	out = corridor_out(passage, camera->eye);
	portal.outside = out > CORRIDOR_LENGTH + GREY_DOORWAY;
	portal.inside = out < CORRIDOR_LENGTH - GREY_DOORWAY;
	if (!portal.inside)
		return;
	portal.eye = camera->eye;
	portal.eye_out = out;
	portal.eye_height = -view_z;
	portal.edge[0] = corridor_point(passage, CORRIDOR_LENGTH, (passage->across_min + passage->across_max) >> 1);
	portal.edge[1] = corridor_point(passage, CORRIDOR_LENGTH, passage->across_max);
	portal.sign[0] = grey_side_of(portal.eye, portal.edge[0], portal.edge[1]) > 0 ? 1 : -1;
	portal.sign[1] = grey_side_of(portal.eye, portal.edge[1], portal.edge[0]) > 0 ? 1 : -1;
	// Far out: each edge on along its line from the eye, 6000 past it (the city's slice ends ~2100
	// past the doorway). Its x and y step in proportion: a division, once a frame for each edge
	for (int e = 0; e < 2; ++e)
	{
		const world_point p = portal.edge[e];
		const int dx = p.x - portal.eye.x, dy = p.y - portal.eye.y;
		const int far = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);

		portal.quad[e] = p;
		portal.quad[3 - e].x = (short)(p.x + (dx * 6000) / far);
		portal.quad[3 - e].y = (short)(p.y + (dy * 6000) / far);
	}
}

// Whether any of a rectangle can be seen through the doorway: not if all its corners are outside
// one of the wedge's edges
static bool grey_portal_sees(const world_rect *r)
{
	const world_point corners[4] = {{r->x_min, r->y_min}, {r->x_max, r->y_min}, {r->x_max, r->y_max},
									{r->x_min, r->y_max}};

	for (int e = 0; e < 2; ++e)
	{
		int k = 0;

		while (k < 4 && grey_portal_in(e, corners[k]) < 0)
			++k;
		if (k == 4)
			return false;
	}
	return true;
}

// Whether all of a rectangle's in the wedge: then it needs no cutting to it
static bool grey_portal_holds(const world_rect *r)
{
	const world_point corners[4] = {{r->x_min, r->y_min}, {r->x_max, r->y_min}, {r->x_max, r->y_max},
									{r->x_min, r->y_max}};

	for (int k = 0; k < 4; ++k)
		if (grey_portal_in(0, corners[k]) < 0 || grey_portal_in(1, corners[k]) < 0)
			return false;
	return true;
}

// Cuts a box's side (on the line along_x ? y = line : x = line, from `from` to `to` along it) to
// the wedge: false if none of it's in it. Where it crosses an edge's line: a division, once a
// crossing. The side's sums are brought down 256 first, so the length x one stays in 32 bits (in
// 64, the SH-2's division is a library call of hundreds of cycles)
static bool grey_portal_cut(bool along_x, int line, int *from, int *to)
{
	for (int e = 0; e < 2; ++e)
	{
		const world_point a = {(short)(along_x ? *from : line), (short)(along_x ? line : *from)};
		const world_point b = {(short)(along_x ? *to : line), (short)(along_x ? line : *to)};
		const int in_a = grey_portal_in(e, a), in_b = grey_portal_in(e, b);
		int crossing;

		if (in_a < 0 && in_b < 0)
			return false;
		if (in_a >= 0 && in_b >= 0)
			continue;
		{
			const int a8 = in_a >> 8, span8 = (in_a - in_b) >> 8;

			crossing = *from + (span8 == 0 ? 0 : ((*to - *from) * a8) / span8);
		}
		if (in_a < 0)
			*from = crossing;
		else
			*to = crossing;
	}
	return *to > *from;
}

/*
 * Whether a tower's under the corridor floor's far edge, from inside: the city below 0 is seen
 * through the doorway only above that edge. Its top's highest on the screen at its near side
 * (out_min), and it's below the edge if it's further down per unit along than the edge is: (eye -
 * top) / (out_min - eye's out) > eye / (the doorway - eye's out). Both are along the corridor, and
 * any ray from the eye crosses the doorway's line and the tower's near side in the same ratio, so
 * it holds whichever way you look. Multiplied out: no division. From far down the corridor that's
 * every tower below 0, and only the guard pages are left to draw
 */
static bool grey_under_the_floor(const city_tower *t)
{
	const int along = t->out_min - portal.eye_out, to_doorway = CORRIDOR_LENGTH - portal.eye_out;

	return t->height < 0 && (portal.eye_height - t->height) * to_doorway > portal.eye_height * along;
}

// Whether box i is drawn this frame at all: the corridor's not from out in the city, a tower not
// from inside the corridor unless it's seen through the doorway, above the floor's far edge
static bool grey_box_drawn(int i)
{
	if (i < CORRIDOR_BOXES)
		return !portal.outside;
	return !portal.inside ||
		   (!grey_under_the_floor(&city_towers[i - CORRIDOR_BOXES]) && grey_portal_sees(&grey_boxes[i].rect));
}

// One side of a box, added to the band: the columns along it, a piece every GREY_FACE_PIECE or
// so (halves and quarters: shifts, not a division)
// whole: the box is wholly well in front and in view (grey_box_whole): its side is added as it is,
// with none of the cuts, caps or checks below
static void add_grey_side(int box, int which, bool along_x, int line, int from, int to,
						  const wall_clip_camera *camera, bool whole)
{
	world_point columns[6];
	int alongs[6];
	ATTR side;
	wall_clip_camera low;
	int length, pieces, shift;
	const int bottom = grey_boxes[box].bottom[which];
	const int top = grey_boxes[box].height;
	const int first = block_low.mesh.data.nbPoint;
	const int first_polygon = block_low.mesh.data.nbPolygon;

	if (bottom >= top)
		return; // hidden all the way up
	if (!whole && box >= CORRIDOR_BOXES && portal.inside && !grey_portal_cut(along_x, line, &from, &to))
		return; // a tower's side, none of it seen through the doorway
	length = to - from;
	pieces = length > 2 * GREY_FACE_PIECE ? 4 : length > GREY_FACE_PIECE ? 2 : 1;
	shift = pieces == 4 ? 2 : pieces == 2 ? 1 : 0;

	for (int k = 0; k <= pieces; ++k)
		alongs[k] = from + ((length * k) >> shift);
	if (camera != JO_NULL && !whole)
	{
		// One more column where the screen's bottom edge meets the side's bottom, in its place
		const int cut = grey_side_crossing(along_x, line, from, to, bottom, camera);
		int k = pieces;

		if (cut > from && cut < to)
		{
			for (; alongs[k] > cut; --k)
				alongs[k + 1] = alongs[k];
			if (alongs[k] == cut)
				for (++k; k <= pieces; ++k) // already a column there: put them back
					alongs[k] = alongs[k + 1];
			else
			{
				alongs[k + 1] = cut;
				++pieces;
			}
		}
	}
	for (int k = 0; k <= pieces; ++k)
	{
		columns[k].x = along_x ? (short)alongs[k] : (short)line;
		columns[k].y = along_x ? (short)line : (short)alongs[k];
	}
	if (box & 1)
		set_corridor_wall_attribute(&side, along_x ? GREY_SIDE_Y_ALT : GREY_SIDE_X_ALT);
	else
		set_corridor_wall_attribute(&side, along_x ? GREY_SIDE_Y : GREY_SIDE_X);
	if (whole)
	{
		block_band_add(&block_low, &side, columns, pieces, bottom, top, JO_NULL, 0); // no camera: uncut
		return;
	}
	block_band_add(&block_low, &side, columns, pieces, bottom, top, corridor_strip_camera(camera, top, bottom, &low),
				   corridor_near_below(top, bottom));
	if (camera != JO_NULL)
	{
		cap_band_tops(&block_low, first, camera);
		cap_band_bottoms(&block_low, first, camera);
		lay_wide_pieces_flat(&block_low, first_polygon, along_x, camera); // a front facing you: lying lines
	}
}

static int grey_top_place(const wall_clip_camera *camera, const grey_box *box, int height); // below

/*
 * Whether box i's sides can go to VDP1 as they are, uncut (place: grey_top_place's, 1 when it's
 * wholly GLASS_MIN_FORWARD or more in front and in the view's reach). Then none of a side's
 * corners is behind the camera or near it, and none is far off the screen sideways (at most ~370
 * pixels off the middle). Up and down it's checked: its top and its feet (a city tower's, 3000
 * down) no more than 4 x as far from the eye's height as the box is in front, ~830 pixels, inside
 * VDP1's 1023. And not a tower cut to the doorway. The stairs' fronts down the corridor, each a
 * few pixels, were each cut to the view, capped and checked: the box sides were a quarter of the
 * frame
 */
static bool grey_box_whole(int i, int place, const wall_clip_camera *camera)
{
	const grey_box *b = &grey_boxes[i];
	const int forward = (((b->centre.x - camera->eye.x) * camera->forward_x +
						  (b->centre.y - camera->eye.y) * camera->forward_y) >> 10) - b->radius;
	const int eye = -view_z;

	if (place != 1 || (i >= CORRIDOR_BOXES && portal.inside && !grey_portal_holds(&b->rect)))
		return false;
	return eye - b->floor <= 4 * forward && b->height - eye <= 4 * forward;
}

// The boxes' sides: only those facing the eye (a box's other sides are behind it). Added as the
// corridor's walls are: cut to the cone in front, upright lines
static void add_grey_box_sides(const corridor *passage, const world_point *eye, const wall_clip_camera *camera)
{
	if (corridor_meshes == JO_NULL)
		return;
	grey_boxes_work_out(passage);
	grey_portal_set(passage, camera);
	corridor_cone = true;
	upright_lines = true;
	flat_fog = true;
	for (int i = 0; i < GREY_BOXES; ++i)
	{
		const world_rect *r = &grey_boxes[i].rect;

		const int place = camera != JO_NULL ? grey_top_place(camera, &grey_boxes[i], grey_boxes[i].floor) : 2;
		bool whole;

		// Not seen from here (grey_box_drawn), or out of the view: none of its sides worked out
		if (!grey_box_drawn(i) || place == 0)
			continue;
		whole = camera != JO_NULL && grey_box_whole(i, place, camera);
		if (eye->x < r->x_min)
			add_grey_side(i, GREY_X_MIN, false, r->x_min, r->y_min, r->y_max, camera, whole);
		if (eye->x > r->x_max)
			add_grey_side(i, GREY_X_MAX, false, r->x_max, r->y_min, r->y_max, camera, whole);
		if (eye->y < r->y_min)
			add_grey_side(i, GREY_Y_MIN, true, r->y_min, r->x_min, r->x_max, camera, whole);
		if (eye->y > r->y_max)
			add_grey_side(i, GREY_Y_MAX, true, r->y_max, r->x_min, r->x_max, camera, whole);
	}
	corridor_cone = false;
	upright_lines = false;
	flat_fog = false;
}

/*
 * The broken glass over the hole you came through (tools/make_glass.py): looking back from the
 * corridor, the breach is a pane of dark glass, shattered from where you hit it. A mirror would be
 * better, but the Saturn has no reflections: it would be the corridor drawn twice. Four textures
 * side by side, one a 250 piece, so the cracks run on across the joins; a narrower hole (3
 * pieces) uses the middle ones. Drawn as a wall: the same cuts near you
 */
#define GLASS_PIECES 4
static ATTR glass_attributes[GLASS_PIECES];
static bool glass_loaded = false;

void load_corridor_glass(void)
{
	static const char *const names[GLASS_PIECES] = {"GLASS0.TGA", "GLASS1.TGA", "GLASS2.TGA", "GLASS3.TGA"};
	jo_3d_mesh setter; // jo's texture setter works on a mesh: pointed at one attribute at a time

	if (glass_loaded)
		return;
	setter.data.nbPolygon = 1;
	for (int k = 0; k < GLASS_PIECES; ++k)
	{
		const int id = jo_sprite_add_tga(JO_ROOT_DIR, names[k], JO_COLOR_Transparent);

		setter.data.attbl = &glass_attributes[k];
		// As init_attributes: jo_3d_create_plane's defaults, so jo's setter behaves the same
		JO_ZERO(glass_attributes[k].sort);
		JO_ZERO(glass_attributes[k].atrb);
		JO_ZERO(glass_attributes[k].gstb);
		glass_attributes[k].flag = Dual_Plane;
		jo_3d_set_mesh_polygon_texture(&setter, id, 0);
	}
	glass_loaded = true;
}

// The pane: across the hole, just inside the corridor (out 1), floor to the walls' top
static void add_corridor_glass(const corridor *passage, const world_point *eye, const wall_clip_camera *camera,
							   int top)
{
	const int width = passage->across_max - passage->across_min;
	// Whole pieces (corridor.h: the hole's whole 250 pieces); a shift and compare, no division
	const int pieces = width >= 4 * 250 ? 4 : width >= 3 * 250 ? 3 : width >= 2 * 250 ? 2 : 1;
	const int first_texture = (GLASS_PIECES - pieces) >> 1;
	world_point columns[GLASS_PIECES + 1];

	if (!glass_loaded || corridor_out(passage, *eye) <= 0)
		return; // not loaded yet, or you're not in the corridor: nothing to look back at
	for (int k = 0; k <= pieces; ++k)
		columns[k] = corridor_point(passage, 1, passage->across_min + 250 * k);
	for (int k = 0; k < pieces; ++k)
	{
		const int first = block_high.mesh.data.nbPoint;

		block_band_add(&block_high, &glass_attributes[first_texture + k], &columns[k], 1, WALL_BOTTOM_Z, top, camera,
					   CORRIDOR_MIN_FORWARD);
		if (camera != JO_NULL)
			cap_band_tops(&block_high, first, camera);
	}
}

static void draw_corridor_walls(const corridor *passage, const world_rect *view, const world_point *eye,
								const wall_clip_camera *camera, bool cutaway, int rise)
{
	static unsigned int frame = 0; // counts the frames it's drawn: the pulse's clock
	world_point from[CORRIDOR_FACES];
	world_point to[CORRIDOR_FACES];
	ATTR base;
	ATTR body;
	const int top = (WALL_TOP_Z * ((rise * CORRIDOR_WALL_RISE) >> 8)) >> 8;

	const int corridor_frc = jo_time_get_frc(); // the debug line's "co" (utils/perf.h)
	int sides_frc;

	set_corridor_wall_attribute(&base, corridor_pulse(frame++));
	set_corridor_wall_attribute(&body, CORRIDOR_BODY);
	// Sorted by their furthest corner, not their middle: the corridor's walls bound it, so from
	// inside they're behind everything in it. By the middle, a wall piece (250 long) beside the
	// stairs came out nearer than the steps' fronts, and was drawn over their ends: a notch cut
	// out of the stairs where they meet the wall. (SGL's sort is the low 2 bits: SKILLS.md)
	base.sort = (Uint8)((base.sort & ~3) | SORT_MAX);
	body.sort = (Uint8)((body.sort & ~3) | SORT_MAX);
	corridor_faces(passage, from, to);
	for (int i = 0; i < CORRIDOR_FACES; ++i)
	{
		// Each face lies along x or along y: its line, where it starts, and which way it runs
		const bool along_x = from[i].y == to[i].y;
		const int start = along_x ? from[i].x : from[i].y;
		const int end = along_x ? to[i].x : to[i].y;
		const int length = end > start ? end - start : start - end;
		world_point columns[ARENA_WALL_COLUMNS_MAX];
		// Whole 250 pieces (corridor.h). A division, but once per face: three a frame
		const int count = run_columns(along_x, along_x ? from[i].y : from[i].x, start, end > start ? 1 : -1,
									  length / (2 * ARENA_PIECE_HALF), view, eye, columns);
		wall_clip_camera low;
		// The pieces this face adds, for cap_band_bottoms and lay_wide_pieces_flat
		const int low_point = block_low.mesh.data.nbPoint;
		const int low_polygon = block_low.mesh.data.nbPolygon;
		const int high_polygon = block_high.mesh.data.nbPolygon;

		if (count == 0)
			continue;
		// The glowing base, then the dark glass above it (the cutaway: just the base, as the walls).
		// The base is never more than 5 above the eye: only the body needs its tops cut
		corridor_cone = true;
		upright_lines = true; // flat colours: no texture to turn on its side
		block_band_add(&block_low, &base, columns, count, WALL_BOTTOM_Z, WALL_BASE_TOP_Z,
					   corridor_strip_camera(camera, WALL_BASE_TOP_Z, WALL_BOTTOM_Z, &low),
					   corridor_near_below(WALL_BASE_TOP_Z, WALL_BOTTOM_Z));
		if (!cutaway)
		{
			const int eye = -view_z;
			int first;

			// Looking down, the body below the eye is seen further aside the further below it is
			// (corridor_strip_camera): it's added in bands, each cut as widely as is safe for it:
			// up to CORRIDOR_SPLIT_BELOW under the eye, on up to CORRIDOR_SPLIT_NEAR under it, and
			// the rest (round the eye's height, and above it: near you, that's behind the tilted
			// camera, and keeps the level cut). In one, the part just under the eye beside you went
			// missing: the white floor showed through
			int cuts[4], n = 0;

			cuts[n++] = WALL_BASE_TOP_Z;
			if (camera != JO_NULL && view_look_pitch() > 0)
			{
				if (eye - CORRIDOR_SPLIT_BELOW > cuts[n - 1] && eye - CORRIDOR_SPLIT_BELOW < top)
					cuts[n++] = eye - CORRIDOR_SPLIT_BELOW;
				if (eye - CORRIDOR_SPLIT_NEAR > cuts[n - 1] && eye - CORRIDOR_SPLIT_NEAR < top)
					cuts[n++] = eye - CORRIDOR_SPLIT_NEAR;
			}
			cuts[n++] = top;
			for (int k = 0; k < n - 1; ++k)
			{
				first = block_high.mesh.data.nbPoint; // the last band's are the ones capped
				block_band_add(&block_high, &body, columns, count, cuts[k], cuts[k + 1],
							   corridor_strip_camera(camera, cuts[k + 1], cuts[k], &low),
							   corridor_near_below(cuts[k + 1], cuts[k]));
				if (camera != JO_NULL)
					cap_band_bottoms(&block_high, first, camera); // each band's its own strip
			}
			if (camera != JO_NULL)
				cap_band_tops(&block_high, first, camera);
		}
		// The far end's wall faces you down the corridor: lying lines, as a box's front
		if (camera != JO_NULL)
		{
			cap_band_bottoms(&block_low, low_point, camera); // the base: one strip
			lay_wide_pieces_flat(&block_low, low_polygon, along_x, camera);
			lay_wide_pieces_flat(&block_high, high_polygon, along_x, camera);
		}
		corridor_cone = false;
		upright_lines = false;
	}
	// The glass behind you: textured, so its corners go top first (no upright lines)
	if (!cutaway)
	{
		corridor_cone = true;
		add_corridor_glass(passage, eye, camera, top);
		corridor_cone = false;
	}
	perf_part_add(PERF_CORRIDOR, corridor_frc);
	sides_frc = jo_time_get_frc();
	add_grey_box_sides(passage, eye, camera);
	perf_part_add(PERF_SIDES, sides_frc);
}

void draw_arena_walls(const world_rect *view, const world_point *eye, const wall_clip_camera *camera, bool cutaway,
					  int rise, const wall_flicker *flicker, const corridor *passage)
{
	if (rise <= 0)
		return; // still flat in the floor
	// The blocks' bands take the flicker's pieces as well as the blocks' faces: filled by both,
	// then drawn once
	int frc = jo_time_get_frc(); // the debug line's "ar" (utils/perf.h)

	block_band_begin(&block_low);
	block_band_begin(&block_high);
	for (int s = 0; s < 4; ++s)
		// Cutaway: the south and west walls stand between the iso camera (which sits on the +x, +y
		// side of the cycle) and the cycle. Full height they'd hide it, and their tops would rise
		// behind the camera. Just the glowing base, lower than the cycle, still marks the edge
		draw_side(&sides[s], s, view, eye, camera, cutaway && sides[s].fixed > 0, rise, flicker);
	draw_blocks(view, eye, camera, cutaway, rise);
	perf_part_add(PERF_ARENA, frc);
	if (passage != JO_NULL && passage->open)
		draw_corridor_walls(passage, view, eye, camera, cutaway, rise);
	frc = jo_time_get_frc();
	block_band_draw(&block_low);
	block_band_draw(&block_high);
	perf_part_add(PERF_BANDS, frc);
}

// A flat coloured quad's attribute, as jo_3d_set_mesh_polygon_color sets one, but sorted by its
// nearest corner (in front of everything round it) or its furthest (behind everything)
static void set_surround_attribute(ATTR *a, jo_color colour, bool in_front)
{
	a->flag = Dual_Plane;
	a->sort = (in_front ? SORT_MIN : SORT_MAX) | ((sprPolygon >> 16) & 0x1c) | No_Option;
	a->texno = No_Texture;
	a->atrb = (CL32KRGB | No_Gouraud) | ((sprPolygon >> 24) & 0xc0);
	a->colno = colour;
	a->gstb = 0;
	a->dir = sprPolygon & 0x3f;
}

void draw_arena_surround(int rise, bool cutaway)
{
	surround_piece pieces[SURROUND_PIECES];
	// Its height: level with the walls' tops, rising with them. In the iso cutaway the south and
	// west walls are just their glowing base, so the ground behind them is as low
	const int top = (WALL_TOP_Z * rise) >> 8;
	world_point top_quad[4];
	world_point base_quad[4];
	// Where the camera sees each height (view.h): false when it can't see that height's top
	const bool top_seen = view_floor_quad(top, top_quad);
	const bool base_seen = cutaway && view_floor_quad(WALL_BASE_TOP_Z, base_quad);
	const int half = arena_half();
	world_point eye;
	int points = 0;
	int quads = 0;

	if (!top_seen && !base_seen)
		return; // in 1st person, the camera's always under it: nothing to work out
	eye.x = (short)view_x;
	eye.y = (short)view_y;
	surround_pieces(half, pieces);
	for (int p = 0; p < SURROUND_PIECES; ++p)
	{
		const bool low = cutaway && (pieces[p].side == SURROUND_SOUTH || pieces[p].side == SURROUND_WEST);
		const bool in_front = surround_in_front(pieces[p].side, eye, half);
		const jo_color colour = pieces[p].rim ? SURROUND_GLOW : SURROUND_GROUND;
		const FIXED z = jo_int2fixed(low ? WALL_BASE_TOP_Z : top);
		world_point corners[SURROUND_CLIP_MAX];
		unsigned char fog_levels[SURROUND_CLIP_MAX];
		int n;

		if (low ? !base_seen : !top_seen)
			continue;
		n = surround_clip(&pieces[p].area, low ? base_quad : top_quad, corners);
		if (n == 0)
			continue; // not on the screen
		for (int i = 0; i < n; ++i)
		{
			surround_points[points + i][X] = jo_int2fixed(corners[i].x);
			surround_points[points + i][Y] = jo_int2fixed(corners[i].y);
			surround_points[points + i][Z] = z;
			fog_levels[i] = object_fog_enabled ? (unsigned char)object_fog_level(corners[i].x, corners[i].y) : 0;
		}
		// The fan: (0 1 2 3), (0 3 4 5), (0 5 6 7); past the last corner, the last again
		for (int k = 1; k < n - 1; k += 2)
		{
			POLYGON *q = &surround_polygons[quads];
			ATTR *a = &surround_attributes[quads];
			int c2 = k + 1;
			int c3 = k + 2 < n ? k + 2 : n - 1;

			q->Vertices[0] = (Uint16)points;
			q->Vertices[1] = (Uint16)(points + k);
			q->Vertices[2] = (Uint16)(points + c2);
			q->Vertices[3] = (Uint16)(points + c3);
			set_surround_attribute(a, colour, in_front);
			if (object_fog_enabled)
				object_fog_set_quad(a, fog_levels[0], fog_levels[k], fog_levels[c2], fog_levels[c3]);
			++quads;
		}
		points += n;
	}
	if (quads == 0)
		return;
	surround_mesh.data.nbPoint = (Uint16)points;
	surround_mesh.data.nbPolygon = (Uint16)quads;
	jo_3d_mesh_draw(&surround_mesh);
}

// A tile at least this far in front of the camera all over is drawn as it is. Nearer, it's cut to
// what the camera sees (corridor_clip): uncut, a tile 64 in front had corners hundreds of pixels
// off the screen's side, and VDP1 walks every off-screen pixel of a polygon too
#define GLASS_MIN_FORWARD 250
// Tiles further ahead than this aren't drawn one by one: past it the floor's one plain quad of
// glass (in the fog, a hexagon's edges no longer show). Every tile would be 300 polygons
#define GLASS_DETAIL 2000

/*
 * Where a tile stands from the 1st person camera, from its centre (every corner is within
 * CORRIDOR_HEX_R of it): 0 to leave it out (behind the camera, wholly off to one side, or past
 * the detail), 1 to draw it as it is (wholly well in front), 2 to cut it to the screen first (it's
 * at your feet). One dot product and one cross product, 32-bit, no division: cutting every tile
 * every frame (64-bit sums) took half a frame. Only the few at your feet are cut now
 */
static int glass_tile_place(const wall_clip_camera *camera, world_point centre, int below_reach)
{
	const int dx = centre.x - camera->eye.x;
	const int dy = centre.y - camera->eye.y;
	const int forward = (dx * camera->forward_x + dy * camera->forward_y) >> 10;
	const int side = (dx * camera->forward_y - dy * camera->forward_x) >> 10;
	// The view's half width there, and wider looking down from up high (below_reach)
	const int reach = (((forward > 0 ? forward : 0) * camera->view_width) >> 4) + below_reach;

	if (forward + CORRIDOR_HEX_R < 0 || forward > GLASS_DETAIL)
		return 0;
	if ((side < 0 ? -side : side) > reach + CORRIDOR_HEX_R + GLASS_MIN_FORWARD)
		return 0;
	return forward - CORRIDOR_HEX_R >= GLASS_MIN_FORWARD ? 1 : 2;
}

// A convex polygon of glass on the floor, added to the mesh as a fan, as the surround's: (0 1 2 3),
// (0 3 4 5) ...; past the last corner, the last again. The first (the far glass, or the first tile)
// is sorted by its furthest corner: the floor, behind everything stood on it. Every one after
// (on_top) is SORT_BFR: drawn straight after the one before, so the tiles always lie over it
// (by their own depths, a tile and the floor under it could sort either way). Returns false when
// there's no room left
// fog: GLASS_FOG_CORNERS for a fog level worked out at each corner (Gouraud: the far glass, a
// long quad), or the one level for all of it (a tile: drawn in that darker colour, no Gouraud.
// Per corner, every tile cost six fog levels and two Gouraud tables written into VDP1 RAM while
// VDP1 was drawing: the floor took a third of the frame down the corridor)
#define GLASS_FOG_CORNERS (-1)
static bool add_glass(const world_point *corners, int n, jo_color colour, int fog, bool on_top, int *points, int *quads)
{
	unsigned char fog_levels[CORRIDOR_CLIP_MAX];

	if (*points + n > CORRIDOR_POINTS_MAX || *quads + (n - 1) / 2 > CORRIDOR_QUADS_MAX)
		return false;
	if (fog > 0)
		colour = object_fog_flat_colour(colour, fog);
	for (int i = 0; i < n; ++i)
	{
		corridor_points[*points + i][X] = jo_int2fixed(corners[i].x);
		corridor_points[*points + i][Y] = jo_int2fixed(corners[i].y);
		corridor_points[*points + i][Z] = JO_FIXED_0; // on the floor
		if (fog == GLASS_FOG_CORNERS)
			fog_levels[i] = object_fog_enabled ? (unsigned char)object_fog_level(corners[i].x, corners[i].y) : 0;
	}
	for (int k = 1; k < n - 1; k += 2)
	{
		POLYGON *q = &corridor_polygons[*quads];
		ATTR *a = &corridor_attributes[*quads];
		const int c2 = k + 1;
		const int c3 = k + 2 < n ? k + 2 : n - 1;

		q->Vertices[0] = (Uint16)*points;
		q->Vertices[1] = (Uint16)(*points + k);
		q->Vertices[2] = (Uint16)(*points + c2);
		q->Vertices[3] = (Uint16)(*points + c3);
		set_surround_attribute(a, colour, false);
		if (on_top || k > 1)
			a->sort = (Uint8)((a->sort & ~3) | SORT_BFR); // the sort is the low 2 bits (as main.c's cube)
		if (object_fog_enabled && fog == GLASS_FOG_CORNERS)
			object_fog_set_quad(a, fog_levels[0], fog_levels[k], fog_levels[c2], fog_levels[c3]);
		++*quads;
	}
	*points += n;
	return true;
}

// The grey box's tops: flat quads at their heights, each cut to what the camera sees of its
// height (view_floor_quad), as the glass is. Not every box's top fits at once: the furthest are
// left out first (they're added nearest first... in the order they're listed, up the corridor:
// near enough for a grey box)
static jo_3d_mesh grey_top_mesh; // its arrays in Work RAM's memory (corridor_memory)

// Where a box's top stands from the camera, as glass_tile_place for a tile: 0 out of sight, 1 to
// draw as it is (wholly well in front), 2 to cut to the screen first (near you)
// A top's grey: GREY_TOP at the corridor's floor and above, darker the lower it is (a step of 1 in
// 255 every 4 units down, to 100 darker), so the city's roofs one below another read apart: all one
// grey, the street between two looked like more roof
static jo_color grey_top_colour(int height)
{
	const int dim = height >= 0 ? 0 : (-height >> 2) > 100 ? 100 : (-height >> 2);

	return JO_COLOR_RGB(150 - dim, 150 - dim, 164 - (dim >> 1));
}

static int grey_top_place(const wall_clip_camera *camera, const grey_box *box, int height)
{
	const int dx = box->centre.x - camera->eye.x;
	const int dy = box->centre.y - camera->eye.y;
	const int forward = (dx * camera->forward_x + dy * camera->forward_y) >> 10;
	const int side = (dx * camera->forward_y - dy * camera->forward_x) >> 10;
	// The view's half width there; looking down, wider by how far below the eye it is
	// (corridor_cone_below: x16)
	const int reach = (((forward > 0 ? forward : 0) * camera->view_width) >> 4) + (corridor_cone_below(height) >> 4);

	if (forward + box->radius < 0)
		return 0;
	if ((side < 0 ? -side : side) > reach + box->radius + GLASS_MIN_FORWARD)
		return 0;
	return forward - box->radius >= GLASS_MIN_FORWARD ? 1 : 2;
}

void draw_corridor_boxes(const corridor *passage, const wall_clip_camera *camera)
{
	int points = 0, quads = 0;
	const int eye = -view_z;

	if (passage == JO_NULL || !passage->open || corridor_meshes == JO_NULL)
		return;
	grey_boxes_work_out(passage);
	grey_portal_set(passage, camera);
	for (int i = 0; i < GREY_BOXES; ++i)
	{
		const world_rect *r = &grey_boxes[i].rect;
		const world_point corners[4] = {{r->x_min, r->y_min}, {r->x_max, r->y_min}, {r->x_max, r->y_max},
										{r->x_min, r->y_max}};
		const world_point *top = corners;
		const int place = camera != JO_NULL ? grey_top_place(camera, &grey_boxes[i], grey_boxes[i].height) : 2;
		world_point seen[4], cut[CORRIDOR_CLIP_MAX], through[CORRIDOR_CLIP_MAX];
		int n = 4;

		if (place == 0 || eye <= grey_boxes[i].height || !grey_box_drawn(i))
			continue; // out of sight, the eye's not above it (its top can't be seen), or hidden
		// A tower's top seen from inside the corridor: only what's through the doorway
		if (i >= CORRIDOR_BOXES && portal.inside && !grey_portal_holds(r))
		{
			n = corridor_clip(corners, 4, portal.quad, through);
			if (n < 3)
				continue;
			top = through;
		}
		// Only a top near you is cut to what the camera sees (corridor_clip's 64-bit sums): one
		// well in front is drawn as it is
		if (place == 2)
		{
			if (!view_roof_quad(grey_boxes[i].height, seen))
				continue;
			n = corridor_clip(top, n, seen, cut);
			top = cut;
		}
		if (n < 3 || points + n > GREY_TOP_POINTS_MAX || quads + (n - 1) / 2 > GREY_TOP_QUADS_MAX)
			continue;
		for (int k = 0; k < n; ++k)
		{
			grey_top_points[points + k][X] = jo_int2fixed(top[k].x);
			grey_top_points[points + k][Y] = jo_int2fixed(top[k].y);
			grey_top_points[points + k][Z] = jo_int2fixed(grey_boxes[i].height);
		}
		// A fan, as the glass's: (0 1 2 3), (0 3 4 5) ...; past the last corner, the last again
		for (int k = 1; k < n - 1; k += 2)
		{
			POLYGON *q = &grey_top_polygons[quads];

			q->norm[X] = JO_FIXED_0;
			q->norm[Y] = JO_FIXED_0;
			q->norm[Z] = JO_FIXED_1;
			q->Vertices[0] = (Uint16)points;
			q->Vertices[1] = (Uint16)(points + k);
			q->Vertices[2] = (Uint16)(points + k + 1);
			q->Vertices[3] = (Uint16)(points + (k + 2 < n ? k + 2 : n - 1));
			set_corridor_wall_attribute(&grey_top_attributes[quads], grey_top_colour(grey_boxes[i].height));
			++quads;
		}
		points += n;
	}
	if (quads == 0)
		return;
	grey_top_mesh.data.pntbl = grey_top_points;
	grey_top_mesh.data.pltbl = grey_top_polygons;
	grey_top_mesh.data.attbl = grey_top_attributes;
	grey_top_mesh.data.nbPoint = (Uint16)points;
	grey_top_mesh.data.nbPolygon = (Uint16)quads;
	jo_3d_mesh_draw(&grey_top_mesh);
}

void draw_corridor_floor(const corridor *passage, const wall_clip_camera *camera)
{
	// What the camera sees of the floor, worked out only once something needs cutting to it:
	// then no corner is behind the camera or far off the screen (VDP1 would wrap it into a spike)
	world_point seen[4];
	bool seen_known = false;
	int points = 0;
	int quads = 0;
	// Looking down from up high (a roof), the floor's seen further aside than the level view
	// reaches (corridor_cone_below), in world units
	const int below_reach = corridor_cone_below(0) >> 4;
	// Whether the far glass is under the tiles: they're chained after it (SORT_BFR), all drawn
	// straight after it, far back. Without it (near the corridor's far end), the chain hung off
	// the first tile listed, by the arena's wall behind you, and the tiles were drawn over the
	// grey box's steps: each is sorted by its own furthest corner instead
	bool far_glass = false;

	if (passage == JO_NULL || !passage->open || corridor_meshes == JO_NULL)
		return;
	grey_portal_set(passage, camera);
	if (portal.outside)
		return; // out in the city: the corridor's floor is behind its walls (grey_portal)
	// Past the detail, the rest of the corridor as one quad of glass, cut to the screen: from a
	// hexagon's width short of it, under the last tiles' jagged edge (the tiles are SORT_BFR after
	// it, so they lie over it). The seams between the tiles are left open: the arena's floor (VDP2)
	// shows through them, a lighter navy than the glass, for nothing. A floor quad under them all
	// in a seam colour cost VDP1 the whole floor twice over: 30 fps
	if (!view_ground_quad(seen))
		return; // the camera can't see the floor at all
	seen_known = true;
	if (camera != JO_NULL)
	{
		const int detail = corridor_out(passage, camera->eye) + GLASS_DETAIL - 2 * CORRIDOR_HEX_R;
		world_point cut[CORRIDOR_CLIP_MAX];
		int n;

		if (detail < CORRIDOR_LENGTH)
		{
			const int far_from = detail > 0 ? detail : 0;
			const world_point far[4] = {
				corridor_point(passage, far_from, passage->across_min),
				corridor_point(passage, CORRIDOR_LENGTH, passage->across_min),
				corridor_point(passage, CORRIDOR_LENGTH, passage->across_max),
				corridor_point(passage, far_from, passage->across_max),
			};

			n = corridor_clip(far, 4, seen, cut);
			if (n > 0)
				far_glass = add_glass(cut, n, glass_shades[1], GLASS_FOG_CORNERS, quads > 0, &points, &quads);
		}
	}
	for (int t = 0; t < passage->tile_count; ++t)
	{
		const corridor_tile *tile = &passage->tiles[t];
		const int place = camera != JO_NULL ? glass_tile_place(camera, tile->centre, below_reach) : 2;
		world_point cut[CORRIDOR_CLIP_MAX];
		const world_point *corners = tile->corners;
		int n = tile->count;

		if (place == 0)
			continue;
		if (place == 2)
		{
			if (!seen_known)
			{
				if (!view_ground_quad(seen))
					return; // the camera can't see the floor at all
				seen_known = true;
			}
			n = corridor_clip(tile->corners, tile->count, seen, cut);
			corners = cut;
			if (n == 0)
				continue; // not on the screen
		}
		if (!add_glass(corners, n, glass_shades[tile->shade],
					   object_fog_enabled ? object_fog_level(tile->centre.x, tile->centre.y) : 0, far_glass, &points,
					   &quads))
			break; // no room: the rest are left out this frame
	}
	if (quads == 0)
		return;
	corridor_mesh.data.nbPoint = (Uint16)points;
	corridor_mesh.data.nbPolygon = (Uint16)quads;
	jo_3d_mesh_draw(&corridor_mesh);
}
