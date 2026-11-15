#include "level.h"
#include "game/settings.h" // the pause menu's "City sides" (CITY_SIDES_TO_BOARD at boot, features.h)
#include "game/body.h"
#include "game/walker.h"
#include "utils/angles.h"
#include "scene_memory.h"
#include "game/arena.h"
#include "game/surround.h"
#include "game/city.h"
#include "game/occluder.h"
#include "game/skyline.h"
#include "game/floor_hides.h" // what the roof you stand on hides
#include "features.h"           // FLOOR_HIDES
#include "game/landmarks.h"
#include "object_fog_vdp1.h"
#include "view.h"
#include "utils/perf.h"
#include "slave_draw.h"

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
 * and unmirrored. SGL maps a texture's corners to vertices[0..3] as
 * top-left, top-right, bottom-right, bottom-left.
 */

#define STRIP_PIECES (ARENA_WALL_COLUMNS_MAX - 1)
#define STRIP_POINTS (ARENA_WALL_COLUMNS_MAX * 2)

typedef struct
{
	jo_3d_mesh mesh;
	jo_pos3D_fixed points[STRIP_POINTS];
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
static jo_3d_polygon piece_polygons[STRIP_PIECES];
static jo_3d_polygon_attribute low_attributes[STRIP_PIECES];
static jo_3d_polygon_attribute high_attributes[STRIP_PIECES];
static jo_3d_polygon_attribute base_attributes[STRIP_PIECES];

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

static jo_pos3D_fixed surround_points[SURROUND_PIECES * SURROUND_CLIP_MAX];
static jo_3d_polygon surround_polygons[SURROUND_QUADS_MAX];
static jo_3d_polygon_attribute surround_attributes[SURROUND_QUADS_MAX];
static jo_3d_mesh surround_mesh;

/*
 * The corridor's floor (game/corridor.h): its hexagons, each cut to what's on the screen, as
 * plain coloured fans like the surround's. Dark, matt glass: three shades of near black, no
 * texture, no shine; the floor fog fades them as it does everything. The arena's navy floor shows
 * through the seams between them as lines, as the grid's lines show on it. Room for every tile on the screen at once, most of them 2 quads (6 corners);
 * one that won't fit is left out (a gap far off, not a crash)
 */
// On foot each tile has its outline too, three polylines on its own corners (draw_glass_floor)
#define CORRIDOR_POINTS_MAX 720
#define CORRIDOR_QUADS_MAX 480

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

static void skyline_load(void); // the city all round (below)

void load_arena_textures(void)
{
	// 24-bit, 33 KB of VDP1 RAM between them (they replace the four debug BOX textures)
	low_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "WALL_LO.TGA", JO_COLOR_Transparent);
	high_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "WALL_HI.TGA", JO_COLOR_Transparent);
	base_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "WALLBASE.TGA", JO_COLOR_Transparent);
	skyline_load(); // the city all round (skyline_add): 16 KB more
	// A wall piece right beside the camera reaches far off the screen, past jo-engine 2026's guard
	// band (512 pixels out: SGL's limit, and wall_clip's, was VDP1's 1023). jo then cuts the
	// polygon, and VDP1 stretched the whole texture over the cut shape: the near wall's lines
	// slanted the wrong way and fanned out (2026-11-08). With tiles of the texture, jo cuts such a
	// polygon into parts each with its own piece of it. Only those polygons (_near_only): the rest
	// are drawn whole, as before. The tiles are copies of the texture: ~60 KB more of VDP1 RAM
	jo_3d_set_texture_subdivision_near_only(low_texture_id);
	jo_3d_set_texture_subdivision_near_only(high_texture_id);
	jo_3d_set_texture_subdivision_near_only(base_texture_id);
}

static void init_attributes(jo_3d_polygon_attribute *attributes, int texture_id)
{
	jo_3d_mesh setter; // jo's texture setter works on a mesh: point one at the shared table

	setter.data.attributes = attributes;
	setter.data.polygon_count = STRIP_PIECES;
	for (int j = 0; j < STRIP_PIECES; ++j)
	{
		// Same defaults as jo_3d_create_plane, so jo's texture setter behaves the same
		JO_ZERO(attributes[j].sort);
		JO_ZERO(attributes[j].draw_mode);
		JO_ZERO(attributes[j].gouraud_table);
		attributes[j].plane = JO_3D_DUAL_PLANE;
		jo_3d_set_mesh_polygon_texture(&setter, texture_id, j);
		// Fog: VDP1 shades each corner by its own Gouraud colour (object_fog_vdp1.h)
		if (object_fog_enabled)
			attributes[j].draw_mode |= JO_VDP1_COLOR_CALC_GOURAUD;
	}
}

static void init_strip(wall_strip *strip, jo_3d_polygon_attribute *attributes)
{
	strip->mesh.data.points = strip->points;
	strip->mesh.data.polygons = piece_polygons;
	strip->mesh.data.attributes = attributes;
	strip->mesh.data.point_count = 0;
	strip->mesh.data.polygon_count = 0;
}

void init_arena_walls(void)
{
	for (int j = 0; j < STRIP_PIECES; ++j)
	{
		piece_polygons[j].normal.x = JO_FIXED_0;
		piece_polygons[j].normal.y = JO_FIXED_1;
		piece_polygons[j].normal.z = JO_FIXED_0;
		piece_polygons[j].vertices[0] = j * 2 + 1; // left, top
		piece_polygons[j].vertices[1] = j * 2 + 3; // right, top
		piece_polygons[j].vertices[2] = j * 2 + 2; // right, bottom
		piece_polygons[j].vertices[3] = j * 2;	   // left, bottom
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
		surround_polygons[q].normal.x = JO_FIXED_0; // unused: both faces are drawn, unlit
		surround_polygons[q].normal.y = JO_FIXED_0;
		surround_polygons[q].normal.z = JO_FIXED_1;
	}
	surround_mesh.data.points = surround_points;
	surround_mesh.data.polygons = surround_polygons;
	surround_mesh.data.attributes = surround_attributes;
	surround_mesh.data.point_count = 0;
	surround_mesh.data.polygon_count = 0;
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

// Set while the arena's walls are added on foot (draw_arena_walls): fill_strip leaves out the
// pieces at a run's ends that can't be seen (arena_piece_hidden, below). STRIP_SEEN_ALL: nothing
// left out; STRIP_OUTSIDE: an outer side's face, seen from outside; STRIP_INSIDE: the square's inside
// (a far side's inner face, a block), seen from outside only through the hole
#define STRIP_SEEN_ALL 0
#define STRIP_OUTSIDE 1
#define STRIP_INSIDE 2
static int strip_seen = STRIP_SEEN_ALL;
static bool arena_piece_hidden(world_point a, world_point b, int z_bottom, int z_top, bool inside);

/*
 * How many of a run's pieces at one end can't be seen, as one box: end is that end's column, and the
 * run's `count` pieces go on from it the way `step` says (+1: columns[1], [2] ...; -1: back). The
 * whole run isn't hidden (fill_strip tests it first). Hidden-ness only falls as the box grows (a
 * part of a hidden box is hidden), so it's found by halving: hidden lo pieces, not hidden hi, and
 * the gap halved each test, ~5 tests for a 30 piece side, not 30. A shift, no division. The ends
 * need one reason each (one wall's shadow, or out of the hole's sight): an end hidden by two walls
 * in turn is drawn, never one seen left out
 */
static int strip_hidden_end(const world_point *end, int count, int step, int z_bottom, int z_top, bool inside)
{
	int lo = 0, hi = count;

	while (hi - lo > 1)
	{
		const int mid = (lo + hi) >> 1;

		if (arena_piece_hidden(end[0], end[step * mid], z_bottom, z_top, inside))
			lo = mid;
		else
			hi = mid;
	}
	return lo;
}

static int fill_strip(jo_pos3D_fixed *points, unsigned char *fog_levels, const world_point *columns, int pieces, int z_bottom,
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
		/*
		 * On foot, the pieces at each end of the run that can't be seen are left off it (a piece
		 * in the middle stays: the strip is one mesh, its pieces sharing columns). Behind the
		 * corridor seen from the city, that's all of them. A piece dropped from an end takes its
		 * slid end with it: the next piece's starts at its own column. Each test is the whole
		 * piece, slid or not: a little more than's drawn, so it's never wrongly left out.
		 * The whole run's tested first, as one box: hidden, that's one test. Then each end's hidden
		 * pieces are found by halving (strip_hidden_end): a piece at a time, 3 walls' 5 planes each,
		 * was ~400 tests a frame and cost more than drawing them (30 fps still)
		 */
		if (strip_seen != STRIP_SEEN_ALL && count > 0)
		{
			const bool inside = strip_seen == STRIP_INSIDE;
			int drop;

			if (arena_piece_hidden(columns[first], columns[first + count], z_bottom, z_top, inside))
				return 0;
			drop = strip_hidden_end(&columns[first], count, 1, z_bottom, z_top, inside);
			if (drop > 0)
			{
				first += drop;
				count -= drop;
				run.start = columns[first];
			}
			drop = strip_hidden_end(&columns[first + count], count, -1, z_bottom, z_top, inside);
			if (drop > 0)
			{
				count -= drop;
				run.end = columns[first + count];
			}
		}
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

		points[i * 2].x = jo_int2fixed(p.x);
		points[i * 2].y = jo_int2fixed(p.y);
		points[i * 2].z = jo_int2fixed(z_bottom);
		points[i * 2 + 1].x = points[i * 2].x;
		points[i * 2 + 1].y = points[i * 2].y;
		points[i * 2 + 1].z = jo_int2fixed(z_top);
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
	// Each piece's Gouraud table, its corners in vertices order (init_arena_walls):
	// left top, right top, right bottom, left bottom. The attributes are shared by every strip of
	// this kind, but SGL reads them during jo_3d_mesh_draw below, before the next strip changes them
	if (object_fog_enabled)
		for (int j = 0; j < count; ++j)
			object_fog_set_quad(&strip->mesh.data.attributes[j], fog_levels[j], fog_levels[j + 1], fog_levels[j + 1],
								fog_levels[j]);
	strip->mesh.data.point_count = (count + 1) * 2;
	strip->mesh.data.polygon_count = count;
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
	jo_pos3D_fixed points[BLOCK_POINTS_MAX];
	jo_3d_polygon polygons[BLOCK_PIECES_MAX];
	jo_3d_polygon_attribute attributes[BLOCK_PIECES_MAX];
} block_band;

static block_band block_low;
static block_band block_high;

/*
 * Set while the corridor's walls are added (draw_corridor_walls): block_band_add lists each
 * piece's corners starting from a bottom one, so VDP1 fills it with upright lines, not lying ones.
 * VDP1 fills a quad with lines from its first-to-last edge (vertices 0 to 3) across to its second
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

static void block_band_begin(block_band *band)
{
	band->mesh.data.points = band->points;
	band->mesh.data.polygons = band->polygons;
	band->mesh.data.attributes = band->attributes;
	band->mesh.data.point_count = 0;
	band->mesh.data.polygon_count = 0;
}

// One face's strip, appended to the band, textured as `texture` (one of the bands' attribute
// tables: low, high or base). Stops at a piece that wouldn't fit: a gap far off, not a crash
static void block_band_add(block_band *band, const jo_3d_polygon_attribute *texture, const world_point *columns, int pieces, int z_bottom,
						   int z_top, const wall_clip_camera *camera, int min_forward)
{
	unsigned char fog_levels[ARENA_WALL_COLUMNS_MAX];
	const int base = band->mesh.data.point_count;
	int polygon = band->mesh.data.polygon_count;
	int count;

	if (polygon + pieces > BLOCK_PIECES_MAX)
		pieces = BLOCK_PIECES_MAX - polygon;
	if (pieces <= 0)
		return;
	count = fill_strip(&band->points[base], fog_levels, columns, pieces, z_bottom, z_top, camera, min_forward);
	for (int j = 0; j < count; ++j, ++polygon)
	{
		jo_3d_polygon *q = &band->polygons[polygon];

		q->normal.x = JO_FIXED_0; // unused: the walls are unlit
		q->normal.y = JO_FIXED_1;
		q->normal.z = JO_FIXED_0;
		band->attributes[polygon] = *texture;
		if (upright_lines)
		{
			// The same corners, the same way round, starting a corner earlier: left bottom, left
			// top, right top, right bottom (upright_lines, above)
			q->vertices[0] = (unsigned short)(base + j * 2);
			q->vertices[1] = (unsigned short)(base + j * 2 + 1);
			q->vertices[2] = (unsigned short)(base + j * 2 + 3);
			q->vertices[3] = (unsigned short)(base + j * 2 + 2);
			if (object_fog_enabled) // the fog's shades in the same corners' order
				object_fog_set_quad(&band->attributes[polygon], fog_levels[j], fog_levels[j], fog_levels[j + 1],
									fog_levels[j + 1]);
			continue;
		}
		// As init_arena_walls: left top, right top, right bottom, left bottom
		q->vertices[0] = (unsigned short)(base + j * 2 + 1);
		q->vertices[1] = (unsigned short)(base + j * 2 + 3);
		q->vertices[2] = (unsigned short)(base + j * 2 + 2);
		q->vertices[3] = (unsigned short)(base + j * 2);
		if (object_fog_enabled)
			object_fog_set_quad(&band->attributes[polygon], fog_levels[j], fog_levels[j + 1], fog_levels[j + 1],
								fog_levels[j]);
	}
	if (count > 0)
		band->mesh.data.point_count = (unsigned short)(base + (count + 1) * 2);
	band->mesh.data.polygon_count = (unsigned short)polygon;
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
	for (int p = first_polygon; p < (int)band->mesh.data.polygon_count; ++p)
	{
		jo_3d_polygon *q = &band->polygons[p];
		// Upright order (block_band_add): left bottom, left top, right top, right bottom
		const int lb = q->vertices[0], lt = q->vertices[1], rt = q->vertices[2], rb = q->vertices[3];
		const int dx0 = jo_fixed2int(band->points[lb].x) - camera->eye.x;
		const int dy0 = jo_fixed2int(band->points[lb].y) - camera->eye.y;
		const int dx1 = jo_fixed2int(band->points[rb].x) - camera->eye.x;
		const int dy1 = jo_fixed2int(band->points[rb].y) - camera->eye.y;
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
		tall0 = (jo_fixed2int(band->points[lt].z) - jo_fixed2int(band->points[lb].z)) * d1;
		tall1 = (jo_fixed2int(band->points[rt].z) - jo_fixed2int(band->points[rb].z)) * d0;
		taller = tall0 > tall1 ? tall0 : tall1;
		shorter = tall0 > tall1 ? tall1 : tall0;
		wide = s1 * d0 - s0 * d1;
		if (wide < 0)
			wide = -wide;
		if (taller >= wide || wide * 2 <= d0 * d1 || taller * 4 > shorter * 5)
			continue; // taller than wide, too small to gain, or tapered: upright
		// Lying: left top, right top, right bottom, left bottom (the same way round)
		q->vertices[0] = (unsigned short)lt;
		q->vertices[1] = (unsigned short)rt;
		q->vertices[2] = (unsigned short)rb;
		q->vertices[3] = (unsigned short)lb;
		// The fog's shades follow the corners round (a piece with no fog has no Gouraud to follow)
		if (object_fog_enabled && (band->attributes[p].draw_mode & JO_VDP1_COLOR_CALC_GOURAUD))
		{
			const int left = object_fog_level(dx0 + camera->eye.x, dy0 + camera->eye.y);
			const int right = object_fog_level(dx1 + camera->eye.x, dy1 + camera->eye.y);

			object_fog_set_quad(&band->attributes[p], left, right, right, left);
		}
	}
}

static void block_band_draw(block_band *band)
{
	if (band->mesh.data.polygon_count > 0)
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
		low->mesh.data.attributes = base_attributes;
		draw_strip(low, columns, count, WALL_BOTTOM_Z, WALL_BASE_TOP_Z, camera, LOW_MIN_FORWARD);
		low->mesh.data.attributes = low_attributes;
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

	// On foot, the whole of the side that's in view tested at once (arena_piece_hidden): all
	// hidden, none of its pieces is worked out at all
	if (strip_seen != STRIP_SEEN_ALL)
	{
		const int half = arena_half();
		const int wall = side->fixed * half;
		world_point a, b;

		view_along(side, view, &along_min, &along_max);
		along_min = along_min < -half ? -half : along_min;
		along_max = along_max > half ? half : along_max;
		a.x = (short)(side->along_x ? along_min : wall);
		a.y = (short)(side->along_x ? wall : along_min);
		b.x = (short)(side->along_x ? along_max : wall);
		b.y = (short)(side->along_x ? wall : along_max);
		if (along_min <= along_max &&
			arena_piece_hidden(a, b, WALL_BOTTOM_Z, (WALL_TOP_Z * rise) >> 8, strip_seen == STRIP_INSIDE))
			return;
	}
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

		// On foot, the block as a whole first: from outside the square it's the inside, and it's
		// tested as a box from corner to corner (arena_piece_hidden takes any two)
		if (strip_seen != STRIP_SEEN_ALL)
		{
			const world_point lo = {b->x_min, b->y_min}, hi = {b->x_max, b->y_max};

			if (arena_piece_hidden(lo, hi, WALL_BOTTOM_Z, (WALL_TOP_Z * rise) >> 8, true))
				continue;
		}

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
// On foot, risen full: a third of the arena's (corridor_walls_add)
#define CORRIDOR_TOP_ON_FOOT ((WALL_TOP_Z * CORRIDOR_WALL_RISE) >> 8)
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
	return (below * (jo_sin_angle((jo_angle)pitch) >> 6)) >> 6;
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
	cos_p = jo_cos_angle((jo_angle)pitch) >> 6;
	near = ((((deepest * 164) >> 10) + 8) * 1024 - below * (jo_sin_angle((jo_angle)pitch) >> 6)) / (cos_p > 64 ? cos_p : 64);
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
static void set_corridor_wall_attribute(jo_3d_polygon_attribute *a, jo_color colour)
{
	a->plane = JO_3D_DUAL_PLANE;
	a->sort = JO_3D_SORT_CENTER | ((JO_VDP1_POLYGON >> 16) & 0x1c) | JO_3D_NO_OPTION;
	a->texture_id = JO_3D_NO_TEXTURE;
	a->draw_mode = (JO_VDP1_COLOR_MODE_RGB | JO_VDP1_NO_GOURAUD) | ((JO_VDP1_POLYGON >> 24) & 0xc0);
	a->color = colour;
	a->gouraud_table = 0;
	a->direction = JO_VDP1_POLYGON & 0x3f;
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
#define CAP_TOP_EDGE JO_DEG_TO_ANGLE(35.0)
static void cap_band_tops(block_band *band, int first, const wall_clip_camera *camera)
{
	const int eye_height = -view_z; // view_z moves the world: the eye's height is -view_z
	const int pitch = view_look_pitch(); // + down
	// The reach above the eye, per unit in front, x16
	const int reach16 = pitch < 0 ? 32 : pitch == 0 ? 16 : pitch >= CAP_TOP_EDGE ? 0 : (angle_tan((jo_angle)(CAP_TOP_EDGE - pitch)) >> 12) + 1;

	for (int i = first + 1; i < band->mesh.data.point_count; i += 2)
	{
		const int dx = jo_fixed2int(band->points[i].x) - camera->eye.x;
		const int dy = jo_fixed2int(band->points[i].y) - camera->eye.y;
		const int forward = (dx * camera->forward_x + dy * camera->forward_y) >> 10;
		const int bottom = jo_fixed2int(band->points[i - 1].z);
		int highest = eye_height + 4 + ((reach16 * (forward > 0 ? forward : 0)) >> 4);

		if (highest <= bottom)
			highest = bottom + 1;
		if (jo_fixed2int(band->points[i].z) > highest)
			band->points[i].z = jo_int2fixed(highest);
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
 * missing, the floor showing through. (The arena's walls, riding, and the corridor's then. On foot
 * everything's cut to the view in 3D instead: level_face_add, which has no such edges to mind)
 */
#define CAP_BOTTOM_EDGE JO_DEG_TO_ANGLE(37.0)

// The plane's slope below the eye, per unit in front, x16 (CAP_BOTTOM_EDGE + the pitch); -1 when
// it's too steep to cut anything. Looking up far enough, the edge is above level: nothing under
// the eye is seen, but a column's kept from 4 under it up (0)
static int bottom_cap_reach16(void)
{
	const int edge = CAP_BOTTOM_EDGE + view_look_pitch(); // + down

	if (edge >= JO_DEG_TO_ANGLE(70.0))
		return -1;
	return edge <= 0 ? 0 : (angle_tan((jo_angle)edge) >> 12) + 1;
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
	const int dx = jo_fixed2int(band->points[point].x) - camera->eye.x;
	const int dy = jo_fixed2int(band->points[point].y) - camera->eye.y;
	const int lowest = bottom_cap_at((dx * camera->forward_x + dy * camera->forward_y) >> 10, reach16);
	const int top = jo_fixed2int(band->points[point + 1].z);

	return lowest >= top ? top - 1 : lowest;
}

static void cap_band_bottoms(block_band *band, int first, const wall_clip_camera *camera)
{
	const int reach16 = bottom_cap_reach16();
	const int end = band->mesh.data.point_count;
	// Whether a column's bottom is at the plane or under it: then an edge to it from a column
	// raised to the plane stays on or below the plane. Past the strip's ends: nothing to keep to
	int lowest, next = 0, before_ok = 1;

	if (reach16 < 0 || first + 1 >= end)
		return;
	lowest = band_bottom_cap(band, first, camera, reach16);
	for (int i = first; i + 1 < end; i += 2)
	{
		const int bottom = jo_fixed2int(band->points[i].z);
		const int ok = lowest >= bottom;
		int after_ok = 1;

		if (i + 3 < end)
		{
			next = band_bottom_cap(band, i + 2, camera, reach16);
			after_ok = next >= jo_fixed2int(band->points[i + 2].z);
		}
		if (lowest > bottom && before_ok && after_ok)
			band->points[i].z = jo_int2fixed(lowest);
		before_ok = ok; // as it was built, not as it's now
		lowest = next;
	}
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
#define GREY_UNDERSIDE JO_COLOR_RGB(48, 48, 60) // a bar's, seen from below: in its own shadow
// A hub's tower (city.h, CITY_HUB): lit cyan the full height of it, a landmark to find your way
// back to from anywhere you can see it (docs/WORK_RAM.md, "Hubs and lifts")
#define HUB_SIDE_X JO_COLOR_RGB(40, 190, 225)
#define HUB_SIDE_Y JO_COLOR_RGB(28, 140, 180)
#define HUB_TOP JO_COLOR_RGB(150, 240, 255)

/*
 * The boxes as the world has them, worked out once when the corridor's open (and again only if
 * it moves): each one's rectangle, its middle (centre, and halfway up), and how far down each of
 * its sides is
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
	short middle; // halfway up, from its floor to its top
	short bottom[4];
	short height, floor;
	bool lit; // a hub's tower: drawn lit (HUB_SIDE_X ...)
} grey_box;
/*
 * The boxes in groups that stand together, each with a box round them all: tested first, as one
 * (CULLING.md: a coarse cull before the fine one). Wholly out of view, none of a group's boxes is
 * looked at; wholly in view, none of them is tested. The stairs (the corridor's first twenty boxes:
 * corridor.c lists them first), the corridor's other five, and the city's rows of three (city.c
 * lists them a row at a time). Tested one by one, every box every frame was a sixth of the frame
 */
#define GREY_STAIRS 20
/*
 * The city's in groups of a block, 2 x 2 towers (the last column's blocks 1 x 2): its boxes are
 * put in the grey box's list a block at a time (grey_boxes_work_out), so a block is a run of them.
 * A row of the plan as one group (7 towers across) was almost never wholly out of view, and with 84
 * towers every one was tested every frame: 83% of a frame (play-test 2026-11-09)
 */
#define GREY_BLOCK_COLUMNS ((CITY_COLUMNS + 1) >> 1)
#define GREY_BLOCK_ROWS (CITY_ROWS >> 1)
#define GREY_GROUPS (2 + GREY_BLOCK_COLUMNS * GREY_BLOCK_ROWS)
typedef struct
{
	unsigned char first, last; // its boxes, first to last
	frustum_point middle, half; // the box round them all: its middle, and half its size each way
	short lowest;				// its lowest box's top (the city's blocks as hiders: grey_boxes_add)
} grey_group;
static world_rect grey_corridor_floor; // the corridor's floor, for grey_side_bottom
static unsigned char grey_city_order[CITY_TOWERS]; // the city's towers (city.h's index) in block order
static int grey_boxes_for = -1; // the corridor they're for: its wall and side (-1: none yet)

/*
 * The corridor's meshes and the grey box's, in Work RAM's memory (scene_memory.h): taken as the
 * breach starts, given back when the arena starts again. NULL meanwhile: nothing's drawn
 */
// The boxes' faces, sides and tops, each cut to the view: most a quad, some up to 7 (draw_corridor_boxes)
#define GREY_POINTS_MAX 1000 // 640 and 288 with the city's first 21 towers; it's 84 now (city.h)
#define GREY_QUADS_MAX 448
typedef struct
{
	jo_pos3D_fixed points[CORRIDOR_POINTS_MAX]; // the glass floor's
	jo_3d_polygon polygons[CORRIDOR_QUADS_MAX];
	jo_3d_polygon_attribute attributes[CORRIDOR_QUADS_MAX];
	jo_pos3D_fixed grey_points[GREY_POINTS_MAX]; // the boxes' faces (draw_corridor_boxes)
	jo_3d_polygon grey_polygons[GREY_QUADS_MAX];
	jo_3d_polygon_attribute grey_attributes[GREY_QUADS_MAX];
	grey_box boxes[GREY_BOXES];
	grey_group groups[GREY_GROUPS];
	unsigned char block_first[GREY_GROUPS]; // each group's first box (the city's: by block)
	frustum_point stairs_ramp[4], stairs_side[4]; // the stairs far off: grey_stairs_ramp
	short stairs_open_line, stairs_open_out; // their open side: its line (x or y), and outside it, +1 or -1
	bool stairs_open_along_x;				 // the open side runs along x (it's at y = line)
} corridor_memory;
static corridor_memory *corridor_meshes = JO_NULL;
// The arrays by their names, as when they were static
#define corridor_points (corridor_meshes->points)
#define corridor_polygons (corridor_meshes->polygons)
#define corridor_attributes (corridor_meshes->attributes)
#define grey_mesh_points (corridor_meshes->grey_points)
#define grey_mesh_polygons (corridor_meshes->grey_polygons)
#define grey_mesh_attributes (corridor_meshes->grey_attributes)
#define grey_boxes (corridor_meshes->boxes)
#define grey_groups (corridor_meshes->groups)
#define grey_block_first (corridor_meshes->block_first)
#define grey_stairs_ramp_corners (corridor_meshes->stairs_ramp)
#define grey_stairs_side_corners (corridor_meshes->stairs_side)
#define grey_stairs_open_line (corridor_meshes->stairs_open_line)
#define grey_stairs_open_out (corridor_meshes->stairs_open_out)
#define grey_stairs_open_along_x (corridor_meshes->stairs_open_along_x)

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
		corridor_polygons[q].normal.x = JO_FIXED_0;
		corridor_polygons[q].normal.y = JO_FIXED_0;
		corridor_polygons[q].normal.z = JO_FIXED_1;
	}
	corridor_mesh.data.points = corridor_points;
	corridor_mesh.data.polygons = corridor_polygons;
	corridor_mesh.data.attributes = corridor_attributes;
	corridor_mesh.data.point_count = 0;
	corridor_mesh.data.polygon_count = 0;
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

static frustum_point grey_point(world_point p, int z); // below

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
	// The city's towers a block at a time (2 x 2), each block's first box noted: a group each
	{
		int i = CORRIDOR_BOXES, g = 2;

		grey_block_first[0] = 0;
		grey_block_first[1] = GREY_STAIRS;
		for (int block_row = 0; block_row < GREY_BLOCK_ROWS; ++block_row)
			for (int block_column = 0; block_column < GREY_BLOCK_COLUMNS; ++block_column)
			{
				grey_block_first[g++] = (unsigned char)i;
				for (int row = block_row * 2; row < block_row * 2 + 2; ++row)
					for (int column = block_column * 2; column < block_column * 2 + 2 && column < CITY_COLUMNS; ++column)
						grey_city_order[i++ - CORRIDOR_BOXES] = (unsigned char)(row * CITY_COLUMNS + column);
			}
	}
	for (int i = 0; i < GREY_BOXES; ++i)
	{
		world_point a, b;
		world_rect *r = &grey_boxes[i].rect;

		if (i < CORRIDOR_BOXES)
		{
			corridor_box_corners(passage, &corridor_boxes[i], &a, &b);
			grey_boxes[i].height = corridor_boxes[i].height;
			grey_boxes[i].floor = corridor_boxes[i].floor; // a bar's above the floor
			grey_boxes[i].lit = false;
		}
		else
		{
			const city_tower *t = &city_towers[grey_city_order[i - CORRIDOR_BOXES]];

			city_tower_corners(passage, t, &a, &b);
			grey_boxes[i].height = t->height;
			grey_boxes[i].floor = CITY_GROUND;
			grey_boxes[i].lit = t->kind == CITY_HUB;
		}
		r->x_min = JO_MIN(a.x, b.x);
		r->x_max = JO_MAX(a.x, b.x);
		r->y_min = JO_MIN(a.y, b.y);
		r->y_max = JO_MAX(a.y, b.y);
		grey_boxes[i].centre.x = (short)((r->x_min + r->x_max) >> 1);
		grey_boxes[i].centre.y = (short)((r->y_min + r->y_max) >> 1);
		grey_boxes[i].middle = (short)((grey_boxes[i].height + grey_boxes[i].floor) >> 1);
	}
	// The stairs as a ramp (grey_stairs_ramp): its slope through the steps' front edges, from the
	// first step's top (8) up to the platform (160), and its side's profile on the open side (the
	// other's against the corridor's wall, its back against the platform: never seen)
	{
		const corridor_box *first = &corridor_boxes[0], *last = &corridor_boxes[GREY_STAIRS - 1];
		const int width = passage->across_max - passage->across_min;
		const int wall_side = passage->across_min + ((width * first->across_min) >> 4);
		const int open_side = passage->across_min + ((width * first->across_max) >> 4);
		const world_point front_wall = corridor_point(passage, first->out_min, wall_side);
		const world_point front_open = corridor_point(passage, first->out_min, open_side);
		const world_point back_wall = corridor_point(passage, last->out_max, wall_side);
		const world_point back_open = corridor_point(passage, last->out_max, open_side);

		grey_stairs_ramp_corners[0] = grey_point(front_wall, first->height);
		grey_stairs_ramp_corners[1] = grey_point(front_open, first->height);
		grey_stairs_ramp_corners[2] = grey_point(back_open, last->height);
		grey_stairs_ramp_corners[3] = grey_point(back_wall, last->height);
		grey_stairs_side_corners[0] = grey_point(front_open, 0);
		grey_stairs_side_corners[1] = grey_point(back_open, 0);
		grey_stairs_side_corners[2] = grey_point(back_open, last->height);
		grey_stairs_side_corners[3] = grey_point(front_open, first->height);
		// Which line the open side's on, and which way's outside it (away from the wall side)
		grey_stairs_open_along_x = front_open.y == back_open.y;
		grey_stairs_open_line = grey_stairs_open_along_x ? front_open.y : front_open.x;
		grey_stairs_open_out = (short)((grey_stairs_open_along_x ? front_open.y - front_wall.y : front_open.x - front_wall.x) > 0 ? 1 : -1);
	}
	for (int g = 0; g < GREY_GROUPS; ++g)
	{
		grey_group *group = &grey_groups[g];
		int x_min = 32000, x_max = -32000, y_min = 32000, y_max = -32000, z_min = 32000, z_max = -32000;

		group->first = grey_block_first[g];
		group->last = (unsigned char)((g + 1 < GREY_GROUPS ? grey_block_first[g + 1] : GREY_BOXES) - 1);
		for (int i = group->first; i <= group->last; ++i)
		{
			const grey_box *b = &grey_boxes[i];

			x_min = JO_MIN(x_min, b->rect.x_min);
			x_max = JO_MAX(x_max, b->rect.x_max);
			y_min = JO_MIN(y_min, b->rect.y_min);
			y_max = JO_MAX(y_max, b->rect.y_max);
			z_min = JO_MIN(z_min, b->floor);
			z_max = JO_MAX(z_max, b->height);
		}
		group->middle.x = (x_min + x_max) >> 1;
		group->middle.y = (y_min + y_max) >> 1;
		group->middle.z = (z_min + z_max) >> 1;
		group->half.x = ((x_max - x_min) >> 1) + 1;
		group->half.y = ((y_max - y_min) >> 1) + 1;
		group->half.z = ((z_max - z_min) >> 1) + 1;
		group->lowest = 32000;
		for (int i = group->first; i <= group->last; ++i)
			if (grey_boxes[i].height < group->lowest)
				group->lowest = grey_boxes[i].height;
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
 * This frame's view on foot (view_frustum, game/frustum.h): the camera's planes, near and the
 * screen's edges, and the doorway's (below). Every box's sides and top are cut to them in 3D, as
 * one polygon each: no floor-plan cones, no caps, no columns added where the screen's bottom
 * crosses, nothing that depends on which way you look. Off (level_view_on false) in the other
 * views: then everything's drawn uncut, as the iso view always was
 */
static frustum level_view;
static bool level_view_on = false;

/*
 * The corridor's open end (city.h): the right half of its far end, a doorway onto Work RAM's city.
 * Two rooms joined by a portal, as a game with doors does it: from one room, the other's only
 * drawn where it's seen through the opening. The opening's three edges (its two sides, and its
 * sill: the corridor floor's far edge) are each a plane from the eye (frustum_add_edge), added to
 * this frame's view:
 *   - inside the corridor, the city's towers are cut to them. Nothing of the city can be drawn over
 *     the corridor's walls, and nothing below the sill: that's under the corridor's floor
 *   - out in the city, the corridor's boxes and floor are cut to them: seen through the doorway,
 *     and only there
 *   - in the doorway itself (GREY_DOORWAY either side of it), neither is cut: the planes from an
 *     eye in the opening are the opening itself
 * The planes are worked out once a frame (a few divisions each); each box is then tested against
 * them like the screen's edges: wholly outside one, it's not drawn at all
 */
#define GREY_DOORWAY 40
typedef struct
{
	bool inside, outside; // the eye in the corridor (the city cut to the doorway), or out in the city
	unsigned planes;	  // the doorway's planes' bits in level_view (0: none this frame)
	// From the city, the planes that can hide the corridor's floor and what stands on it (the
	// rest of planes can't: testing them is wasted). The sill is the floor's own far edge, so all
	// of the floor and everything above it is on its inside, while the eye's above the floor. The
	// right side is the corridor's side wall: it hides nothing unless you stand beyond that wall
	unsigned floor_planes;
	int eye_across; // the eye's across the corridor, as corridor_point's across
} grey_portal;
static grey_portal portal;
static int level_eye_out = 0; // how far out along the corridor the eye is this frame (on foot)
static int level_eye_across = 0; // and how far across, from its middle line (as city.h's towers)

/*
 * Deep in the city: on foot, past the doorway, and either well out from it (CITY_DEEP_OUT: off the
 * entry's block) or below the corridor's floor (CITY_DEEP_BELOW). From there the arena and the
 * corridor are far behind you and over you, mostly hidden by towers, a few pixels each: but they
 * were still worked out piece by piece, the arena's walls 9-26% of a frame and the corridor's
 * 3-12% from the CD block's hub (2026-11-13). On foot the arena's a pyramid of plain faces (arena_pyramid_add); deep, the corridor's walls are too (
 * corridor_walls_add), and the glass over the hole isn't drawn
 */
#define CITY_DEEP_OUT 1200
#define CITY_DEEP_BELOW (-200)
static bool city_deep(void)
{
	return level_view_on && view_is_on_foot() && portal.outside &&
		   (level_eye_out > CORRIDOR_LENGTH + CITY_DEEP_OUT || level_view.eye.z < CITY_DEEP_BELOW);
}

bool level_in_city(void)
{
	// Last frame's (level_view_begin sets these up as the frame's drawn): a frame late at the doorway
	return level_view_on && view_is_on_foot() && portal.outside;
}

static frustum_point grey_point(world_point p, int z)
{
	frustum_point f;

	f.x = p.x;
	f.y = p.y;
	f.z = z;
	return f;
}

static void grey_portal_set(const corridor *passage)
{
	const int middle = (passage->across_min + passage->across_max) >> 1;
	// The doorway's two sides, standing on the sill, and a point in the opening: the planes' insides
	const world_point left = corridor_point(passage, CORRIDOR_LENGTH, middle);
	const world_point right = corridor_point(passage, CORRIDOR_LENGTH, passage->across_max);
	const world_point opening = corridor_point(passage, CORRIDOR_LENGTH, (middle + passage->across_max) >> 1);
	const frustum_point in = grey_point(opening, 100);
	// The eye's across the corridor: corridor_point puts across straight into x or y, whichever
	// the corridor's across runs along (the other changes with out)
	const world_point origin = corridor_point(passage, 0, 0);
	const bool across_x = corridor_point(passage, 0, 1).x != origin.x;
	unsigned left_plane, right_plane, sill_plane;
	int out, eye_across;

	portal.inside = portal.outside = false;
	portal.planes = portal.floor_planes = 0;
	level_eye_out = 0;
	if (!level_view_on)
		return;
	{
		world_point eye;

		eye.x = (short)level_view.eye.x;
		eye.y = (short)level_view.eye.y;
		out = corridor_out(passage, eye);
		eye_across = across_x ? eye.x : eye.y;
		portal.eye_across = eye_across;
	}
	level_eye_out = out;
	level_eye_across = eye_across - ((passage->across_min + passage->across_max) >> 1);
	portal.outside = out > CORRIDOR_LENGTH + GREY_DOORWAY;
	portal.inside = out < CORRIDOR_LENGTH - GREY_DOORWAY;
	/*
	 * Between the two you're in the doorway: neither (nothing's cut to it, and the city's own
	 * things, its board, its ring of skyline, the roofs' tables, wait until you're through). But
	 * that's a band right across the city, GREY_DOORWAY either side of the doorway's line: the
	 * roofs by the CD block's hub reach it, 2900 to the side of the corridor, and stood on their
	 * edge there the board, the towers and the skyline all went, the pyramid's flat slope over
	 * everything (the user, 2026-11-13: "When up close to edge nearest to pyramid from the
	 * elevator, the pyramid walls and sky city clips out"; hub_edge.steps). Off to the side of
	 * the corridor's width you're not in its doorway: you're out in the city
	 */
	if (!portal.inside && !portal.outside &&
		(eye_across < passage->across_min - GREY_DOORWAY || eye_across > passage->across_max + GREY_DOORWAY))
		portal.outside = true;
	if (!portal.inside && !portal.outside)
		return;
	left_plane = frustum_add_edge(&level_view, grey_point(left, 0), grey_point(left, 1000), in);
	right_plane = frustum_add_edge(&level_view, grey_point(right, 0), grey_point(right, 1000), in);
	sill_plane = frustum_add_edge(&level_view, grey_point(left, 0), grey_point(right, 0), in);
	portal.planes = left_plane | right_plane | sill_plane;
	portal.floor_planes = portal.planes;
	if (level_view.eye.z > 0)
		portal.floor_planes &= ~sill_plane;
	if (eye_across <= passage->across_max)
		portal.floor_planes &= ~right_plane;
	/*
	 * From above the corridor's walls (up the CD block's lift, a tall roof), you look in over
	 * them: the doorway hides none of the floor then. Cut to it still, the floor went and what
	 * stands on it stayed (the user, 2026-11-13: "When at elevator, looking back up at the yard
	 * section, there is no floor in the yard")
	 */
	if (level_view.eye.z > CORRIDOR_TOP_ON_FOOT)
		portal.floor_planes = 0;
}

/*
 * The mirror (play-test 2026-11-09): looking back from the corridor, the hole you broke through is a
 * mirror, and you see yourself in it: as a person now, where riding you were only energy (the
 * cube). It's the standard way: the corridor drawn a second time, reflected in the glass's plane.
 *   - Drawing the world reflected is the same as cutting the world as it is to the view of your eye
 *     reflected in the glass, then reflecting what's left (game/frustum.h, frustum_reflect). So
 *     the second pass is the first's code (the floor, the boxes, the walls) run with level_view
 *     reflected, inside a matrix that reflects the world (level_mirror_matrix)
 *   - It's cut to the glass: its outline's four edges, seen from the reflected eye, take the place
 *     of the screen's four edges (planes 1 .. 4). Everything that's cut by the screen is then cut
 *     by the glass instead, with no change to the code that draws it. The guard band stays
 *   - Everything reflected is beyond the glass, further than anything in front of it: VDP1 draws
 *     far to near, so the reflection lands behind what's this side of it, with no depth buffer
 *   - It's tinted darker and cooler (mirror_colour: no cost to VDP1), so it reads as glass
 * Only from inside the corridor, with the glass in view: from the city it's far and small, and
 * the dark glass (add_corridor_glass) stands in for it. The arena's inside is already hidden below
 * the glass's top (arena_culling_begin), so nothing of it is drawn where the reflection goes
 */
#define MIRROR_SHADE 3 // 3 of a channel's 31 steps darker (blue: none): the corridor's already dark,
					   // and a darker glass hid it (5 was near black)
#define MIRROR_NEAR 20 // nearer the glass than this, no reflection (you're at the glass)
// How far out from the glass the reflection's drawn: the start, its rail and duct (corridor.c), and
// no further. Beyond, a near-black wall across the corridor (mirror_far_wall), as if the glass
// darkened with depth. All of it, the stairs, the tower and the city through the far doorway, took
// 41% of a frame near the glass (cpu 104%, 57 fps)
#define MIRROR_DEPTH 2300
/*
 * And less near the glass: three times your distance from it, from MIRROR_DEPTH_NEAR. Walking up to
 * it, the pane fills the screen and the reflection's a second whole view of the corridor, as deep
 * as the first: within ~400 of it the frame took 34-44 fps, the mirror 41-57% (mi), VDP1 not
 * finishing (a black frame now and then: the user, 2026-11-14, "the whole screen glitches out";
 * mirror_slow.steps). Up close the glass shows you and the floor at your feet; past the depth, the
 * dark wall (mirror_far_wall), which moves in with it. A multiply and two compares, once a frame
 */
#define MIRROR_DEPTH_NEAR 700
static int mirror_depth = MIRROR_DEPTH; // this frame's (mirror_decide)
static bool mirror_on = false; // this frame: the glass is in view from the corridor
/*
 * Once you've slid under the duct (corridor.c: the first thing you go under, out 2050 to 2110),
 * the mirror's gone for good: it's a second pass over the corridor, up to 41% of a frame, and from
 * there on it's behind the duct and the rail, a few pixels of it left. The dark glass
 * (add_corridor_glass) stands in for it, as from the city. Till you're next on the cycle
 */
#define MIRROR_GONE_OUT 2110
static bool mirror_gone = false;
static int mirror_axis;		   // 0: the corridor runs out along x, 1: along y
static int mirror_at;		   // the glass's plane: that coordinate
static int mirror_shade = 0;   // drawing the reflection: how much darker (0: not)
static frustum mirror_saved;   // level_view, while the reflection's drawn
static grey_portal mirror_saved_portal;
static int mirror_across_min, mirror_across_max; // the corridor's width, while the mirror's on

// The glass's four corners in order round it (floor to the corridor's walls' top, at out 0), and
// a point inside it
static void mirror_pane(const corridor *passage, frustum_point *pane, frustum_point *centre)
{
	const world_point a = corridor_point(passage, 0, passage->across_min);
	const world_point b = corridor_point(passage, 0, passage->across_max);

	pane[0] = grey_point(a, WALL_BOTTOM_Z);
	pane[1] = grey_point(b, WALL_BOTTOM_Z);
	pane[2] = grey_point(b, CORRIDOR_TOP_ON_FOOT);
	pane[3] = grey_point(a, CORRIDOR_TOP_ON_FOOT);
	centre->x = (a.x + b.x) >> 1;
	centre->y = (a.y + b.y) >> 1;
	centre->z = CORRIDOR_TOP_ON_FOOT >> 1;
}

// A flat colour as the glass shows it: red and green MIRROR_SHADE darker, blue as it is:
// darker and cooler (RGB555: red in bits 0 .. 4, green 5 .. 9, blue 10 .. 14)
static jo_color mirror_colour(jo_color colour)
{
	int r, g, b;

	if (mirror_shade == 0)
		return colour;
	r = (colour & 31) - mirror_shade;
	g = ((colour >> 5) & 31) - mirror_shade;
	b = (colour >> 10) & 31;
	r = r < 0 ? 0 : r;
	g = g < 0 ? 0 : g;
	b = b < 1 ? 1 : b; // never all 0 with bit 15: still not see-through
	return (jo_color)(0x8000 | (b << 10) | (g << 5) | r);
}

static void mirror_decide(const corridor *passage)
{
	const world_point origin = corridor_point(passage, 0, 0);
	frustum_point pane[4], centre;
	world_point eye;
	int out;

	mirror_on = false;
	if (!level_view_on || !view_is_on_foot())
	{
		mirror_gone = false; // a new time on foot: it's there again
		return;
	}
	eye.x = (short)level_view.eye.x;
	eye.y = (short)level_view.eye.y;
	out = corridor_out(passage, eye);
	if (out > MIRROR_GONE_OUT)
		mirror_gone = true;
	if (mirror_gone || out < MIRROR_NEAR || out > CORRIDOR_LENGTH)
		return; // not in the corridor, or past the duct
	mirror_axis = corridor_point(passage, 1, 0).x != origin.x ? 0 : 1;
	mirror_at = mirror_axis == 0 ? origin.x : origin.y;
	mirror_across_min = passage->across_min;
	mirror_across_max = passage->across_max;
	mirror_pane(passage, pane, &centre);
	mirror_on = frustum_classify(&level_view, pane, 4, FRUSTUM_SCREEN) != FRUSTUM_OUTSIDE;
	mirror_depth = out * 3;
	mirror_depth = mirror_depth < MIRROR_DEPTH_NEAR ? MIRROR_DEPTH_NEAR : mirror_depth > MIRROR_DEPTH ? MIRROR_DEPTH : mirror_depth;
}

/*
 * Whether a piece of the arena's wall, column a to b, stands in the glass: in its plane, and wholly
 * within the corridor's width. With the mirror drawn there it isn't drawn: the shattered glass was
 * 1 nearer and hid it, but the mirror's reflection is beyond the plane, and a slice of the wall at
 * the hole's edge showed in front of it, a striped post (play-test 2026-11-09). Wholly within, not
 * touching: arena_piece_hidden's runs are halved on the rule that a part of a hidden box is hidden
 */

static bool mirror_in_the_glass(world_point a, world_point b)
{
	const int along_a = mirror_axis == 0 ? a.x : a.y, along_b = mirror_axis == 0 ? b.x : b.y;
	const int across_a = mirror_axis == 0 ? a.y : a.x, across_b = mirror_axis == 0 ? b.y : b.x;
	const int low = across_a < across_b ? across_a : across_b, high = across_a < across_b ? across_b : across_a;

	return mirror_on && along_a == mirror_at && along_b == mirror_at && low >= mirror_across_min - 2 &&
		   high <= mirror_across_max + 2;
}

void level_view_begin(const corridor *passage, bool free_camera)
{
	level_view_on = view_frustum(free_camera, &level_view);
	portal.inside = portal.outside = false;
	portal.planes = 0;
	mirror_on = false;
	if (passage == JO_NULL || !passage->open || corridor_meshes == JO_NULL)
		return;
	grey_boxes_work_out(passage);
	grey_portal_set(passage);
	mirror_decide(passage);
}

bool level_mirror_begin(const corridor *passage, const wall_clip_camera *camera, wall_clip_camera *mirrored)
{
	frustum m;
	frustum_plane guard[4];
	frustum_point pane[4], centre;

	if (!mirror_on || camera == JO_NULL)
		return false;
	frustum_reflect(&level_view, mirror_axis, mirror_at, &m);
	/*
	 * The glass's edges in place of the screen's (1 .. 4), the guard band's (5 .. 8) kept. The
	 * doorway's planes (from 9) were the real eye's: grey_portal_set adds the reflected eye's.
	 * But each side only where the glass's edge is inside the screen's: near the glass it's far
	 * bigger than the screen, and the reflection cut to its edges ran far off the screen, VDP1
	 * walking pixels nobody sees and its corners out of its 11-bit range: a garbled or black frame
	 * walking up to it (the user, 2026-11-14: "in some cases, the whole screen glitches out";
	 * mirror_slow.steps, 30 fps at 236 from it). So each glass edge is paired with the screen
	 * edge it faces most nearly (its normal's nearest), and replaces it only if the glass edge's
	 * two corners are inside that screen edge; else the screen's edge is the tighter, and stays
	 */
	for (int k = 0; k < 4; ++k)
		guard[k] = m.planes[5 + k];
	mirror_pane(passage, pane, &centre);
	{
		frustum glass = m; // the glass's four edges, worked out on their own
		bool taken[5] = {false, false, false, false, false};

		glass.count = 1;
		for (int k = 0; k < 4; ++k)
			if (frustum_add_edge(&glass, pane[k], pane[(k + 1) & 3], centre) == 0)
				return false; // the reflected eye's on an edge's line: no reflection this frame
		for (int k = 0; k < 4; ++k)
		{
			const frustum_plane *g = &glass.planes[1 + k];
			int side = 1, best = -0x7FFFFFFF;

			for (int j = 1; j <= 4; ++j)
			{
				const int facing = g->x * m.planes[j].x + g->y * m.planes[j].y + g->z * m.planes[j].z;

				if (facing > best && !taken[j])
				{
					best = facing;
					side = j;
				}
			}
			taken[side] = true;
			if (frustum_distance(&m, side, pane[k]) >= 0 || frustum_distance(&m, side, pane[(k + 1) & 3]) >= 0)
				m.planes[side] = *g; // the glass's edge is the tighter there (or crosses the screen's)
		}
	}
	for (int k = 0; k < 4; ++k)
		m.planes[5 + k] = guard[k];
	m.count = FRUSTUM_PORTAL_FIRST;
	// The near plane's no use to the reflected eye (everything's well in front of it): in its place,
	// the reflection's depth, out up to MIRROR_DEPTH. Every cut has the near plane (planes 0 ..), so
	// everything's cut to it with no change to the code. Its normal points back down the corridor
	// (-dir along the axis, dir the way out goes); it's 0 at out = MIRROR_DEPTH, measured from the eye
	{
		const world_point origin = corridor_point(passage, 0, 0), one = corridor_point(passage, 1, 0);
		const int dir = mirror_axis == 0 ? one.x - origin.x : one.y - origin.y; // 1 or -1
		const int limit = mirror_at + dir * mirror_depth;
		const int eye = mirror_axis == 0 ? m.eye.x : m.eye.y;

		m.planes[FRUSTUM_NEAR].x = mirror_axis == 0 ? -dir * FRUSTUM_ONE : 0;
		m.planes[FRUSTUM_NEAR].y = mirror_axis == 1 ? -dir * FRUSTUM_ONE : 0;
		m.planes[FRUSTUM_NEAR].z = 0;
		m.planes[FRUSTUM_NEAR].offset = -dir * FRUSTUM_ONE * (limit - eye);
	}
	mirror_saved = level_view;
	mirror_saved_portal = portal;
	level_view = m;
	grey_portal_set(passage);
	*mirrored = *camera;
	if (mirror_axis == 0)
	{
		mirrored->eye.x = (short)(2 * mirror_at - camera->eye.x);
		mirrored->forward_x = -camera->forward_x;
	}
	else
	{
		mirrored->eye.y = (short)(2 * mirror_at - camera->eye.y);
		mirrored->forward_y = -camera->forward_y;
	}
	if (object_fog_enabled)
		object_fog_set_camera(mirrored->eye, mirrored->forward_x, mirrored->forward_y);
	mirror_shade = MIRROR_SHADE;
	return true;
}

static void mirror_far_wall(const corridor *passage); // below

void level_mirror_draw(const corridor *passage, const wall_clip_camera *mirrored)
{
	draw_corridor_floor(passage, mirrored);
	draw_corridor_boxes(passage, mirrored);
	mirror_far_wall(passage);
}

void level_mirror_matrix(void)
{
	// A point p goes to 2 x mirror_at - p on the axis: moved so the glass is at 0, flipped, moved back
	if (mirror_axis == 0)
	{
		jo_3d_translate_matrix(mirror_at, 0, 0);
		matrix_scale(-JO_FIXED_1, JO_FIXED_1, JO_FIXED_1);
		jo_3d_translate_matrix(-mirror_at, 0, 0);
	}
	else
	{
		jo_3d_translate_matrix(0, mirror_at, 0);
		matrix_scale(JO_FIXED_1, -JO_FIXED_1, JO_FIXED_1);
		jo_3d_translate_matrix(0, -mirror_at, 0);
	}
}


/*
 * The broken glass over the hole you came through (tools/make_glass.py): looking back from the
 * corridor, the breach is a pane of dark glass, shattered from where you hit it. A mirror would be
 * better, but the Saturn has no reflections: it would be the corridor drawn twice. Four textures
 * side by side, one a 250 piece, so the cracks run on across the joins; a narrower hole (3
 * pieces) uses the middle ones. Drawn as a wall: the same cuts near you
 */
#define GLASS_PIECES 4
static jo_3d_polygon_attribute glass_attributes[GLASS_PIECES];
static bool glass_loaded = false;

void load_corridor_glass(void)
{
	static const char *const names[GLASS_PIECES] = {"GLASS0.TGA", "GLASS1.TGA", "GLASS2.TGA", "GLASS3.TGA"};
	jo_3d_mesh setter; // jo's texture setter works on a mesh: pointed at one attribute at a time

	if (glass_loaded)
		return;
	setter.data.polygon_count = 1;
	for (int k = 0; k < GLASS_PIECES; ++k)
	{
		const int id = jo_sprite_add_tga(JO_ROOT_DIR, names[k], JO_COLOR_Transparent);

		setter.data.attributes = &glass_attributes[k];
		// As init_attributes: jo_3d_create_plane's defaults, so jo's setter behaves the same
		JO_ZERO(glass_attributes[k].sort);
		JO_ZERO(glass_attributes[k].draw_mode);
		JO_ZERO(glass_attributes[k].gouraud_table);
		glass_attributes[k].plane = JO_3D_DUAL_PLANE;
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

	if (!glass_loaded || corridor_out(passage, *eye) <= 0 || mirror_on)
		return; // not loaded yet, or you're not in the corridor: nothing to look back at. Or the
				// mirror's drawn there instead (level_mirror_begin)
	// Nor from deep in the city: it's at the corridor's other end, behind its far wall and under
	// its floor from down there. It's drawn with the walls' bands, not cut to the doorway, so it
	// showed in the sky from below the yard (seen in play, 2026-11-13)
	if (city_deep())
		return;
	for (int k = 0; k <= pieces; ++k)
		columns[k] = corridor_point(passage, 1, passage->across_min + 250 * k);
	for (int k = 0; k < pieces; ++k)
	{
		const int first = block_high.mesh.data.point_count;

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
	jo_3d_polygon_attribute base;
	jo_3d_polygon_attribute body;
	const int top = (WALL_TOP_Z * ((rise * CORRIDOR_WALL_RISE) >> 8)) >> 8;

	const int corridor_frc = jo_time_get_frc(); // the debug line's "co" (utils/perf.h)

	if (level_view_on && view_is_on_foot())
	{
		// On foot, the walls are cut to the view in 3D with the boxes (corridor_walls_add). The glass
		// behind you is textured: a cut would squash its texture, so it keeps the floor-plan cuts
		if (!cutaway)
		{
			corridor_cone = true;
			add_corridor_glass(passage, eye, camera, top);
			corridor_cone = false;
		}
		perf_part_add(PERF_CORRIDOR, corridor_frc);
		return;
	}
	set_corridor_wall_attribute(&base, corridor_pulse(frame++));
	set_corridor_wall_attribute(&body, CORRIDOR_BODY);
	// Sorted by their furthest corner, not their middle: the corridor's walls bound it, so from
	// inside they're behind everything in it. By the middle, a wall piece (250 long) beside the
	// stairs came out nearer than the steps' fronts, and was drawn over their ends: a notch cut
	// out of the stairs where they meet the wall. (SGL's sort is the low 2 bits: SKILLS.md)
	base.sort = (unsigned char)((base.sort & ~3) | JO_3D_SORT_MAX);
	body.sort = (unsigned char)((body.sort & ~3) | JO_3D_SORT_MAX);
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
		const int low_point = block_low.mesh.data.point_count;
		const int low_polygon = block_low.mesh.data.polygon_count;
		const int high_polygon = block_high.mesh.data.polygon_count;

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
				first = block_high.mesh.data.point_count; // the last band's are the ones capped
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
}

/*
 * On foot, the arena's walls that can't be seen aren't drawn (game/occluder.h). Looking down the
 * corridor from the city, all of them were drawn behind the corridor's, unseen: 65 polygons, ~22% of
 * the frame, and 30 fps. Two reasons a piece can't be seen, both worked out once a frame:
 *   - it's wholly behind one of the corridor's walls (its two sides, the far end's left half: as
 *     drawn on foot, both faces, WALL_BOTTOM_Z to CORRIDOR_TOP_ON_FOOT): arena_hiders
 *   - it's the square's inside, the eye's outside it, and lower than the walls' tops: every ray
 *     from the eye to it crosses a wall at a height under the tops, so a wall's in the way. All
 *     but where the wall's open: the hole you broke through (the flicker's section, all its pieces
 *     gone), above the glass over its bottom. So the inside is drawn only where it's seen through
 *     that, the doorway's trick again: arena_hole
 * An outer side's face seen from outside isn't the inside: it's only hidden behind the corridor.
 * Not in the iso view, the debug camera or riding (level_view off, or the eye inside the square)
 */
static occluder arena_hiders[CORRIDOR_FACES];
static occluder arena_hole;
static bool arena_culling = false;		 // on foot this frame: the rest is set
static bool arena_inside_hidden = false; // the eye's outside the square and under the walls' tops
static bool arena_hole_open = false;	 // and the hole's open: the inside's seen through arena_hole

static void arena_culling_begin(const corridor *passage, const wall_flicker *flicker, int rise)
{
	const frustum_point e = level_view.eye;
	const int half = arena_half();
	const int top = (WALL_TOP_Z * rise) >> 8;

	arena_culling = false;
	if (!level_view_on || !view_is_on_foot())
		return;
	arena_culling = true;
	arena_inside_hidden = (e.x > half || e.x < -half || e.y > half || e.y < -half) && e.z < top;
	arena_hole_open = false;
	if (arena_inside_hidden && flicker != JO_NULL && flicker->count > 0 && flicker->hidden != 0)
	{
		const wall_side *side = &sides[flicker->side];

		// The glass over the hole (add_corridor_glass) is as wide as it (the corridor is the hole's
		// width, whole pieces) and as tall as the corridor's walls: the inside's seen only above it.
		// From the city, the far side's top is below the glass's (it's 16000 further): hidden
		const int glass_top = (WALL_TOP_Z * ((rise * CORRIDOR_WALL_RISE) >> 8)) >> 8;

		// The whole section, even if only some of its pieces are out: a little more than the hole
		occluder_set(&arena_hole, e, side->along_x, side->fixed * half, flicker->along_min,
					 wall_flicker_along_max(flicker), glass_loaded ? glass_top : WALL_BOTTOM_Z, top);
		arena_hole_open = true;
	}
	if (passage != JO_NULL && passage->open)
	{
		world_point from[CORRIDOR_FACES], to[CORRIDOR_FACES];

		corridor_faces(passage, from, to);
		for (int i = 0; i < CORRIDOR_FACES; ++i)
		{
			const bool along_x = from[i].y == to[i].y;

			occluder_set(&arena_hiders[i], e, along_x, along_x ? from[i].y : from[i].x, along_x ? from[i].x : from[i].y,
						 along_x ? to[i].x : to[i].y, WALL_BOTTOM_Z, CORRIDOR_TOP_ON_FOOT);
		}
	}
	else
		for (int i = 0; i < CORRIDOR_FACES; ++i)
			arena_hiders[i].on = false;
}

// Whether the piece of wall from column a to b, z_bottom to z_top, can't be seen (above). inside:
// it's the square's inside
static bool arena_piece_hidden(world_point a, world_point b, int z_bottom, int z_top, bool inside)
{
	frustum_point centre, half;

	if (!arena_culling)
		return false;
	if (mirror_in_the_glass(a, b))
		return true;
	centre.x = (a.x + b.x) >> 1;
	centre.y = (a.y + b.y) >> 1;
	centre.z = (z_bottom + z_top) >> 1;
	// Rounded down, a half could be a unit short: one more keeps it the whole piece or more
	half.x = ((a.x > b.x ? a.x - b.x : b.x - a.x) >> 1) + 1;
	half.y = ((a.y > b.y ? a.y - b.y : b.y - a.y) >> 1) + 1;
	half.z = ((z_top - z_bottom) >> 1) + 1;
	if (inside && arena_inside_hidden && (!arena_hole_open || !occluder_shows(&arena_hole, centre, half)))
		return true;
	for (int i = 0; i < CORRIDOR_FACES; ++i)
		if (occluder_hides(&arena_hiders[i], centre, half))
			return true;
	return false;
}

static void arena_pyramid_add(const corridor *passage); // below, with the faces it's made of

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
	arena_culling_begin(passage, flicker, rise);
	if (level_view_on && view_is_on_foot() && passage != JO_NULL && passage->open)
	{
		// On foot: the pyramid's faces, not the arena's walls piece by piece (arena_pyramid_add)
		arena_pyramid_add(passage);
		strip_seen = STRIP_SEEN_ALL;
		perf_part_add(PERF_ARENA, frc);
		if (passage != JO_NULL && passage->open)
			draw_corridor_walls(passage, view, eye, camera, cutaway, rise);
		return;
	}
	for (int s = 0; s < 4; ++s)
	{
		// From outside, a side's outer face is seen if the eye's beyond its line; else it's its
		// inner face, the square's inside
		const int eye_across = sides[s].along_x ? level_view.eye.y : level_view.eye.x;

		if (arena_culling && camera != JO_NULL && !cutaway)
			strip_seen = eye_across * sides[s].fixed > arena_half() ? STRIP_OUTSIDE : STRIP_INSIDE;
		// Cutaway: the south and west walls stand between the iso camera (which sits on the +x, +y
		// side of the cycle) and the cycle. Full height they'd hide it, and their tops would rise
		// behind the camera. Just the glowing base, lower than the cycle, still marks the edge
		draw_side(&sides[s], s, view, eye, camera, cutaway && sides[s].fixed > 0, rise, flicker);
	}
	if (arena_culling && camera != JO_NULL && !cutaway)
		strip_seen = STRIP_INSIDE; // the blocks stand inside the square
	draw_blocks(view, eye, camera, cutaway, rise);
	strip_seen = STRIP_SEEN_ALL; // the corridor's walls and glass, below, are all drawn
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
static void set_surround_attribute(jo_3d_polygon_attribute *a, jo_color colour, bool in_front)
{
	a->plane = JO_3D_DUAL_PLANE;
	a->sort = (in_front ? JO_3D_SORT_MIN : JO_3D_SORT_MAX) | ((JO_VDP1_POLYGON >> 16) & 0x1c) | JO_3D_NO_OPTION;
	a->texture_id = JO_3D_NO_TEXTURE;
	a->draw_mode = (JO_VDP1_COLOR_MODE_RGB | JO_VDP1_NO_GOURAUD) | ((JO_VDP1_POLYGON >> 24) & 0xc0);
	a->color = colour;
	a->gouraud_table = 0;
	a->direction = JO_VDP1_POLYGON & 0x3f;
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
		const jo_fixed z = jo_int2fixed(low ? WALL_BASE_TOP_Z : top);
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
			surround_points[points + i].x = jo_int2fixed(corners[i].x);
			surround_points[points + i].y = jo_int2fixed(corners[i].y);
			surround_points[points + i].z = z;
			fog_levels[i] = object_fog_enabled ? (unsigned char)object_fog_level(corners[i].x, corners[i].y) : 0;
		}
		// The fan: (0 1 2 3), (0 3 4 5), (0 5 6 7); past the last corner, the last again
		for (int k = 1; k < n - 1; k += 2)
		{
			jo_3d_polygon *q = &surround_polygons[quads];
			jo_3d_polygon_attribute *a = &surround_attributes[quads];
			int c2 = k + 1;
			int c3 = k + 2 < n ? k + 2 : n - 1;

			q->vertices[0] = (unsigned short)points;
			q->vertices[1] = (unsigned short)(points + k);
			q->vertices[2] = (unsigned short)(points + c2);
			q->vertices[3] = (unsigned short)(points + c3);
			set_surround_attribute(a, colour, in_front);
			if (object_fog_enabled)
				object_fog_set_quad(a, fog_levels[0], fog_levels[k], fog_levels[c2], fog_levels[c3]);
			++quads;
		}
		points += n;
	}
	if (quads == 0)
		return;
	surround_mesh.data.point_count = (unsigned short)points;
	surround_mesh.data.polygon_count = (unsigned short)quads;
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
// (on_top) is JO_3D_SORT_PREVIOUS: drawn straight after the one before, so the tiles always lie over it
// (by their own depths, a tile and the floor under it could sort either way). Returns false when
// there's no room left
// fog: GLASS_FOG_CORNERS for a fog level worked out at each corner (Gouraud: the far glass, a
// long quad), or the one level for all of it (a tile: drawn in that darker colour, no Gouraud.
// Per corner, every tile cost six fog levels and two Gouraud tables written into VDP1 RAM while
// VDP1 was drawing: the floor took a third of the frame down the corridor)
#define GLASS_FOG_CORNERS (-1)
static bool add_glass(const world_point *corners, int n, jo_color colour, int fog, bool on_top, int *points, int *quads)
{
	colour = mirror_colour(colour);
	unsigned char fog_levels[CORRIDOR_CLIP_MAX];

	if (*points + n > CORRIDOR_POINTS_MAX || *quads + (n - 1) / 2 > CORRIDOR_QUADS_MAX)
		return false;
	if (fog > 0)
		colour = object_fog_flat_colour(colour, fog);
	for (int i = 0; i < n; ++i)
	{
		corridor_points[*points + i].x = jo_int2fixed(corners[i].x);
		corridor_points[*points + i].y = jo_int2fixed(corners[i].y);
		corridor_points[*points + i].z = JO_FIXED_0; // on the floor
		if (fog == GLASS_FOG_CORNERS)
			fog_levels[i] = object_fog_enabled ? (unsigned char)object_fog_level(corners[i].x, corners[i].y) : 0;
	}
	for (int k = 1; k < n - 1; k += 2)
	{
		jo_3d_polygon *q = &corridor_polygons[*quads];
		jo_3d_polygon_attribute *a = &corridor_attributes[*quads];
		const int c2 = k + 1;
		const int c3 = k + 2 < n ? k + 2 : n - 1;

		q->vertices[0] = (unsigned short)*points;
		q->vertices[1] = (unsigned short)(*points + k);
		q->vertices[2] = (unsigned short)(*points + c2);
		q->vertices[3] = (unsigned short)(*points + c3);
		set_surround_attribute(a, colour, false);
		if (on_top || k > 1)
			a->sort = (unsigned char)((a->sort & ~3) | JO_3D_SORT_PREVIOUS); // the sort is the low 2 bits (as main.c's cube)
		if (object_fog_enabled && fog == GLASS_FOG_CORNERS)
			object_fog_set_quad(a, fog_levels[0], fog_levels[k], fog_levels[c2], fog_levels[c3]);
		++*quads;
	}
	*points += n;
	return true;
}

/*
 * Drawing the corridor and the city on foot: every face cut to this frame's view (level_view) in
 * 3D, one polygon each, by the same few lines (level_face_add). A face is a convex polygon of up to
 * 8 corners; cut, it has up to FRUSTUM_POLY_MAX, and it's added as a fan of quads (0 1 2 3),
 * (0 3 4 5) ...; past the last corner, the last again. Two meshes, each rewritten every frame (their
 * arrays in Work RAM's memory, corridor_memory): the boxes' and walls' (grey_out), and the glass
 * floor's (glass_out), whose polygons are chained to keep their order (draw_glass_floor)
 */
typedef struct
{
	jo_pos3D_fixed *points;
	jo_3d_polygon *polygons;
	jo_3d_polygon_attribute *attributes;
	int point_count, quad_count;
	int points_max, quads_max;
} level_mesh;
static level_mesh grey_out, glass_out;
static jo_3d_mesh grey_mesh;

static void level_mesh_begin(level_mesh *m, jo_pos3D_fixed *points, jo_3d_polygon *polygons, jo_3d_polygon_attribute *attributes, int points_max,
							 int quads_max)
{
	m->points = points;
	m->polygons = polygons;
	m->attributes = attributes;
	m->point_count = m->quad_count = 0;
	m->points_max = points_max;
	m->quads_max = quads_max;
}

// How a face is fogged: not at all, by its colour (one level for it all, its nearest corner's: no
// Gouraud table written into VDP1 RAM while VDP1 draws), or a level at each corner (Gouraud: long
// faces, the corridor's walls, whose far end is far deeper in the fog than their near one)
#define LEVEL_FOG_NONE 0
#define LEVEL_FOG_FLAT 1
#define LEVEL_FOG_CORNERS 2

/*
 * One face (n corners in order round it) into mesh m, cut by the planes in `planes` (0: none, as
 * it is: level_planes_box or level_planes_rect worked them out), flat colour, sorted by `sort`
 * (JO_3D_SORT_CENTER, JO_3D_SORT_MAX, JO_3D_SORT_PREVIOUS). lie: a side (LB, LT, RT, RB) to be filled with lying lines (level_side_lies): listed from its top left instead, the same way
 * round, before it's cut. The cut keeps the order round, starting where it started, so a wide front
 * cut at the screen's edge still lies. Returns false if nothing of it was added
 */
static bool level_face_add(level_mesh *m, const frustum_point *corners, int n, unsigned planes, jo_color colour, int fog,
						   int sort, bool lie)
{
	colour = mirror_colour(colour); // unchanged but in the mirror
	frustum_point cut[FRUSTUM_POLY_MAX];
	frustum_point turned[4];
	unsigned char levels[FRUSTUM_POLY_MAX];
	const frustum_point *p = corners;
	const int first = m->point_count;

	if (lie)
	{
		for (int k = 0; k < 4; ++k)
			turned[k] = corners[(k + 1) & 3]; // LT, RT, RB, LB
		corners = p = turned;
	}
	if (planes == FRUSTUM_OUTSIDE)
		return false;
	if (planes != 0)
	{
		n = frustum_clip(&level_view, planes, corners, n, cut);
		p = cut;
	}
	if (n < 3 || m->point_count + n > m->points_max || m->quad_count + (n - 1) / 2 > m->quads_max)
		return false; // gone, or no room left: a face far off missing for a frame, not a crash
	if (fog != LEVEL_FOG_NONE && object_fog_enabled)
	{
		int nearest = 255;
		// One level for a flat quad: its two columns' (corners 0 and 2 are one in each, listed
		// upright or lying), the fog going by the distance along the floor
		const int step = fog == LEVEL_FOG_FLAT && n == 4 ? 2 : 1;

		for (int k = 0; k < n; k += step)
		{
			levels[k] = (unsigned char)object_fog_level(p[k].x, p[k].y);
			if (levels[k] < nearest)
				nearest = levels[k];
		}
		if (fog == LEVEL_FOG_FLAT)
			colour = object_fog_flat_colour(colour, nearest);
	}
	else
		fog = LEVEL_FOG_NONE;
	for (int k = 0; k < n; ++k)
	{
		m->points[first + k].x = jo_int2fixed(p[k].x);
		m->points[first + k].y = jo_int2fixed(p[k].y);
		m->points[first + k].z = jo_int2fixed(p[k].z);
	}
	for (int k = 1; k < n - 1; k += 2)
	{
		jo_3d_polygon *q = &m->polygons[m->quad_count];
		jo_3d_polygon_attribute *a = &m->attributes[m->quad_count];
		const int last = k + 2 < n ? k + 2 : n - 1;

		q->normal.x = JO_FIXED_0; // unused: unlit, both faces drawn
		q->normal.y = JO_FIXED_0;
		q->normal.z = JO_FIXED_1;
		q->vertices[0] = (unsigned short)first;
		q->vertices[1] = (unsigned short)(first + k);
		q->vertices[2] = (unsigned short)(first + k + 1);
		q->vertices[3] = (unsigned short)(first + last);
		set_corridor_wall_attribute(a, colour);
		a->sort = (unsigned char)((a->sort & ~3) | sort); // the sort is the low 2 bits (SKILLS.md)
		if (fog == LEVEL_FOG_CORNERS)
			object_fog_set_quad(a, levels[0], levels[k], levels[k + 1], levels[last]);
		++m->quad_count;
	}
	m->point_count += n;
	return true;
}

/*
 * Whether a side, uncut (corners LB, LT, RT, RB), is filled with fewer lines lying than upright
 * (block_band_add, lay_wide_pieces_flat: the same rule, for the same reasons). On the screen a
 * column h tall at depth d is h x F / d pixels (F, 160 at 90 degrees: about the same on foot), and
 * its two ends, s0 and s1 aside, are |s1 / d1 - s0 / d0| x F apart. Times d0 x d1 / F, both:
 * column 0's h x d1, column 1's h x d0, the width |s1 x d0 - s0 x d1|. Lying when it's square on
 * to you (its columns within 5:4), wider than tall, and upright would be over 80 lines (F / 2:
 * the width x 2 over d0 x d1). Depths and asides are along the camera's own directions, so it holds
 * looking up or down too. Only within 4096: the products stay in 32 bits, and further off no front
 * is that big. And only a face across your view: one running along it (the camera's forward along
 * the face over half) has no part square on to you, and isn't measured at all
 */
static bool level_side_lies(const frustum_point *c)
{
	const frustum *v = &level_view;
	const int along = c[0].x != c[3].x ? v->forward[0] : v->forward[1]; // x1024
	int dx0, dy0, dx1, dy1, d0, d1, s0, s1, wide;

	if (along > 512 || along < -512)
		return false;
	dx0 = c[0].x - v->eye.x;
	dy0 = c[0].y - v->eye.y;
	dx1 = c[3].x - v->eye.x;
	dy1 = c[3].y - v->eye.y;
	d0 = (dx0 * v->forward[0] + dy0 * v->forward[1] + (c[0].z - v->eye.z) * v->forward[2]) >> 10;
	d1 = (dx1 * v->forward[0] + dy1 * v->forward[1] + (c[3].z - v->eye.z) * v->forward[2]) >> 10;
	if (d0 <= 0 || d1 <= 0 || d0 > 4096 || d1 > 4096)
		return false;
	if ((d0 > d1 ? d0 : d1) * 4 > (d0 > d1 ? d1 : d0) * 5)
		return false; // tapered: upright
	s0 = (dx0 * v->right[0] + dy0 * v->right[1]) >> 10;
	s1 = (dx1 * v->right[0] + dy1 * v->right[1]) >> 10;
	wide = s1 * d0 - s0 * d1;
	if (wide < 0)
		wide = -wide;
	return (c[1].z - c[0].z) * (d0 > d1 ? d1 : d0) < wide && wide * 2 > d0 * d1;
}

// The corners of a side on the line along_x ? y = line : x = line, from `from` to `to` along it,
// bottom to top: LB, LT, RT, RB
static void level_side_corners(bool along_x, int line, int from, int to, int bottom, int top, frustum_point *c)
{
	for (int k = 0; k < 4; ++k)
	{
		const int along = k < 2 ? from : to;

		c[k].x = along_x ? along : line;
		c[k].y = along_x ? line : along;
		c[k].z = (k == 1 || k == 2) ? top : bottom;
	}
}

// A top's grey: GREY_TOP at the corridor's floor and above, darker the lower it is (a step of 1 in
// 255 every 4 units down, to 100 darker), so the city's roofs one below another read apart: all one
// grey, the street between two looked like more roof
static jo_color grey_top_colour(int height)
{
	const int dim = height >= 0 ? 0 : (-height >> 2) > 100 ? 100 : (-height >> 2);

	return JO_COLOR_RGB(150 - dim, 150 - dim, 164 - (dim >> 1));
}

static unsigned level_planes_rect(const frustum_point *a, const frustum_point *b, unsigned planes); // below

/*
 * The roofs the watchdog drone's scan line lights this frame (drone_draw_prepare: game/drone.h,
 * drone_scan_lit), each the lit part of one roof, and their colour. The city's roof tops are drawn
 * as faces of their own (grey_boxes_add), and VDP1 has no depth: a lit part drawn on its own would
 * be sorted by its own middle, against the roof's, and could come out under it. So each is added
 * straight after its roof, chained to it (JO_3D_SORT_PREVIOUS), cut to the view as the roof is
 */
static drone_box scan_lit[DRONE_LIT_MAX];
static int scan_lit_count = 0;
static jo_color scan_lit_colour;

static skyline city_skyline; // this frame's towers that hide others (game/skyline.h)
// What the roof you stand on hides this frame (game/floor_hides.h): below a height for each upright
// column past it, worked out from where you look. Nothing when you're not on a roof
static floor_hides stand_floor;

/*
 * Whether a box in the world (its middle, and half its size each way) could be seen this frame: in
 * the view, and in the city not wholly behind a nearer, taller tower (the skyline the towers have
 * just been drawn by: so only after draw_corridor_boxes). For things drawn on their own (the
 * drone): jo transforms a mesh's every point whether any of it's on screen or not
 */
bool level_box_seen(frustum_point centre, frustum_point half)
{
	if (!level_view_on)
		return true;
	if (frustum_classify_box(&level_view, centre, half, FRUSTUM_SCREEN) == FRUSTUM_OUTSIDE)
		return false;
	if (portal.outside && city_skyline.count > 0)
	{
		const skyline_box b = {(short)(centre.x - half.x), (short)(centre.x + half.x), (short)(centre.y - half.y),
							   (short)(centre.y + half.y), (short)(centre.z + half.z)};

		if (skyline_hides(&city_skyline, &b))
			return false;
	}
	return true;
}

void level_scan_set(const drone_box *lit, int count, jo_color colour)
{
	scan_lit_count = count < DRONE_LIT_MAX ? count : DRONE_LIT_MAX;
	for (int i = 0; i < scan_lit_count; ++i)
		scan_lit[i] = lit[i];
	scan_lit_colour = colour;
}

// After roof r (at height) has been added: the lit parts on it, chained after it
static void scan_lit_add(const world_rect *r, int height, unsigned planes)
{
	for (int i = 0; i < scan_lit_count; ++i)
	{
		const drone_box *l = &scan_lit[i];

		if (l->top != height || l->x_min < r->x_min || l->x_max > r->x_max || l->y_min < r->y_min || l->y_max > r->y_max)
			continue;
		{
			const frustum_point c[4] = {{l->x_min, l->y_min, height}, {l->x_max, l->y_min, height}, {l->x_max, l->y_max, height},
										{l->x_min, l->y_max, height}};

			level_face_add(&grey_out, c, 4, planes != 0 ? level_planes_rect(&c[0], &c[2], planes) : 0, scan_lit_colour,
						   LEVEL_FOG_NONE, JO_3D_SORT_PREVIOUS, false);
		}
	}
}


static bool grey_stairs_profiled(int i, bool along_x, int line); // below

// One side of box i: from what hides its foot (bottom: grey_side_bottom) to its top. test: the
// planes to test it against (0: its box is wholly in view, so it is too)
/*
 * A city tower's sides ended 900 below the eye, not at the ground 3000 down: below that
 * the background's black (the VDP2 floor's off on foot), so the towers drop away into the dark, as
 * the plan has it ("the drop feels endless"). With 84 towers, sides to the ground nearly all reached
 * past the screen's bottom and were cut there, in 3D, two divisions each: the boxes took 83% of a
 * frame from the entry (play-test 2026-11-09). Ending higher, most stay inside the screen and its
 * guard band, uncut, and VDP1 has less to fill.
 * Since then the circuit board's under them (game/board.h, the VDP2 floor): ended in the air,
 * they'd float over it. So they go down to the ground again (2026-11-13), and what ends them is
 * the screen's bottom (the slopes, below: added after the depth, it does the same job, a multiply
 * a side, no cut)
 */
// (0: ended 900 under the eye, as before. Filmed both ways at the hub, hub.steps: no difference
// that the films' own run-to-run swing, 36-60 fps view by view, didn't swamp)
// (the pause menu's "City sides": game/settings.h)
/*
 * And they end where the screen does. Down in the city (the CD block's hub, 1400 down) nearly every
 * tower stands a thousand or more above you: each side reached far past the screen's top, was cut
 * there in 3D, and VDP1 was handed the lot: looking back across the city, cpu 200% and VDP1 not
 * finishing, the roof at your feet (the nearest thing, so drawn last) left half filled, a wedge
 * (play-test 2026-11-12). The view can only see so steeply up and down (frustum_slopes, once a
 * pass): a side whose far end is D along the floor from the eye can't be seen above
 * eye.z + D x up. So its top comes down to there and its foot up, a multiply each, and it's
 * still a rectangle: mostly no cut at all
 */
static int city_slope_up = FRUSTUM_STEEP, city_slope_down = -FRUSTUM_STEEP;

// Whether city box i can be seen from where you stand: seen is the patch's row of city_seen
// (NULL: no table here, everything can)
static bool grey_city_seen(const unsigned char *seen, int i)
{
	const int tower = grey_city_order[i - CORRIDOR_BOXES];

	return seen == JO_NULL || ((seen[tower >> 3] >> (tower & 7)) & 1) != 0;
}

static skyline_box grey_skyline_box(const grey_box *b)
{
	const skyline_box box = {(short)b->rect.x_min, (short)b->rect.x_max, (short)b->rect.y_min, (short)b->rect.y_max, (short)b->height};

	return box;
}

// No less than how far (x, y) is from the eye along the floor: the longer of the two ways plus
// half the shorter (at most 12% over, never under: no square root)
static int city_reach(int x, int y)
{
	int dx = x - level_view.eye.x, dy = y - level_view.eye.y;

	if (dx < 0)
		dx = -dx;
	if (dy < 0)
		dy = -dy;
	return dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);
}

int level_reach(int x, int y)
{
	return city_reach(x, y);
}

/*
 * tall: the screen's top and bottom planes, if the tower's box crosses them and they were left
 * out of `test` (grey_boxes_add). A city tower's side is ended where the view does (the slopes,
 * below), so it can't reach far past the screen's top or bottom: unless it's close, when its near
 * end is much nearer than the far end the trim goes by. So a side whose far end is no more than
 * half as far again as its near end isn't tested against those two planes at all: trimmed, it's
 * within the guard band, where nothing's cut. Deep in the city every tower crosses them (they
 * stand from 3000 below to far above you), and testing every tower and then every face against
 * them, twice a face, was most of the boxes' 50% of a frame at the CD block's hub (2026-11-13)
 */
static void grey_side_add(int i, int which, bool along_x, int line, int from, int to, unsigned test, unsigned tall)
{
	const grey_box *b = &grey_boxes[i];
	frustum_point c[4];
	jo_color colour;
	int bottom = b->bottom[which], top = b->height;
	// Each end's own top and foot (a city tower's: below): the `from` end's, and the `to` end's
	int top_from, top_to, bottom_from, bottom_to;
	unsigned planes = test | tall;

	top_from = top_to = top;
	bottom_from = bottom_to = bottom;
	// A city tower's side that's partly behind a nearer, taller tower: only what shows of it
	// (game/skyline.h, skyline_trim: this frame's hiders). Nothing left: not drawn
	if (b->floor == CITY_GROUND && level_view_on && city_skyline.count > 0 && !skyline_trim(&city_skyline, along_x, line, &from, &to, top))
		return;
	if (b->floor == CITY_GROUND && level_view_on)
	{
		// Its further end from the eye: what's seen of it is within the slopes that far out
		const int one = along_x ? city_reach(from, line) : city_reach(line, from);
		const int other = along_x ? city_reach(to, line) : city_reach(line, to);
		const int reach = one > other ? one : other, nearest = one > other ? other : one;

		if (reach * 2 <= nearest * 3 && city_slope_up < FRUSTUM_STEEP && city_slope_down > -FRUSTUM_STEEP)
			planes = test; // trimmed, and not close: no top or bottom plane to mind
		if (!settings_on(&game_settings, SETTING_CITY_SIDES) && bottom < level_view.eye.z - 900)
			bottom = level_view.eye.z - 900;
		/*
		 * Each end's trimmed by its own distance (2026-11-13): the side's top and foot then run
		 * along the screen's top and bottom, a trapezoid in the same upright plane. Both by the
		 * further end's, the near end reached far past the screen, and VDP1 walks every pixel of
		 * what it's given, on the screen or off: once the sides went down to the board again, a
		 * side seen along its length hung a screen or two below the picture at its near end.
		 * Still errs towards drawing: along the side the distance from you is never more than
		 * the straight line between its ends' distances, and the slopes are never the wrong
		 * way (up's 0 or more, down's 0 or less: frustum_slopes).
		 * (a few units over, for the rounding. A reach is under 2^14 and a slope 2^12: 32 bits)
		 */
		/*
		 * Each is offered as a whole line, end to end, and taken only if it's above the side's own
		 * foot (or below its own top) at both ends: floor_hides_line has why. Not the higher of the
		 * two at each end: where the line crosses the side's foot partway along, a straight line
		 * between those cut a wedge out of the side (the user, 2026-11-15, 0650: a guard tower's side
		 * standing on the lower roof next to yours, the street's depths showing under it)
		 */
		if (city_slope_up < FRUSTUM_STEEP)
		{
			// The top's the same, upside down: negated, a line at or below the top is one "above" it
			int down_from = -top, down_to = -top;

			floor_hides_line(-top, &down_from, &down_to, -(level_view.eye.z + ((one * city_slope_up) >> 8) + 8),
							 -(level_view.eye.z + ((other * city_slope_up) >> 8) + 8));
			top_from = -down_from;
			top_to = -down_to;
		}
		if (city_slope_down > -FRUSTUM_STEEP)
			floor_hides_line(bottom, &bottom_from, &bottom_to, level_view.eye.z + ((one * city_slope_down) >> 8) - 8,
							 level_view.eye.z + ((other * city_slope_down) >> 8) - 8);
		/*
		 * The roof you stand on hides the side below a height at each end (game/floor_hides.h): it
		 * starts there, not at the street 3000 below. This is most of what VDP1 was drawing under
		 * your own roof, running across the city (tests/e2e/roof_cut_run.steps)
		 */
		floor_hides_line(bottom, &bottom_from, &bottom_to,
						 along_x ? floor_hides_below(&stand_floor, from, line) : floor_hides_below(&stand_floor, line, from),
						 along_x ? floor_hides_below(&stand_floor, to, line) : floor_hides_below(&stand_floor, line, to));
		// An end with nothing left comes to a point; neither: no side
		if (bottom_from > top_from)
			bottom_from = top_from;
		if (bottom_to > top_to)
			bottom_to = top_to;
		top = JO_MAX(top_from, top_to);
		bottom = JO_MIN(bottom_from, bottom_to);
		if (bottom_from >= top_from && bottom_to >= top_to)
			return;
	}
	if (bottom >= top || grey_stairs_profiled(i, along_x, line))
		return; // hidden all the way up (or below the depth they're drawn to), or the stairs' profile
	level_side_corners(along_x, line, from, to, bottom, top, c);
	{
		// The box it's tested against: all of it, from its lowest foot to its highest top
		const frustum_point low = c[0], high = c[2];

		c[0].z = bottom_from; // LB, LT at `from`; RT, RB at `to`
		c[1].z = top_from;
		c[2].z = top_to;
		c[3].z = bottom_to;
		if (planes != 0)
			planes = level_planes_rect(&low, &high, planes);
	}
	if (b->lit)
		colour = along_x ? HUB_SIDE_Y : HUB_SIDE_X;
	else if (i & 1)
		colour = along_x ? GREY_SIDE_Y_ALT : GREY_SIDE_X_ALT;
	else
		colour = along_x ? GREY_SIDE_Y : GREY_SIDE_X;
	level_face_add(&grey_out, c, 4, planes, colour, LEVEL_FOG_FLAT, JO_3D_SORT_CENTER, level_view_on && level_side_lies(c));
}

/*
 * The planes a box lined up with the axes needs cutting by (centre, and half its size each way; a
 * face is a box with no thickness: level_planes_rect). It's tested against the screen's edges (and
 * a portal's, in `planes`): wholly outside one, FRUSTUM_OUTSIDE, it's not drawn; wholly inside them
 * all, 0, it's drawn as it is. Crossing some, it's cut by the near plane and the portal's. And by
 * a screen's edge only if it reaches past the guard band beyond that edge (game/frustum.h:
 * FRUSTUM_GUARD): something a little over the edge is left for VDP1 (walking a few pixels off the
 * screen costs it less than the cut costs the CPU), but something reaching far past it is cut at
 * the edge itself. Cut at the guard band instead, the stairs' fronts (lying lines, each the cut
 * width) were walked out to twice the screen's width, and VDP1 went to two frames (30 fps).
 * frustum_classify_box: exact, as its corners. level_cut_exact: cut at every edge it crosses (the
 * glass floor's far quads, always far wider than the screen)
 */
static bool level_cut_exact = false;

static unsigned level_planes_box(frustum_point centre, frustum_point half, unsigned planes)
{
	const unsigned crossed = frustum_classify_box(&level_view, centre, half, planes);

	unsigned reach;

	if (crossed == 0 || crossed == FRUSTUM_OUTSIDE || level_cut_exact)
		return crossed;
	// The screen's edges are bits 1 .. 4, the guard band's 5 .. 8: four up. Which guard band edges
	// it reaches past, then the screen's edges they're beyond, four back down
	reach = frustum_classify_box(&level_view, centre, half, (crossed & ~0x1Eu) | ((crossed & 0x1Eu) << 4));
	return (reach & ~0x1E0u) | ((reach & 0x1E0u) >> 4);
}

// The same for a flat rectangle lined up with the axes, from two opposite corners of it (a box's
// side or top, a strip of the floor)
static void level_rect_box(const frustum_point *a, const frustum_point *b, frustum_point *centre, frustum_point *half)
{
	centre->x = (a->x + b->x) >> 1;
	centre->y = (a->y + b->y) >> 1;
	centre->z = (a->z + b->z) >> 1;
	// Rounded down, a half could be a unit short: one more keeps it the whole rectangle or more
	half->x = ((a->x > b->x ? a->x - b->x : b->x - a->x) >> 1) + 1;
	half->y = ((a->y > b->y ? a->y - b->y : b->y - a->y) >> 1) + 1;
	half->z = ((a->z > b->z ? a->z - b->z : b->z - a->z) >> 1) + 1;
}

static unsigned level_planes_rect(const frustum_point *a, const frustum_point *b, unsigned planes)
{
	frustum_point centre, half;

	level_rect_box(a, b, &centre, &half);
	return level_planes_box(centre, half, planes);
}

// The planes the rectangle crosses, just as they are (no guard band): for leaving things out
static unsigned level_rect_classify(const frustum_point *a, const frustum_point *b, unsigned planes)
{
	frustum_point centre, half;

	level_rect_box(a, b, &centre, &half);
	return frustum_classify_box(&level_view, centre, half, planes);
}

/*
 * The stairs far off, as a ramp: past GREY_STAIRS_RAMP each step (8 high, 12 deep) is about a
 * pixel on the screen, and the twenty of them were ~40 faces, each tested, cut and handed to SGL:
 * from the yard, looking down the corridor at them ~1700 away, a tenth of the frame. Two faces
 * now: the slope through the steps' front edges, and the open side's profile. Only how it's
 * drawn: you still walk up the steps (game/corridor.h)
 */
#define GREY_STAIRS_RAMP 1000
#define GREY_STAIRS_SLOPE JO_COLOR_RGB(106, 106, 120) // between the steps' two fronts' greys

/*
 * Nearer than that, past GREY_STAIRS_PROFILE, the stairs' open side (seen from beside them) is one
 * face, the ramp's profile, in place of the twenty steps' sides: each a whole rectangle from the
 * floor up to its own top, twenty overlapping faces for one wall (the yard's end, ~600 off: ~10%
 * of the CPU, and VDP1 filling the side twenty times over). The profile's slope edge cuts off the
 * steps' corners (8 x 12): under 3 pixels from that far. The fronts and tops stay, step by step
 */
#define GREY_STAIRS_PROFILE 400
static bool grey_stairs_profile_on = false; // this frame: the steps' open sides are left out

// Whether a step's side (box i, on a line) is the open side the profile stands in for this frame
static bool grey_stairs_profiled(int i, bool along_x, int line)
{
	return grey_stairs_profile_on && i < GREY_STAIRS && along_x == grey_stairs_open_along_x &&
		   line == grey_stairs_open_line;
}

// How far the eye is outside the box round group g, along x or y, whichever's more (no square root)
static int grey_group_distance(const grey_group *group, const frustum_point *e)
{
	const int dx = (e->x > group->middle.x ? e->x - group->middle.x : group->middle.x - e->x) - group->half.x;
	const int dy = (e->y > group->middle.y ? e->y - group->middle.y : group->middle.y - e->y) - group->half.y;

	return dx > dy ? dx : dy;
}

// Whether the stairs (group g) are far enough off to be the ramp: further than GREY_STAIRS_RAMP
static bool grey_stairs_far(const grey_group *group, const frustum_point *e)
{
	return grey_group_distance(group, e) > GREY_STAIRS_RAMP;
}

// The ramp's two faces, tested against the planes the stairs' group crosses (0: none)
static void grey_stairs_ramp(unsigned crossed)
{
	const frustum_point *ramp = grey_stairs_ramp_corners, *side = grey_stairs_side_corners;

	level_face_add(&grey_out, ramp, 4, crossed != 0 ? level_planes_rect(&ramp[0], &ramp[2], crossed) : 0,
				   GREY_STAIRS_SLOPE, LEVEL_FOG_FLAT, JO_3D_SORT_CENTER, false);
	level_face_add(&grey_out, side, 4, crossed != 0 ? level_planes_rect(&side[0], &side[2], crossed) : 0,
				   GREY_SIDE_Y, LEVEL_FOG_FLAT, JO_3D_SORT_CENTER, false);
}

/*
 * The arena seen on foot: a pyramid (the user, 2026-11-13: "slanted walls down from the arena walls
 * into the city, making it look like a pyramid ... the arena is behind the slanted walls"). From
 * the top of each of the arena's walls a face slopes out and down to the circuit board the city
 * stands on (CITY_GROUND), at 45 degrees: PYRAMID_RUN out for the same down. That's exactly the
 * corridor's length (1500 up + 3000 down = 4500), so the pyramid's foot is at the entry tower's.
 * The arena's own walls aren't drawn on foot at all (their 250 pieces, texture and flicker): from
 * outside they'd be the pyramid's top edge, nothing more. Up to a dozen plain faces instead.
 *
 * The corridor comes out of the pyramid's side. In its own measures (out from the arena's wall,
 * across; the slope's height at `out` is WALL_TOP_Z - out):
 *   - out 0 to PYRAMID_TUNNEL_OUT (1002) the slope's above the corridor's walls: it's a tunnel,
 *     the slope its ceiling, a cheek either side closing the gap over each wall, and the arena's
 *     wall over the mirror at its back
 *   - then to PYRAMID_FLOOR_OUT (1500) the corridor's walls stand out of the slope. The slope
 *     isn't drawn across the corridor's width there: it would cut through the room
 *   - from there on the slope's under the corridor's floor: the corridor's a bridge to the city
 * So that side is four faces: left and right of the corridor, the ceiling, and under the bridge.
 * All sorted by their furthest points (JO_3D_SORT_MAX): the towers, the corridor and what's in it
 * are drawn over them. A side's only drawn from outside it (but the corridor's, also seen from
 * inside as the ceiling); the top only from above it (the lift)
 */
#define PYRAMID_RUN (WALL_TOP_Z - CITY_GROUND)					 // out, for all the way down: 45 degrees
#define PYRAMID_TUNNEL_OUT (WALL_TOP_Z - CORRIDOR_TOP_ON_FOOT)	 // where the slope's down to the corridor's walls' top
#define PYRAMID_FLOOR_OUT WALL_TOP_Z							 // and to its floor
#define PYRAMID_FRONT JO_COLOR_RGB(18, 18, 38)					 // the corridor's side
#define PYRAMID_SIDE JO_COLOR_RGB(10, 10, 24)					 // the others: darker, so its edges read
#define PYRAMID_TOP JO_COLOR_RGB(24, 24, 48)
#define PYRAMID_INSIDE JO_COLOR_RGB(8, 8, 18)					 // the tunnel's cheeks and back
#define PYRAMID_BELOW_EYE 200 // from deep in the city, drawn down to this far under the eye
#define PYRAMID_FACES 12
static jo_pos3D_fixed pyramid_points[PYRAMID_FACES * FRUSTUM_POLY_MAX];
static jo_3d_polygon pyramid_polygons[PYRAMID_FACES * (FRUSTUM_POLY_MAX / 2)];
static jo_3d_polygon_attribute pyramid_attributes[PYRAMID_FACES * (FRUSTUM_POLY_MAX / 2)];
static jo_3d_mesh pyramid_mesh;

// One of its faces: corners as (out, across, z) in the corridor's measures, n of them in order
// round it. Cut to the view; not at all if it's out of it
static void pyramid_face(level_mesh *m, const corridor *passage, const int (*corners)[3], int n, jo_color colour, int sort)
{
	frustum_point c[4];
	unsigned crossed;

	for (int k = 0; k < n; ++k)
		c[k] = grey_point(corridor_point(passage, corners[k][0], corners[k][1]), corners[k][2]);
	crossed = frustum_classify(&level_view, c, n, FRUSTUM_SCREEN);
	if (crossed != FRUSTUM_OUTSIDE)
		level_face_add(m, c, n, crossed, colour, LEVEL_FOG_NONE, sort, false);
}

static void arena_pyramid_add(const corridor *passage)
{
	/*
	 * How far down it's drawn: to the board. But from deep in the city (city_deep: among the
	 * towers, below the corridor's floor) only to PYRAMID_BELOW_EYE under your eye: what's lower is
	 * behind the towers between you and it, and VDP1 has no depth buffer, so it filled all of it
	 * anyway, a third of the screen behind the towers: looking back from the CD block's hub, 60
	 * fps became 30-44 (city_checks.steps, 2026-11-13). The faces end on the slope at that height
	 * (45 degrees: as far out as down), so they're the same planes, only shorter.
	 * Only while you're stood on a roof, though (city_roof_under: your eye at most a jump over it).
	 * The lift takes you up to 4000 over the city, still deep by city_deep's measure: up there you
	 * see over the towers, and the pyramid ended 200 under your eye, its bottom cut off in the air
	 * (the user, 2026-11-13: "the pyramid bottom clips when viewed from elevator")
	 */
	const int top = WALL_TOP_Z, low = CORRIDOR_TOP_ON_FOOT;
	const int a0 = passage->across_min, a1 = passage->across_max;
	// Where you are, in the corridor's measures (across from the arena's middle, not the corridor's)
	const int out = level_eye_out, across = level_eye_across + ((a0 + a1) >> 1), z = level_view.eye.z;
	/*
	 * And from inside the corridor, between its walls and under their tops, only down to the
	 * walls' tops: the walls hide everything lower. Drawn, the slope beside you came out over
	 * the wall and the floor, a dark wedge (corridor_still.steps, 2026-11-13): VDP1 draws far to
	 * near by one depth a polygon, and a piece of a face that big sorts anywhere
	 */
	const bool inside = across > a0 && across < a1 && out > 0 && out < CORRIDOR_LENGTH + GREY_DOORWAY && z < low;
	const int base = inside ? low
					 : city_deep() && z - PYRAMID_BELOW_EYE > CITY_GROUND && city_roof_under(out, level_eye_across, z) >= 0
						 ? z - PYRAMID_BELOW_EYE
																		   : CITY_GROUND;
	const int run = WALL_TOP_Z - base;				  // out from the wall to where it ends
	const int half = passage->wall, far = half + run; // the arena's half width; and its foot's
	const int back = -2 * half;						  // the opposite wall, out
	level_mesh m;

	level_mesh_begin(&m, pyramid_points, pyramid_polygons, pyramid_attributes, PYRAMID_FACES * FRUSTUM_POLY_MAX,
					 PYRAMID_FACES * (FRUSTUM_POLY_MAX / 2));
	{
		// The corridor's side: left and right of it (each from the arena's corner to the corridor's
		// wall at the top, from the foot's corner at the bottom),
		const int left[4][3] = {{0, -half, top}, {0, a0, top}, {run, a0, base}, {run, -far, base}};
		const int right[4][3] = {{0, a1, top}, {0, half, top}, {run, far, base}, {run, a1, base}};
		// the tunnel's ceiling, and the slope under the bridge
		const int ceiling[4][3] = {{0, a0, top}, {0, a1, top}, {PYRAMID_TUNNEL_OUT, a1, low}, {PYRAMID_TUNNEL_OUT, a0, low}};
		const int under[4][3] = {{PYRAMID_FLOOR_OUT, a0, 0}, {PYRAMID_FLOOR_OUT, a1, 0}, {run, a1, base}, {run, a0, base}};

		pyramid_face(&m, passage, left, 4, PYRAMID_FRONT, JO_3D_SORT_MAX);
		pyramid_face(&m, passage, right, 4, PYRAMID_FRONT, JO_3D_SORT_MAX);
		if (base < 0) // (not if it ends above the corridor's floor)
			pyramid_face(&m, passage, under, 4, PYRAMID_FRONT, JO_3D_SORT_MAX);
		// The tunnel, only from in it or looking into it down the corridor: its ceiling, its
		// cheeks (a triangle over each wall, from the arena's top down to where the slope meets
		// the wall's top) and its back (the arena's wall over the mirror)
		if (inside)
		{
			const int cheek0[3][3] = {{0, a0, top}, {PYRAMID_TUNNEL_OUT, a0, low}, {0, a0, low}};
			const int cheek1[3][3] = {{0, a1, top}, {PYRAMID_TUNNEL_OUT, a1, low}, {0, a1, low}};
			const int wall[4][3] = {{0, a0, low}, {0, a0, top}, {0, a1, top}, {0, a1, low}};

			pyramid_face(&m, passage, ceiling, 4, PYRAMID_FRONT, JO_3D_SORT_MAX);
			pyramid_face(&m, passage, wall, 4, PYRAMID_INSIDE, JO_3D_SORT_MAX);
			pyramid_face(&m, passage, cheek0, 3, PYRAMID_INSIDE, JO_3D_SORT_CENTER);
			pyramid_face(&m, passage, cheek1, 3, PYRAMID_INSIDE, JO_3D_SORT_CENTER);
		}
	}
	// The other three, each from outside it: beyond its plane, which leans in at 45 degrees
	// (as far out past its wall as you're below the top, you're on it)
	if (-across - half + (z - top) > 0)
	{
		const int face[4][3] = {{back, -half, top}, {0, -half, top}, {run, -far, base}, {back - run, -far, base}};

		pyramid_face(&m, passage, face, 4, PYRAMID_SIDE, JO_3D_SORT_MAX);
	}
	if (across - half + (z - top) > 0)
	{
		const int face[4][3] = {{0, half, top}, {back, half, top}, {back - run, far, base}, {run, far, base}};

		pyramid_face(&m, passage, face, 4, PYRAMID_SIDE, JO_3D_SORT_MAX);
	}
	if (back - out + (z - top) > 0)
	{
		const int face[4][3] = {{back, half, top}, {back, -half, top}, {back - run, -far, base}, {back - run, far, base}};

		pyramid_face(&m, passage, face, 4, PYRAMID_SIDE, JO_3D_SORT_MAX);
	}
	// Its flat top, over the arena: from above it (the lift)
	if (z > top)
	{
		const int face[4][3] = {{0, -half, top}, {0, half, top}, {back, half, top}, {back, -half, top}};

		pyramid_face(&m, passage, face, 4, PYRAMID_TOP, JO_3D_SORT_MAX);
	}
	if (m.quad_count == 0)
		return;
	pyramid_mesh.data.points = pyramid_points;
	pyramid_mesh.data.polygons = pyramid_polygons;
	pyramid_mesh.data.attributes = pyramid_attributes;
	pyramid_mesh.data.point_count = (unsigned short)m.point_count;
	pyramid_mesh.data.polygon_count = (unsigned short)m.quad_count;
	jo_3d_mesh_draw(&pyramid_mesh);
}

static void grey_boxes_add(void)
{
	const frustum_point *e = &level_view.eye;

	floor_hides_none(&stand_floor); // (set below, if you're stood on a roof with the view worked out)

	/*
	 * What's seen from the patch of roof you're on (city.h, city_seen: worked out ahead): a tower
	 * its bit's clear for is hidden from anywhere on that patch, and is left out with that one
	 * test: not offered as a hider, not tested against the view or the hiders. NULL off a roof:
	 * then every tower's looked at, as before
	 */
	const unsigned char *seen = JO_NULL;
	int roof = -1; // the roof you're stood on (city_towers' index), if you are
	/*
	 * Out in the city, stood on a roof the corridor's doorway can't see (city_seen_doorway: worked
	 * out ahead, every point of the doorway up to CITY_DOORWAY_EYE against every tower), you can't
	 * see into the corridor either: what's between hides it both ways. Then none of its boxes is
	 * looked at, the yard's tower, stairs and crates (the first two groups): before, each was tested
	 * against the view and cut to the doorway every frame, deep in the city where it can't be seen
	 * (the user, 2026-11-14: slow at 0650-0630 and 0B40-0940, "check that objects hidden behind the
	 * city are not drawn")
	 */
	bool corridor_hidden = false;

	if (level_view_on && view_is_on_foot() && portal.outside)
	{
		roof = city_roof_under(level_eye_out, level_eye_across, e->z);
		if (roof >= 0)
		{
			seen = city_seen[roof][city_seen_patch(&city_towers[roof], level_eye_out, level_eye_across)];
			corridor_hidden = !((city_seen_doorway[roof >> 3] >> (roof & 7)) & 1);
		}
	}
	/*
	 * Inside the corridor, only what's seen through the doorway. Every tower was looked at from in
	 * there, every frame: tested against the view and the doorway, against the hiders, its sides
	 * trimmed, to send 5-11 sides in the end. The boxes took 31-59% of a frame walking to the
	 * stairs, the frame 53-59 fps (stairs_sky.steps, 2026-11-13)
	 */
	else if (level_view_on && view_is_on_foot() && portal.inside && e->z <= CITY_DOORWAY_EYE)
		seen = city_seen_doorway; // (city.h: worked out ahead, for the widest doorway)
	if (level_view_on)
	{
		frustum_slopes(&level_view, &city_slope_up, &city_slope_down);
		/*
		 * The roof under you hides what's below it: stood H over it and at least d from its
		 * nearest edge, every line of sight leaves the roof d or more out, H lower: so nothing's
		 * seen below a slope of H / d down from your eye, whichever way you look. Stood in the
		 * middle of a roof (d 250, H 35) that's far shallower than the screen's bottom, and the
		 * towers' sides end there (grey_side_add), not at the screen's bottom behind the roof:
		 * VDP1 has no depth buffer and filled all of that, under the roof it drew last. With the
		 * skyline ring whole (2026-11-13) that was the difference between 30 fps and 60 looking
		 * back from the CD block's hub. One division, once a frame; near an edge (d small) it's
		 * steeper than the screen's and changes nothing
		 */
		floor_hides_none(&stand_floor);
		if (roof >= 0 && FLOOR_HIDES)
		{
			// The roof's box, in the world's x and y (grey_city_order maps the boxes to city.h's towers)
			for (int i = CORRIDOR_BOXES; i < GREY_BOXES; ++i)
				if (grey_city_order[i - CORRIDOR_BOXES] == roof)
				{
					const world_rect *r = &grey_boxes[i].rect;

					floor_hides_begin(&stand_floor, e->x, e->y, e->z, r->x_min, r->x_max, r->y_min, r->y_max,
									  grey_boxes[i].height);
					break;
				}
		}
		if (roof >= 0)
		{
			const city_tower *t = &city_towers[roof];
			const int h = e->z - t->height;
			int d = level_eye_out - t->out_min;

			d = JO_MIN(d, t->out_max - level_eye_out);
			d = JO_MIN(d, level_eye_across - t->across_min);
			d = JO_MIN(d, t->across_max - level_eye_across);
			if (h > 0 && d > 16)
			{
				// (rounded steeper, and a little more: it errs towards drawing)
				const int slope = -((h * FRUSTUM_SLOPE_ONE) / d) - 8;

				if (slope > city_slope_down)
					city_slope_down = slope;
			}
		}
		/*
		 * The towers that hide others this frame (game/skyline.h): the nearest ones in front of
		 * you that stand over your eye. A whole block (2 x 2, touching) where all four do, as one
		 * hider as low as its lowest, and every tower on its own
		 */
		skyline_begin(&city_skyline, e->x, e->y, e->z, level_view.forward[0], level_view.forward[1]);
		for (int g = 0; g < GREY_GROUPS; ++g)
		{
			const grey_group *group = &grey_groups[g];
			const int lowest = group->lowest;

			if (group->first < CORRIDOR_BOXES)
				continue;
			if (lowest >= e->z)
			{
				const skyline_box block = {(short)(group->middle.x - group->half.x + 1), (short)(group->middle.x + group->half.x - 1),
										   (short)(group->middle.y - group->half.y + 1), (short)(group->middle.y + group->half.y - 1),
										   (short)lowest};

				skyline_offer(&city_skyline, &block);
			}
			// (and its towers too: one guard page in a block of lower towers hides more than the
			// block does, as low as its lowest)
			for (int i = group->first; i <= group->last; ++i)
				if (grey_boxes[i].height >= e->z && // (one you see over is turned away: don't box it up)
					grey_city_seen(seen, i))		 // (nor one that's hidden itself)
				{
					const skyline_box tower = grey_skyline_box(&grey_boxes[i]);

					skyline_offer(&city_skyline, &tower);
				}
		}
	}
	for (int g = 0; g < GREY_GROUPS; ++g)
	{
		const grey_group *group = &grey_groups[g];
		const bool city = group->first >= CORRIDOR_BOXES;

		if (!city && corridor_hidden)
			continue; // the corridor's boxes, from a roof that can't see into it
		// The screen's edges, and the doorway's if the group is in the other room from you (from
		// the city, the corridor's boxes stand on its floor: only the planes that can hide that)
		const unsigned planes =
			FRUSTUM_SCREEN | (city ? (portal.inside ? portal.planes : 0) : (portal.outside ? portal.floor_planes : 0));
		// The planes the group crosses (0: it's all in view)
		unsigned crossed = 0;

		if (level_view_on)
		{
			crossed = frustum_classify_box(&level_view, group->middle, group->half, planes);
			if (crossed == FRUSTUM_OUTSIDE)
				continue; // none of it can be seen from here
			if (city)
			{
				// The whole block behind a nearer, taller one: none of it's drawn
				const skyline_box block = {(short)(group->middle.x - group->half.x + 1), (short)(group->middle.x + group->half.x - 1),
										   (short)(group->middle.y - group->half.y + 1), (short)(group->middle.y + group->half.y - 1),
										   (short)(group->middle.z + group->half.z)};

				if (skyline_hides(&city_skyline, &block))
					continue;
			}
			if (g == 0 && grey_stairs_far(group, e))
			{
				grey_stairs_ramp(crossed); // the stairs (the first group), far off
				continue;
			}
		}
		grey_stairs_profile_on = false;
		if (g == 0 && level_view_on && grey_group_distance(group, e) > GREY_STAIRS_PROFILE &&
			((grey_stairs_open_along_x ? e->y : e->x) - grey_stairs_open_line) * grey_stairs_open_out > 0)
		{
			const frustum_point *side = grey_stairs_side_corners;

			grey_stairs_profile_on = true; // you're outside the open side: its profile, once
			level_face_add(&grey_out, side, 4, crossed != 0 ? level_planes_rect(&side[0], &side[2], crossed) : 0,
						   GREY_SIDE_Y, LEVEL_FOG_FLAT, JO_3D_SORT_CENTER, false);
		}
		for (int i = group->first; i <= group->last; ++i)
		{
			const grey_box *b = &grey_boxes[i];
			const world_rect *r = &b->rect;
			// What its faces are tested against: only the planes it crosses (none if it's all in view)
			unsigned test = 0;
			// A city tower: the screen's top and bottom are left out of its test, and its sides'
			// (grey_side_add has why). One wholly above or below the view isn't turned away here:
			// its sides come out with no height once they're trimmed, and its top is tested with them
			unsigned tall = 0;

			if (city && !grey_city_seen(seen, i))
				continue; // hidden from this patch of roof (city_seen)
			if (city && floor_hides_box(&stand_floor, r->x_min, r->x_max, r->y_min, r->y_max, b->height))
				continue; // all of it below your roof's edge, behind the roof (game/floor_hides.h)
			if (crossed != 0)
			{
				const frustum_point middle = {b->centre.x, b->centre.y, b->middle};
				const frustum_point half = {((r->x_max - r->x_min) >> 1) + 1, ((r->y_max - r->y_min) >> 1) + 1,
											((b->height - b->floor) >> 1) + 1};
				unsigned against = crossed;

				if (city && city_slope_up < FRUSTUM_STEEP && city_slope_down > -FRUSTUM_STEEP)
				{
					tall = crossed & FRUSTUM_TOP_BOTTOM;
					against = crossed & ~(unsigned)FRUSTUM_TOP_BOTTOM;
				}
				if (against != 0)
				{
					test = frustum_classify_box(&level_view, middle, half, against);
					if (test == FRUSTUM_OUTSIDE)
						continue; // none of it can be seen from here
				}
			}
			if (city && level_view_on)
			{
				const skyline_box tower = grey_skyline_box(b);

				if (skyline_hides(&city_skyline, &tower))
					continue; // behind a nearer, taller tower
			}

			if (e->x < r->x_min || !level_view_on)
				grey_side_add(i, GREY_X_MIN, false, r->x_min, r->y_min, r->y_max, test, tall);
			if (e->x > r->x_max || !level_view_on)
				grey_side_add(i, GREY_X_MAX, false, r->x_max, r->y_min, r->y_max, test, tall);
			if (e->y < r->y_min || !level_view_on)
				grey_side_add(i, GREY_Y_MIN, true, r->y_min, r->x_min, r->x_max, test, tall);
			if (e->y > r->y_max || !level_view_on)
				grey_side_add(i, GREY_Y_MAX, true, r->y_max, r->x_min, r->x_max, test, tall);
			if (-view_z > b->height)
			{
				const frustum_point top[4] = {{r->x_min, r->y_min, b->height},
											  {r->x_max, r->y_min, b->height},
											  {r->x_max, r->y_max, b->height},
											  {r->x_min, r->y_max, b->height}};

				if (level_face_add(&grey_out, top, 4, (test | tall) != 0 ? level_planes_rect(&top[0], &top[2], test | tall) : 0,
								   b->lit ? HUB_TOP : grey_top_colour(b->height), LEVEL_FOG_NONE, JO_3D_SORT_CENTER, false) &&
					city && scan_lit_count > 0)
					scan_lit_add(r, b->height, test | tall); // the drone's scan line on it
			}
			// A bar (its floor above the corridor's): its underside, from below it
			if (b->floor > 0 && -view_z < b->floor)
			{
				const frustum_point under[4] = {{r->x_min, r->y_min, b->floor},
												{r->x_max, r->y_min, b->floor},
												{r->x_max, r->y_max, b->floor},
												{r->x_min, r->y_max, b->floor}};

				level_face_add(&grey_out, under, 4, test != 0 ? level_planes_rect(&under[0], &under[2], test) : 0,
							   GREY_UNDERSIDE, LEVEL_FOG_NONE, JO_3D_SORT_CENTER, false);
			}
		}
	}
}

/*
 * The corridor's walls on foot (corridor_faces: its two sides and the far end's left half), as
 * the boxes are: each 250 piece a quad for the glowing base and one for the dark body above it,
 * cut to the view. Sorted by their furthest corner: they bound the corridor, so from inside they're
 * behind everything in it (a piece's middle came out nearer than the stairs' fronts beside it, and
 * was drawn over their ends). Out in the city they're its outside, seen from behind (both faces are
 * drawn). Fogged a level at each corner: a 250 piece runs a long way into the fog
 */
static unsigned int corridor_frame = 0; // the frames the corridor's walls are drawn: the base's pulse

static void corridor_walls_add(const corridor *passage)
{
	world_point from[CORRIDOR_FACES], to[CORRIDOR_FACES];
	const jo_color base = corridor_pulse(corridor_frame++);
	// Each piece's two parts, bottom to top
	const int part_bottom[2] = {WALL_BOTTOM_Z, WALL_BASE_TOP_Z};
	const int part_top[2] = {WALL_BASE_TOP_Z, CORRIDOR_TOP_ON_FOOT};

	const bool deep = city_deep();

	corridor_faces(passage, from, to);
	for (int i = 0; i < CORRIDOR_FACES; ++i)
	{
		const bool along_x = from[i].y == to[i].y;
		const int line = along_x ? from[i].y : from[i].x;
		const int start = along_x ? from[i].x : from[i].y;
		const int end = along_x ? to[i].x : to[i].y;
		const int dir = end > start ? 1 : -1;
		const int eye_along = along_x ? level_view.eye.x : level_view.eye.y;
		// From the city, a side wall you're not beyond (0: across_min's, 1: across_max's) shows
		// only its inside, and only through the doorway: the rest is behind the far end's wall. So
		// a piece wholly outside one of the doorway's planes is hidden, and isn't drawn. Only
		// left out, never cut by them (as before, the far end's wall is drawn over what's behind
		// it). The doorway's planes that can hide the floor (floor_planes) do: the sill can't hide
		// a wall that stands on the floor, and the right side hides nothing unless you're beyond
		// across_max's wall, when it's left in
		const bool beyond = i == 0 ? portal.eye_across < passage->across_min : portal.eye_across > passage->across_max;
		unsigned hiding = portal.outside && i < 2 && !beyond ? portal.floor_planes : 0;

		// The whole face first: wholly outside, none of it's drawn; wholly inside, no piece needs
		// testing
		if (hiding != 0)
		{
			frustum_point c[4];

			level_side_corners(along_x, line, start, end, WALL_BOTTOM_Z, CORRIDOR_TOP_ON_FOOT, c);
			hiding = level_rect_classify(&c[0], &c[2], hiding);
			if (hiding == FRUSTUM_OUTSIDE)
				continue;
		}

		for (int a = start; a != end;)
		{
			// The next column 250 on near you, 500 or 1000 further along (each piece's fog goes in
			// a straight line between its ends; far off, a piece is a few pixels across); or the
			// face's end if that's nearer (the far end's half). Near you, 500 too if there's no fog
			// at either end of it: it's all nearer than the fog starts (depth along a straight piece
			// goes evenly from one end to the other), so there's nothing for the fog to follow, and
			// the near pieces, always cut at the screen's edges, are halved
			const int off = a - eye_along < 0 ? eye_along - a : a - eye_along;
			// (From deep in the city: the whole face in one, 2 quads for up to 36)
			int step = deep ? end - a : dir * (off < 1000 ? 250 : off < 2000 ? 500 : 1000);
			int b;

			if (off < 1000 && !deep)
			{
				const int b2 = (dir > 0 ? a + 2 * step > end : a + 2 * step < end) ? end : a + 2 * step;

				if (object_fog_level(along_x ? a : line, along_x ? line : a) == 0 &&
					object_fog_level(along_x ? b2 : line, along_x ? line : b2) == 0)
					step *= 2;
			}
			b = (dir > 0 ? a + step > end : a + step < end) ? end : a + step;

			for (int k = 0; k < 2; ++k)
			{
				frustum_point c[4];
				unsigned planes;

				level_side_corners(along_x, line, a, b, part_bottom[k], part_top[k], c);
				if (hiding != 0 && level_rect_classify(&c[0], &c[2], hiding) == FRUSTUM_OUTSIDE)
					continue; // behind the far end's wall, from the city
				planes = level_planes_rect(&c[0], &c[2], FRUSTUM_SCREEN);
				if (planes != FRUSTUM_OUTSIDE)
					level_face_add(&grey_out, c, 4, planes, k == 0 ? base : CORRIDOR_BODY, LEVEL_FOG_CORNERS, JO_3D_SORT_MAX,
								   level_side_lies(c));
			}
			a = b;
		}
	}
}

void draw_corridor_boxes(const corridor *passage, const wall_clip_camera *camera)
{
	(void)camera; // cut to level_view, this frame's planes (level_view_begin)
	if (passage == JO_NULL || !passage->open || corridor_meshes == JO_NULL)
		return;
	grey_boxes_work_out(passage);
	level_mesh_begin(&grey_out, grey_mesh_points, grey_mesh_polygons, grey_mesh_attributes, GREY_POINTS_MAX,
					 GREY_QUADS_MAX);
	{
		int frc = jo_time_get_frc(); // the debug line's "sd": the boxes (utils/perf.h)

		grey_boxes_add();
		perf_part_add(PERF_SIDES, frc);
		frc = jo_time_get_frc(); // and "co": the corridor's walls
		if (level_view_on && view_is_on_foot())
			corridor_walls_add(passage); // else with the arena's (draw_corridor_walls)
		perf_part_add(PERF_CORRIDOR, frc);
	}
	if (grey_out.quad_count == 0)
		return;
	grey_mesh.data.points = grey_mesh_points;
	grey_mesh.data.polygons = grey_mesh_polygons;
	grey_mesh.data.attributes = grey_mesh_attributes;
	grey_mesh.data.point_count = (unsigned short)grey_out.point_count;
	grey_mesh.data.polygon_count = (unsigned short)grey_out.quad_count;
	/*
	 * In the corridor the slave draws it (slave_draw.h). Out in the city this CPU does: there the
	 * master was waiting 4 to 21 ms a frame for the slave (a whole frame is 16.7: the debug
	 * screen's "mwait", jo_core_master_wait_time), cpu 99-163% at the CD block's hub, and drawn
	 * here instead the wait went and the cpu was 48-96% (hub.steps, 2026-11-13). The towers' big
	 * near faces are what the slave was slow with; why isn't known yet (docs/BACKLOG.md)
	 */
	if (portal.outside)
		jo_3d_mesh_draw(&grey_mesh);
	else
		slave_draw_mesh(&grey_mesh);
}

/*
 * The glass floor on foot, cut to the view in 3D as everything else, and seen through the doorway
 * from the city. All in one mesh:
 *   - near you (within GLASS_OUTLINED along the corridor), the tiles, each outlined in white: the
 *     seams. They used to be the arena's floor (VDP2) showing through the gaps between the tiles,
 *     but that floor is one plane at one height: on foot it had to lie under the city, 3000 down,
 *     where its grid shimmered and fogged black (the black over the hexagons at the start). VDP2 is
 *     left the sky and a black background now (main.c). A white floor under the tiles (VDP1) took
 *     VDP1 to two frames: it filled the floor twice. Outlines are VDP1 lines, a few pixels each,
 *     on the tiles' own corners (glass_outline_add): no second fill, no extra corners for SGL
 *   - further off both ways, the glass as one quad (no seams show that far into the fog)
 * The tiles must lie over the far glass, which is under everything else. So the polygons are
 * chained (JO_3D_SORT_PREVIOUS: each drawn straight after the one before), behind a first one that's sorted
 * by its furthest corner (JO_3D_SORT_MAX): a dot at the floor's deepest point (glass_anchor)
 */
#define GLASS_SEAM JO_COLOR_RGB(255, 255, 255)
// Only tiles this near (along the corridor) are outlined: past it the seams are a pixel or two in the
// fog, and each outline is three more polygons for SGL to sort (the stairs' view went to 30 fps)
#define GLASS_OUTLINED 1000

// Which of mesh m's points from `first` to before `last` is deepest in front of the camera
static int level_deepest(const level_mesh *m, int first, int last)
{
	const frustum *v = &level_view;
	int deepest = first, depth = -32000 * FRUSTUM_ONE;

	for (int k = first; k < last; ++k)
	{
		const int d = (jo_fixed2int(m->points[k].x) - v->eye.x) * v->forward[0] +
					  (jo_fixed2int(m->points[k].y) - v->eye.y) * v->forward[1] +
					  (jo_fixed2int(m->points[k].z) - v->eye.z) * v->forward[2];

		if (d > depth)
		{
			depth = d;
			deepest = k;
		}
	}
	return deepest;
}

/*
 * A point added to mesh m straight ahead of the camera, as deep as its point `deepest`: in the middle
 * of the screen across, so jo always draws it. (The deepest point itself is usually on an edge the floor
 * was cut to: the view's sides are a little wider than the screen (frustum.h), and a dot there was
 * off the screen to jo, which then skipped it. The chain had no key of its own and took the one
 * before it in the slave's list, the nearest there is: facing across the corridor, the floor was
 * drawn over the grey boxes and the map. Play-test 2026-11-08, tests/e2e/floor_chain.steps)
 * The depth is the same dot product as level_deepest's, x FRUSTUM_ONE; the camera's forward is
 * FRUSTUM_ONE long too, so the point is eye + forward x depth / FRUSTUM_ONE^2: two shifts by 10
 * (FRUSTUM_ONE is 1024), no division. Without room for one more point, the deepest itself
 */
static int level_ahead(level_mesh *m, int deepest)
{
	const frustum *v = &level_view;
	const int depth = ((jo_fixed2int(m->points[deepest].x) - v->eye.x) * v->forward[0] +
					   (jo_fixed2int(m->points[deepest].y) - v->eye.y) * v->forward[1] +
					   (jo_fixed2int(m->points[deepest].z) - v->eye.z) * v->forward[2]) >> 10; // world units
	jo_pos3D_fixed *p;

	if (m->point_count >= m->points_max)
		return deepest;
	p = &m->points[m->point_count];
	p->x = jo_int2fixed(v->eye.x + ((v->forward[0] * depth) >> 10));
	p->y = jo_int2fixed(v->eye.y + ((v->forward[1] * depth) >> 10));
	// At the deepest point's height (the floor's), not the eye's: the floor's drawn over it there, as it
	// was over the deepest point (the dot is black; in the middle of the sky it showed). Looking down
	// or up, that moves it a little nearer or further than `depth`: forward[2] x the height between
	p->z = m->points[deepest].z;
	return m->point_count++;
}

/*
 * The chain's first polygon (slot 0, kept for it): a quad with all four corners on one point, as deep
 * as the floor's deepest (a dot; drawn first, and the floor drawn over it), sorted JO_3D_SORT_MAX. Every other
 * polygon of the floor is JO_3D_SORT_PREVIOUS, drawn straight after it in the order they were added: so the
 * whole floor's drawn at its deepest point's depth, behind everything stood on it, the tiles on the
 * far glass and the outlines on the tiles. Chained off a tile instead (one moved to the front), its
 * outline had gone in before it, and the tile was drawn over it
 */
static void glass_anchor(level_mesh *m, int point)
{
	jo_3d_polygon *q = &m->polygons[0];

	for (int k = 0; k < 4; ++k)
		q->vertices[k] = (unsigned short)point;
	q->normal.x = JO_FIXED_0;
	q->normal.y = JO_FIXED_0;
	q->normal.z = JO_FIXED_1;
	set_corridor_wall_attribute(&m->attributes[0], JO_COLOR_RGB(0, 0, 0));
	m->attributes[0].sort = (unsigned char)((m->attributes[0].sort & ~3) | JO_3D_SORT_MAX);
}

// The seams' white, fogged by level: twice as fast as a tile darkens (object_fog_flat_colour only
// takes off up to 16 of 31 steps: the white seams stayed grey to the far glass), so they fade out
static jo_color glass_seam_colour(int level)
{
	const int steps = 31 - (object_fog_flat_colour(GLASS_SEAM, level) & 31);

	return (jo_color)depth_fog_darken((unsigned short)GLASS_SEAM, steps * 2);
}

// One outline polygon into mesh m: a polyline (JO_VDP1_POLYLINE: a quad's four edges, nothing filled) or
// a line (JO_VDP1_LINE: from its first corner to its second), on the points given, chained (JO_3D_SORT_PREVIOUS)
static void glass_line_add(level_mesh *m, int p0, int p1, int p2, int p3, unsigned int kind, jo_color colour)
{
	colour = mirror_colour(colour);
	jo_3d_polygon *q;
	jo_3d_polygon_attribute *a;

	if (m->quad_count >= m->quads_max)
		return;
	q = &m->polygons[m->quad_count];
	a = &m->attributes[m->quad_count];
	q->normal.x = JO_FIXED_0;
	q->normal.y = JO_FIXED_0;
	q->normal.z = JO_FIXED_1;
	q->vertices[0] = (unsigned short)p0;
	q->vertices[1] = (unsigned short)p1;
	q->vertices[2] = (unsigned short)p2;
	q->vertices[3] = (unsigned short)p3;
	a->plane = JO_3D_DUAL_PLANE;
	a->sort = (unsigned char)(JO_3D_SORT_PREVIOUS | ((kind >> 16) & 0x1c) | JO_3D_NO_OPTION);
	a->texture_id = JO_3D_NO_TEXTURE;
	a->draw_mode = (unsigned short)((JO_VDP1_COLOR_MODE_RGB | JO_VDP1_NO_GOURAUD) | ((kind >> 24) & 0xc0));
	a->color = colour;
	a->gouraud_table = 0;
	a->direction = (unsigned short)(kind & 0x3f);
	++m->quad_count;
}

/*
 * A white outline on the polygon at mesh m's points first .. first + n - 1 (a tile, just added),
 * chained after it so it's drawn on it. VDP1 lines: a polyline listed a, b, c, b draws just a-b
 * and b-c (and back: the same pixels), with no line across the tile; a line (JO_VDP1_LINE) one edge.
 * whole: all of it (a tile cut by the corridor's wall or the view, or one of the scattered ones by
 * the hole). Otherwise a whole hexagon, and only its three edges from corner 2 to 5 (corridor.c
 * lists them round from its point at +out): one of each pair of opposite edges, so every seam
 * between two tiles gets exactly one line, its neighbour drawing the edge it doesn't. Outlined on
 * both sides, every seam's two lines were twice the pixels for VDP1, each drawn twice over: the
 * stairs' foot went to 30 fps
 */
static void glass_outline_add(level_mesh *m, int first, int n, bool whole, jo_color colour)
{
	if (!whole)
	{
		glass_line_add(m, first + 2, first + 3, first + 4, first + 3, JO_VDP1_POLYLINE, colour);
		glass_line_add(m, first + 4, first + 5, first + 5, first + 4, JO_VDP1_LINE, colour);
		return;
	}
	for (int k = 0; k < n; k += 2)
	{
		// An odd count: the last corner's edge back to the first, alone
		if (k + 1 >= n)
			glass_line_add(m, first + k, first, first, first, JO_VDP1_LINE, colour);
		else
			glass_line_add(m, first + k, first + k + 1, first + (k + 2 < n ? k + 2 : 0), first + k + 1, JO_VDP1_POLYLINE,
						   colour);
	}
}

/*
 * The floor's tiles are worked out on the slave (slave_draw.h, a job), not only drawn there: the
 * master was over 100% walking up the corridor while the slave idled 4-10 ms a frame, and the
 * floor was 23-28% of it (the debug line's `fl`, 2026-11-13). The master lays the far glass
 * first (its Gouraud tables come from object_fog's one count for the frame, which the master's
 * walls take from too: not to be shared between the CPUs), and leaves the job what it worked out.
 * The tiles read the corridor, the view and the fog's distances, none of which the master
 * changes before it waits for the slave (main.c's slave_draw_end), and write only glass_out and
 * the corridor's mesh
 */
static struct
{
	const corridor *passage;
	unsigned planes;
	int eye_out, near_from, near_to, tiles_from;
} glass_job;

static void glass_tiles_add(void)
{
	const corridor *passage = glass_job.passage;
	const unsigned planes = glass_job.planes;
	const int eye_out = glass_job.eye_out, near_from = glass_job.near_from, near_to = glass_job.near_to;
	const int tiles_from = glass_job.tiles_from;
	// The column of tiles being worked through (they're laid a column at a time, along out), and
	// the planes it crosses: tested once as a box, its tiles only against those
	int column_out = -1;
	unsigned column_planes = 0;

	// The tiles near you, each with its outline
	for (int t = 0; t < passage->tile_count; ++t)
	{
		const corridor_tile *tile = &passage->tiles[t];
		const int out = corridor_out(passage, tile->centre);
		frustum_point corners[CORRIDOR_TILE_CORNERS];
		// Every corner's within CORRIDOR_HEX_R of its centre: the square round that holds it
		const frustum_point centre = {tile->centre.x, tile->centre.y, 0};
		const frustum_point half = {CORRIDOR_HEX_R, CORRIDOR_HEX_R, 0};
		const int first_point = glass_out.point_count;
		unsigned need;
		int level;

		if (out < near_from - CORRIDOR_HEX_R || out > near_to + CORRIDOR_HEX_R)
			continue; // over the far glass: no tiles out there
		// A new column: the whole of it as one box, across the corridor and a hexagon along out.
		// From the city most columns are wholly outside the doorway (one test, not one a tile),
		// and a column wholly in view (0) has nothing left to test or cut. Every tile's centre in
		// a column is at the same out (corridor_open)
		if (out != column_out)
		{
			const world_point a = corridor_point(passage, out - CORRIDOR_HEX_R, passage->across_min);
			const world_point b = corridor_point(passage, out + CORRIDOR_HEX_R, passage->across_max);
			const frustum_point column_centre = {(a.x + b.x) >> 1, (a.y + b.y) >> 1, 0};
			const frustum_point column_half = {((a.x > b.x ? a.x - b.x : b.x - a.x) >> 1) + 1,
											   ((a.y > b.y ? a.y - b.y : b.y - a.y) >> 1) + 1, 0};

			column_out = out;
			column_planes = frustum_classify_box(&level_view, column_centre, column_half, planes);
		}
		if (column_planes == FRUSTUM_OUTSIDE)
			continue;
		need = column_planes != 0 ? level_planes_box(centre, half, column_planes) : 0;
		if (need == FRUSTUM_OUTSIDE)
			continue;
		if (glass_out.quad_count + 8 > glass_out.quads_max)
			break; // no room: the rest are left out this frame
		for (int k = 0; k < tile->count; ++k)
		{
			corners[k].x = tile->corners[k].x;
			corners[k].y = tile->corners[k].y;
			corners[k].z = 0;
		}
		level = object_fog_enabled ? object_fog_level(tile->centre.x, tile->centre.y) : 0;
		// (LEVEL_FOG_NONE: no Gouraud table taken, the fog's in the colour)
		if (!level_face_add(&glass_out, corners, tile->count, need, object_fog_flat_colour(glass_shades[tile->shade], level),
							LEVEL_FOG_NONE, JO_3D_SORT_PREVIOUS, false))
			continue;
		if (out > eye_out - GLASS_OUTLINED && out < eye_out + GLASS_OUTLINED)
			glass_outline_add(&glass_out, first_point, glass_out.point_count - first_point,
							  need != 0 || tile->count != 6 || out < CORRIDOR_TAIL_COLUMNS * CORRIDOR_HEX_STEP + CORRIDOR_HEX_R,
							  glass_seam_colour(level));
	}
	if (glass_out.point_count == 0)
		return; // none of the floor's on the screen
	// The deepest point of the far glass, or with none on the screen (near the corridor's end,
	// looking at it), of the tiles: the chain's first, put straight ahead of the camera at that depth
	glass_anchor(&glass_out, level_ahead(&glass_out, tiles_from > 0 ? level_deepest(&glass_out, 0, tiles_from)
																	 : level_deepest(&glass_out, 0, glass_out.point_count)));
	corridor_mesh.data.point_count = (unsigned short)glass_out.point_count;
	corridor_mesh.data.polygon_count = (unsigned short)glass_out.quad_count;
	slave_draw_mesh_here(&corridor_mesh); // on the CPU that built it
}

static void draw_glass_floor(const corridor *passage)
{
	// From the city, only the doorway's planes that can hide the floor (grey_portal)
	const unsigned planes = FRUSTUM_SCREEN | (portal.outside ? portal.floor_planes : 0);
	world_point eye_floor;
	int eye_out, near_from, near_to;

	if (level_view.eye.z <= 0)
		return; // under the floor (a roof in the city below it): its top can't be seen
	eye_floor.x = (short)level_view.eye.x;
	eye_floor.y = (short)level_view.eye.y;
	eye_out = corridor_out(passage, eye_floor);
	// Tiles only as far as they're outlined (GLASS_OUTLINED), either way: past that they're a
	// few pixels each in the fog, and the far glass in their place is a shade apart at most. Each
	// tile drawn is tested, cut and fogged: out to 1750, as before, the view looking down the
	// corridor from the yard (and in from the doorway, from the city) was a few % short of 60 fps
	near_from = JO_MAX(eye_out - GLASS_OUTLINED, 0);
	near_to = JO_MIN(eye_out + GLASS_OUTLINED, CORRIDOR_LENGTH);
	level_mesh_begin(&glass_out, corridor_points, corridor_polygons, corridor_attributes, CORRIDOR_POINTS_MAX,
					 CORRIDOR_QUADS_MAX);
	glass_out.quad_count = 1; // slot 0: the chain's first (glass_anchor)
	// The far glass either side: each a quad across the corridor, the corners fogged (Gouraud)
	for (int part = 0; part < 2; ++part)
	{
		const int from = part == 0 ? 0 : near_to;
		const int to = part == 0 ? near_from : CORRIDOR_LENGTH;
		const world_point a = corridor_point(passage, from, passage->across_min);
		const world_point b = corridor_point(passage, to, passage->across_max);
		const frustum_point c[4] = {{a.x, a.y, 0}, {b.x, a.y, 0}, {b.x, b.y, 0}, {a.x, b.y, 0}};
		unsigned need;

		if (from >= to)
			continue;
		level_cut_exact = true; // far wider than the screen: cut at its edges
		need = level_planes_rect(&c[0], &c[2], planes);
		level_cut_exact = false;
		if (need != FRUSTUM_OUTSIDE)
			level_face_add(&glass_out, c, 4, need, glass_shades[1], LEVEL_FOG_CORNERS, JO_3D_SORT_PREVIOUS, false);
	}
	// The tiles: worked out and drawn by the slave (glass_tiles_add), from what's worked out here
	glass_job.passage = passage;
	glass_job.planes = planes;
	glass_job.eye_out = eye_out;
	glass_job.near_from = near_from;
	glass_job.near_to = near_to;
	glass_job.tiles_from = glass_out.point_count;
	slave_draw_job(glass_tiles_add);
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
	// Whether the far glass is under the tiles: they're chained after it (JO_3D_SORT_PREVIOUS), all drawn
	// straight after it, far back. Without it (near the corridor's far end), the chain hung off
	// the first tile listed, by the arena's wall behind you, and the tiles were drawn over the
	// grey box's steps: each is sorted by its own furthest corner instead
	bool far_glass = false;

	if (passage == JO_NULL || !passage->open || corridor_meshes == JO_NULL)
		return;
	if (level_view_on && view_is_on_foot())
	{
		draw_glass_floor(passage); // cut to the view in 3D, and seen through the doorway from the city
		return;
	}
	if (portal.outside)
		return; // out in the city: the corridor's floor is behind its walls (grey_portal)
	// Past the detail, the rest of the corridor as one quad of glass, cut to the screen: from a
	// hexagon's width short of it, under the last tiles' jagged edge (the tiles are JO_3D_SORT_PREVIOUS after
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
	corridor_mesh.data.point_count = (unsigned short)points;
	corridor_mesh.data.polygon_count = (unsigned short)quads;
	slave_draw_mesh(&corridor_mesh); // the slave draws it (slave_draw.h)
}

/*
 * The glass itself: two streaks of light across it, half-transparent (VDP1 reads the pixel
 * behind each of theirs: 6 times a plain pixel's cost, but they're thin), just this side of it, so
 * they're drawn over the reflection: it reads as a pane, solid, not a way through
 */
#define GLINT_POINTS 24
#define GLINT_QUADS 12
static jo_pos3D_fixed glint_points[GLINT_POINTS];
static jo_3d_polygon glint_polygons[GLINT_QUADS];
static jo_3d_polygon_attribute glint_attributes[GLINT_QUADS];
static jo_3d_mesh glint_mesh;

/*
 * You (game/body.h): the skeleton's bones, each one flat strip from a joint to the next, turned to
 * face whoever's looking (a billboard along the bone: from any side a limb looks the same width,
 * as a tube would, for one quad). Filled in the skin's colour, with a rim of light round it (a
 * VDP1 polyline chained after it: 4 thin lines).
 *
 * Drawn twice from the same joints, which is the point of it: from your own eyes, in the world's
 * matrix (level_body_draw(true): no head, you're inside it), and in the mirror's (false), where
 * level_view is the reflected eye's. So what you see of your arms is what the mirror shows.
 *
 * Finer than the world's whole units: a hand's 20 units from your eye, where a unit is 8 pixels,
 * and joints snapped to whole units would judder. The body's worked out x16 (BODY_SUB), and cut by
 * a copy of the view with its eye and its planes' distances x16 too: the same planes, in the finer
 * measure. It hangs from the camera's eye (whole units, as the camera moves), so it's steady in
 * your view.
 *
 * Call it between slave_draw_begin and slave_draw_end, each pass.
 *
 * Cost: a division a bone for the strip's across (body_across), up to two more for the edges it
 * shares with its neighbours (body_mitre): 18 bones a pass, never per pixel or per corner. And the
 * cut's, only for a bone crossing the near plane or an edge
 */
#define BODY_FAR 500 // further than this from the mirror's eye, a silhouette (level_body_draw)
#define BODY_POINTS 112
#define BODY_QUADS 80
static jo_pos3D_fixed body_points[BODY_POINTS];
static jo_3d_polygon body_polygons[BODY_QUADS];
static jo_3d_polygon_attribute body_attributes[BODY_QUADS];
static jo_3d_mesh body_mesh;
static frustum_point body_world[BODY_JOINTS]; // the joints in the world, x16
static bool body_on = false;				  // this frame: there's a body to draw
static const body_skin *body_look = &body_skin_glass;

void level_body_clear(void)
{
	body_on = false;
}

void level_body_set(body *b, body_state *state, int yaw, int feet)
{
	// The way you face, and your right, x1024 (walker.h: yaw 0 faces north, -y; right is east, -x)
	const int forward_x = -walker_sin(yaw), forward_y = -walker_cos(yaw);
	const int right_x = forward_y, right_y = -forward_x;
	int ground;

	body_on = false;
	if (!level_view_on)
		return;
	// Your eye is the camera's: however high that is above your feet now (its bob, a landing's dip,
	// lying after the crash). Never lower than a head lying on the floor
	state->eye = (level_view.eye.z - feet) << BODY_SUB_SHIFT;
	// (climbing, your feet are on the ledge's top already and the eye's still below it: negative)
	if (state->eye < 10 * BODY_SUB && state->mode != BODY_CLIMB)
		state->eye = 10 * BODY_SUB;
	body_update(b, state);
	ground = (level_view.eye.z << BODY_SUB_SHIFT) - state->eye;
	for (int k = 0; k < BODY_JOINTS; ++k)
	{
		const body_point *j = &b->joint[k];

		body_world[k].x = (level_view.eye.x << BODY_SUB_SHIFT) + ((j->x * right_x + j->y * forward_x) >> 10);
		body_world[k].y = (level_view.eye.y << BODY_SUB_SHIFT) + ((j->x * right_y + j->y * forward_y) >> 10);
		body_world[k].z = ground + j->z;
	}
	body_on = true;
}

/*
 * Across bone i, as the view (fine: x16) sees it: square to the bone and to the line of sight from
 * the eye to its middle (their cross product), 1024 long. false if it's seen end on (looking
 * straight down your own arm): nothing to show. The one division a bone: a reciprocal of the
 * cross product's length, then multiplies
 */
static bool body_across(int i, const frustum *fine, int *across)
{
	const frustum_point a = body_world[body_bones[i].from], b = body_world[body_bones[i].to];
	const int along_x = b.x - a.x, along_y = b.y - a.y, along_z = b.z - a.z;
	// From the eye to the bone's middle: only its direction matters, so it's halved until it fits
	// 10 bits (the products below stay in 32)
	int to_x = ((a.x + b.x) >> 1) - fine->eye.x, to_y = ((a.y + b.y) >> 1) - fine->eye.y, to_z = ((a.z + b.z) >> 1) - fine->eye.z;
	int x, y, z, length, recip;

	while (to_x > 1023 || to_x < -1023 || to_y > 1023 || to_y < -1023 || to_z > 1023 || to_z < -1023)
	{
		to_x >>= 1;
		to_y >>= 1;
		to_z >>= 1;
	}
	x = (along_y * to_z - along_z * to_y) >> 6;
	y = (along_z * to_x - along_x * to_z) >> 6;
	z = (along_x * to_y - along_y * to_x) >> 6;
	length = frustum_isqrt((unsigned int)(x * x + y * y + z * z));
	if (length < 8)
		return false;
	recip = (1 << 24) / length;
	across[0] = (x * recip) >> 14;
	across[1] = (y * recip) >> 14;
	across[2] = (z * recip) >> 14;
	return true;
}

/*
 * The edge two strips share where one bone ends and the next begins (an elbow, a knee): halfway
 * between their two acrosses, so neither strip pinches or sticks out at the bend: a mitre, as a
 * picture frame's corner. The sum of the two (each 1024 long) points the right way and is
 * 2 cos(t) long, t half the angle between them; the edge has to be 1 / cos(t), longer the sharper
 * the bend, to keep both strips their width. So it's the sum x 2 / |sum|^2 (x1024 again): one
 * division a joint, no square root. false past 120 degrees between them (the edge would be over
 * twice the width, a spike): each strip keeps its own square end there
 */
static bool body_mitre(const int *one, const int *other, int *edge)
{
	const int x = one[0] + other[0], y = one[1] + other[1], z = one[2] + other[2];
	const int size = (x * x + y * y + z * z) >> 10; // |sum|^2, 4096 for the same across twice
	int scale;

	if (size < 1024)
		return false;
	scale = (2048 << 10) / size;
	edge[0] = (x * scale) >> 10;
	edge[1] = (y * scale) >> 10;
	edge[2] = (z * scale) >> 10;
	return true;
}

static void body_build(bool own_eyes, bool in_job); // below

/*
 * In the mirror, your body's worked out on the slave as well as drawn there (slave_draw.h, a job):
 * the mirror's pass was 37-54% of a frame at the glass (cpu 116-127%, 47-51 fps), the body 8-15%
 * of it on the master, while the slave idled. Queued first in the pass (main.c), it's built while
 * the master works out the reflected boxes. It reads the joints (level_body_set, before both
 * passes), the mirror's view and its tint (set before the pass, put back after slave_draw_end),
 * and writes only the body's own mesh. From your own eyes it's built here: it's the main pass's
 * last thing, so there's nothing for the master to do meanwhile
 */
static void body_mirror_job(void)
{
	body_build(false, true);
}

void level_body_draw(bool own_eyes)
{
	if (own_eyes)
		body_build(true, false);
	else
		slave_draw_job(body_mirror_job);
}

static void body_build(bool own_eyes, bool in_job)
{
	// Your own eyes: cut at the near plane and the guard band only (VDP1 clips the rest for less
	// than a cut costs). The mirror: at the glass's edges too (planes 1 .. 4 there)
	const unsigned cut_by = own_eyes ? FRUSTUM_GUARD : FRUSTUM_SCREEN | FRUSTUM_GUARD;
	frustum fine; // level_view in the body's x16
	level_mesh m;
	bool far_off = false;
	int across[BODY_BONES][3]; // each bone's across, 1024 long (body_across)
	bool seen[BODY_BONES];	   // drawn this pass, and not end on

	if (!body_on || !level_view_on)
		return;
	/*
	 * Far off in the mirror (over BODY_FAR from the reflected eye: you're under 30 pixels tall), a
	 * silhouette: each strip filled in its rim's colour, no rim drawn, no hands or feet (a pixel
	 * or two each). 11 polygons for 36: every polygon is ~0.45% of a frame of jo's work whoever
	 * draws it (the body in the mirror was 12%: film 2026-11-10), and at that size the rims were
	 * all you saw of it anyway
	 */
	if (!own_eyes)
	{
		const int dx = body_world[BODY_NECK].x - (level_view.eye.x << BODY_SUB_SHIFT);
		const int dy = body_world[BODY_NECK].y - (level_view.eye.y << BODY_SUB_SHIFT);

		far_off = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) > BODY_FAR * BODY_SUB;
	}
	fine = level_view;
	fine.eye.x <<= BODY_SUB_SHIFT;
	fine.eye.y <<= BODY_SUB_SHIFT;
	fine.eye.z <<= BODY_SUB_SHIFT;
	for (int i = 0; i < fine.count; ++i)
		fine.planes[i].offset <<= BODY_SUB_SHIFT;
	level_mesh_begin(&m, body_points, body_polygons, body_attributes, BODY_POINTS, BODY_QUADS);
	// Each bone's across first: a strip's ends need its neighbours' too
	for (int i = 0; i < BODY_BONES; ++i)
	{
		const int part = body_bones[i].part;

		seen[i] = !(own_eyes && i < BODY_BONES_HEAD) && !(far_off && (part == BODY_PART_HAND || part == BODY_PART_FOOT)) &&
				  body_across(i, &fine, across[i]);
	}
	for (int i = 0; i < BODY_BONES; ++i)
	{
		const body_bone *bone = &body_bones[i];
		const frustum_point a = body_world[bone->from], b = body_world[bone->to];
		const jo_color fill = (jo_color)(far_off ? body_look->rim[bone->part] : body_look->fill[bone->part]);
		int start[3], end[3], n;
		frustum_point corners[4], cut[FRUSTUM_POLY_MAX];
		const frustum_point *p = corners;
		unsigned planes;
		const int first = m.point_count;

		if (!seen[i])
			continue;
		// Its two ends' edges: shared with the bone before and the bone after in a limb (the
		// table lists a limb's bones in a row: each starts at the joint the last ended at)
		for (int k = 0; k < 3; ++k)
			start[k] = end[k] = across[i][k];
		if (i > 0 && body_bones[i - 1].to == bone->from && seen[i - 1])
			body_mitre(across[i - 1], across[i], start);
		if (i + 1 < BODY_BONES && body_bones[i + 1].from == bone->to && seen[i + 1])
			body_mitre(across[i], across[i + 1], end);
		for (int k = 0; k < 4; ++k)
		{
			const frustum_point *joint = k < 2 ? &a : &b;
			const int *edge = k < 2 ? start : end;
			const int half = (k == 0 || k == 3 ? -1 : 1) * (k < 2 ? bone->half_from : bone->half_to);

			corners[k].x = joint->x + ((edge[0] * half) >> 10);
			corners[k].y = joint->y + ((edge[1] * half) >> 10);
			corners[k].z = joint->z + ((edge[2] * half) >> 10);
		}
		planes = frustum_classify(&fine, corners, 4, FRUSTUM_SCREEN | FRUSTUM_GUARD);
		if (planes == FRUSTUM_OUTSIDE)
			continue;
		n = 4;
		if (planes & cut_by)
		{
			n = frustum_clip(&fine, planes & cut_by, corners, 4, cut);
			p = cut;
		}
		if (n < 3 || m.point_count + n > m.points_max || m.quad_count + (n - 1) / 2 + n > m.quads_max)
			continue;
		// x16 to 16.16: 12 more bits
		for (int k = 0; k < n; ++k)
		{
			m.points[first + k].x = p[k].x << (16 - BODY_SUB_SHIFT);
			m.points[first + k].y = p[k].y << (16 - BODY_SUB_SHIFT);
			m.points[first + k].z = p[k].z << (16 - BODY_SUB_SHIFT);
		}
		m.point_count += n;
		// A fan of quads, as level_face_add's
		for (int k = 1; k < n - 1; k += 2)
		{
			jo_3d_polygon *q = &m.polygons[m.quad_count];
			jo_3d_polygon_attribute *at = &m.attributes[m.quad_count];
			const int last = k + 2 < n ? k + 2 : n - 1;

			q->normal.x = JO_FIXED_0;
			q->normal.y = JO_FIXED_0;
			q->normal.z = JO_FIXED_1;
			q->vertices[0] = (unsigned short)first;
			q->vertices[1] = (unsigned short)(first + k);
			q->vertices[2] = (unsigned short)(first + k + 1);
			q->vertices[3] = (unsigned short)(first + last);
			set_corridor_wall_attribute(at, mirror_colour(fill));
			at->sort = (unsigned char)((at->sort & ~3) | JO_3D_SORT_CENTER);
			++m.quad_count;
		}
		// The rim of light, drawn straight after it: one polyline round a whole strip; round a cut
		// one (more corners), a line an edge
		if (far_off)
			continue;
		if (n == 4)
			glass_line_add(&m, first, first + 1, first + 2, first + 3, JO_VDP1_POLYLINE, (jo_color)body_look->rim[bone->part]);
		else
			for (int k = 0; k < n; ++k)
			{
				const int next = first + (k + 1 < n ? k + 1 : 0);

				glass_line_add(&m, first + k, next, next, first + k, JO_VDP1_LINE, (jo_color)body_look->rim[bone->part]);
			}
	}
	if (m.quad_count == 0)
		return;
	body_mesh.data.points = body_points;
	body_mesh.data.polygons = body_polygons;
	body_mesh.data.attributes = body_attributes;
	body_mesh.data.point_count = (unsigned short)m.point_count;
	body_mesh.data.polygon_count = (unsigned short)m.quad_count;
	// The slave draws it (slave_draw.h): on the master its 28 polygons were 13% of a frame in the
	// mirror (film 2026-11-10). Its arrays aren't touched again until the slave's done: the second
	// pass comes after slave_draw_end
	if (in_job)
		slave_draw_mesh_here(&body_mesh); // on the slave, which built it
	else
		slave_draw_mesh(&body_mesh);
}

/*
 * They move as a reflection's light does: each is a band GLINT_BAND tall, slanting up the glass,
 * and how high it is on the glass goes with how far you are from it, so walking at the glass the
 * streaks slide down it, and back up as you walk away (the user, 2026-11-14: "the mirror blue
 * effect needs to scroll down when moving closer, rather than being fixed"). A unit nearer is half
 * a unit down; the band goes up the glass and off its top, and after a gap comes up from below
 * again, every GLINT_PERIOD of distance (a power of two: a mask, not a remainder). The second runs
 * half a period behind the first. Each is cut to the pane's height; where it crosses the pane is
 * the slant's across at those heights: the pane's ~506 high, taken as 512 (a shift, no division)
 */
#define GLINT_BAND 256
#define GLINT_PERIOD 1024

static void mirror_glints(const corridor *passage)
{
	// Each streak: a slanting band up the glass, from (bottom across, top across) in sixteenths of
	// its width: a sixteenth of it wide (the first), half that (the second)
	static const unsigned char streaks[2][2] = {{3, 7}, {9, 12}};
	const int width = passage->across_max - passage->across_min;
	const int band = width >> 4; // a sixteenth across: the wider streak
	world_point eye;
	int eye_out;
	level_mesh m;

	eye.x = (short)level_view.eye.x;
	eye.y = (short)level_view.eye.y;
	eye_out = corridor_out(passage, eye);
	level_mesh_begin(&m, glint_points, glint_polygons, glint_attributes, GLINT_POINTS, GLINT_QUADS);
	for (int k = 0; k < 2; ++k)
	{
		const int bottom = passage->across_min + ((width * streaks[k][0]) >> 4);
		const int top = passage->across_min + ((width * streaks[k][1]) >> 4);
		const int wide = k == 0 ? band : band >> 1;
		// The band's top, up from the glass's foot: half your distance, round the period
		const int phase = ((eye_out >> 1) + k * (GLINT_PERIOD >> 1)) & (GLINT_PERIOD - 1);
		int z_high = WALL_BOTTOM_Z + phase, z_low = z_high - GLINT_BAND;
		/*
		 * And across with it: the slant alone moved the band ~0.4 across for each unit down, nearly
		 * straight down, where a real reflection's light slides diagonally (the user, 2026-11-14).
		 * The whole slant drifts 3/8 of a unit across for each unit the band moves, the same way the
		 * slant leans: ~0.8 across a unit down in all, about 40 degrees. Centred (less 3/16 of the
		 * period), so it drifts either side of where it was drawn
		 */
		const int drift = ((phase * 3) >> 3) - ((GLINT_PERIOD * 3) >> 4);
		frustum_point corners[4];
		const int first = m.quad_count;
		int at_low, at_high;

		z_high = z_high > CORRIDOR_TOP_ON_FOOT ? CORRIDOR_TOP_ON_FOOT : z_high;
		z_low = z_low < WALL_BOTTOM_Z ? WALL_BOTTOM_Z : z_low;
		if (z_high - z_low < 8)
			continue; // off the glass, or a sliver at its edge
		// Across where the slant is at those heights, drifted; kept on the glass (a band past its
		// side would shine on the wall)
		at_low = bottom + drift + (((top - bottom) * (z_low - WALL_BOTTOM_Z)) >> 9);
		at_high = bottom + drift + (((top - bottom) * (z_high - WALL_BOTTOM_Z)) >> 9);
		at_low = at_low < passage->across_min ? passage->across_min : at_low;
		at_high = at_high > passage->across_max - wide ? passage->across_max - wide : at_high;
		if (at_low > passage->across_max - wide || at_high < passage->across_min)
			continue; // drifted right off the glass
		corners[0] = grey_point(corridor_point(passage, 1, at_low), z_low);
		corners[1] = grey_point(corridor_point(passage, 1, at_low + wide), z_low);
		corners[2] = grey_point(corridor_point(passage, 1, at_high + wide), z_high);
		corners[3] = grey_point(corridor_point(passage, 1, at_high), z_high);
		if (!level_face_add(&m, corners, 4, FRUSTUM_GUARD, JO_COLOR_RGB(70, 100, 150), LEVEL_FOG_NONE,
							JO_3D_SORT_CENTER, false))
			continue;
		for (int q = first; q < m.quad_count; ++q)
			glint_attributes[q].draw_mode =
				(unsigned short)((glint_attributes[q].draw_mode & ~7) | JO_VDP1_COLOR_CALC_HALF_TRANSPARENT);
	}
	if (m.quad_count == 0)
		return;
	glint_mesh.data.points = glint_points;
	glint_mesh.data.polygons = glint_polygons;
	glint_mesh.data.attributes = glint_attributes;
	glint_mesh.data.point_count = (unsigned short)m.point_count;
	glint_mesh.data.polygon_count = (unsigned short)m.quad_count;
	jo_3d_mesh_draw(&glint_mesh);
}

void level_mirror_end(const corridor *passage, const wall_clip_camera *camera)
{
	level_view = mirror_saved;
	portal = mirror_saved_portal;
	if (object_fog_enabled && camera != JO_NULL)
		object_fog_set_camera(camera->eye, camera->forward_x, camera->forward_y);
	mirror_shade = 0;
	// In the world's own matrix again: the glass's streaks over it all
	mirror_glints(passage);
}

// The end of the reflection: a wall across the corridor at MIRROR_DEPTH, near black, behind
// everything else reflected (sorted by its furthest corner). Drawn in the mirror's matrix, cut by
// the reflected view as the rest is
// (mirror_far_wall itself is below disc_add: it draws the disc's sectors in the glass)
#define FAR_FACES 10 // the wall, and the disc's 8 sectors (and one to spare)
#define FAR_POINTS (FAR_FACES * FRUSTUM_POLY_MAX)
#define FAR_QUADS (FAR_FACES * (FRUSTUM_POLY_MAX / 2))
static jo_pos3D_fixed far_points[FAR_POINTS];
static jo_3d_polygon far_polygons[FAR_QUADS];
static jo_3d_polygon_attribute far_attributes[FAR_QUADS];
static jo_3d_mesh far_mesh;

/*
 * The sky's furniture: the landmarks far off (game/landmarks.h) and the CD block's disc over the
 * city (game/city.h). All flat polygons through the same cut as everything on foot
 * (level_face_add), in one mesh the slave draws.
 *
 * Only when there's a view to cut to (level_view_on: 1st person, cycle or feet). The views from
 * above look down at the floor and never see the horizon, and without the cut every landmark
 * behind the camera would still go to jo to be thrown away, ~0.45% of a frame a polygon.
 * A landmark's box is tested first: out of view, none of its shapes are looked at.
 * Worst case in view at once: two or three landmarks and the disc, ~25 polygons
 */
#define SKY_POINTS 320
#define SKY_QUADS 96
static jo_pos3D_fixed sky_points[SKY_POINTS];
static jo_3d_polygon sky_polygons[SKY_QUADS];
static jo_3d_polygon_attribute sky_attributes[SKY_QUADS];
static jo_3d_mesh sky_mesh;

/*
 * Whether the corridor's walls hide all of it (the box round its corners): from inside the
 * corridor, the walls stand beside you, and their shadow (game/occluder.h, arena_hiders: set up
 * this frame by draw_arena_walls on foot) takes in nearly the whole sky but over the doorway and
 * the strip of sky above the walls' tops. The corridor had no headroom (cpu 99% at 60 fps), and
 * the sky's ~20 polygons there, most of them behind its walls, took it to 51-58 (film
 * 2026-11-12). occluder_hides wants the box within 32 000 of the eye: anything further's drawn
 */
static bool sky_hidden(const frustum_point *c, int n)
{
	int x_min = c[0].x, x_max = c[0].x, y_min = c[0].y, y_max = c[0].y, z_min = c[0].z, z_max = c[0].z;
	frustum_point centre, half;
	const frustum_point *e = &level_view.eye;

	if (!arena_culling)
		return false;
	for (int k = 1; k < n; ++k)
	{
		x_min = JO_MIN(x_min, c[k].x);
		x_max = JO_MAX(x_max, c[k].x);
		y_min = JO_MIN(y_min, c[k].y);
		y_max = JO_MAX(y_max, c[k].y);
		z_min = JO_MIN(z_min, c[k].z);
		z_max = JO_MAX(z_max, c[k].z);
	}
	if (x_min < e->x - 30000 || x_max > e->x + 30000 || y_min < e->y - 30000 || y_max > e->y + 30000)
		return false;
	centre.x = (x_min + x_max) >> 1;
	centre.y = (y_min + y_max) >> 1;
	centre.z = (z_min + z_max) >> 1;
	half.x = ((x_max - x_min) >> 1) + 1;
	half.y = ((y_max - y_min) >> 1) + 1;
	half.z = ((z_max - z_min) >> 1) + 1;
	for (int i = 0; i < CORRIDOR_FACES; ++i)
		if (occluder_hides(&arena_hiders[i], centre, half))
			return true;
	return false;
}

// A flat shape, cut to the view by the screen's edges it crosses (none: as it is; all outside
// one: not at all), unless the corridor's walls hide it. (Cut only past the guard band, as the
// towers are, VDP1 filled more off the screen and the corridor got slower: tried 2026-11-12)
// (and by any more planes: extra, their bits; a landmark's cut by the city's ring, landmark_ring_cut)
static void sky_face_cut(level_mesh *m, const frustum_point *c, jo_color colour, unsigned extra)
{
	const unsigned crossed = frustum_classify(&level_view, c, 4, FRUSTUM_SCREEN | extra);

	if (crossed == FRUSTUM_OUTSIDE || sky_hidden(c, 4))
		return;
	level_face_add(m, c, 4, crossed, colour, LEVEL_FOG_NONE, JO_3D_SORT_CENTER, false);
}

static void sky_face(level_mesh *m, const frustum_point *c, jo_color colour)
{
	sky_face_cut(m, c, colour, 0);
}

/*
 * Work RAM's city, all round (Akira's Neo-Tokyo: towers to the horizon every way you look): a ring
 * of flat strips facing in, textured with the skyline (tools/make_skyline.py: three rows of towers
 * in one picture, far to near, see-through above them, solid black at their feet).
 *
 * The parallax: the ring's centred not on the arena's middle but on a point that follows you,
 * three quarters of the way. A ring that followed you all the way would be at infinity: it would
 * never move against anything, as the sky doesn't. One that didn't follow at all would be as near
 * as it's drawn. Three quarters, it seems four times as far as it is, and slides as you move
 * against the landmarks, the towers and the arena's walls, which don't follow at all. That's why
 * it's polygons and not a VDP2 picture: a VDP2 layer can only slide as you turn, not as you move,
 * and nothing in the world could stand in front of it. (Three rings, each its own layer following
 * a different share, were tried first: three times the polygons, and the user saw little of the
 * difference: one ring, the depth painted into its texture.)
 *
 * Each strip's a textured quad, so it isn't cut to the view (cut, VDP1 would stretch the whole
 * texture over the piece): it's drawn whole if it's wholly in front of you, on the screen at all,
 * and within the guard band (jo cuts what reaches past it, with the CPU, and VDP1 walks every
 * off-screen pixel of what it's given). So a strip must be narrow enough that one crossing the
 * screen's edge never reaches the guard band's (on foot the screen's edge is 37 degrees aside, the
 * guard band's 57: under 20 degrees wide). With 16 strips, up to 35 degrees each from where you
 * stand in the city, whole strips went as you turned, and looking up (a tall strip's top corners
 * swing out wide), and the sky's pink horizon showed where they'd been (the user, 2026-11-13).
 * 32 strips now, under 17 degrees each: each shows half the picture (SKYLINE.TGA's left half on
 * the even ones, its right on the odd: two sprites cut from the one file), so the skyline's
 * the same, in narrower pieces. 8 to 11 in view.
 *
 * In the arena it stands on the floor, its black foot under the walls: when the walls sink
 * between rounds there's city behind them, not a gap of sky. In the city it's nearer (a smaller
 * ring round you: SKYLINE_RADIUS_CITY) and stands higher over you, so it towers as the city's own
 * do; its foot's just under the horizon (the board's haze is below it), its top
 * SKYLINE_ABOVE_CITY over the eye. Riding the lift up past that, the top stays and the foot rises
 * with you: the city sinks below. World coordinates must stay under 32 768 (jo's 16.16): the
 * centre's at most 3/4 of 14 000 out (the city's far edge), plus the radius
 */
#define SKYLINE_STRIPS 32
#define SKYLINE_FOLLOW 192		  // of 256
#define SKYLINE_RING_CLEAR 6000	  // round the arena: this and 1.5 x its half width off (skyline_add)
#define SKYLINE_RADIUS_MAX 24000  // and no further
#define SKYLINE_RING_MARGIN 600	  // the arena's corners kept this far inside it
#define SKYLINE_TOP_X256 70		  // its height there, of its radius: 4400 at 16 000, as it was drawn
#define SKYLINE_RADIUS_CITY 10000 // on foot: nearer, 6500 off at the least
#define SKYLINE_TOP 4400		  // in the arena; and no higher on foot
#define SKYLINE_ABOVE_CITY 3600	  // on foot, its top over the eye
// and its foot under it: just under the horizon. All it has to do down there is cover the sky's
// last bright band at the horizon: below that the board fades into its fog (main.c, BOARD_HAZE),
// and every row of a strip is VDP1's to fill (from 900 under, the ring took the CD block's hub
// from 53-60 fps to 43-53: hub.steps with the sky and without, 2026-11-13)
#define SKYLINE_BELOW_CITY 400
#define SKYLINE_CORRIDOR_FOOT 1500 // in the corridor: what's lower is behind its walls
#define SKYLINE_BEYOND 4000		   // but not the strips this far out past the corridor's end (skyline_add)
/*
 * The city's ring as skyline_add drew it this frame, for the landmarks drawn after it
 * (landmark_ring_cut): its centre, radius and foot. on: the near, tall ring round you in the city
 */
static struct
{
	bool on;
	int cx, cy, r, foot;
} sky_ring;
static jo_3d_polygon_attribute skyline_attribute[2]; // the picture's left half, and its right
// The ring's corners: cos and sin of 0, 11.25, 22.5 ... degrees, x1024 (skyline_load fills it in
// from a quarter turn's worth)
static short skyline_round[SKYLINE_STRIPS][2];
static bool skyline_loaded = false;

static void skyline_load(void)
{
	// cos of 0, 11.25 ... 90 degrees, x1024: the rest of the circle by symmetry
	static const short quarter[9] = {1024, 1004, 946, 851, 724, 569, 392, 200, 0};
	// The picture's two halves (128 x 64: tools/make_skyline.py), each a sprite of its own
	static const jo_tile halves[2] = {{0, 0, 64, 64}, {64, 0, 64, 64}};
	jo_3d_mesh setter; // jo's texture setter works on a mesh: pointed at the one attribute
	// Magenta above the towers: see-through
	const int id = jo_sprite_add_tga_tileset(JO_ROOT_DIR, "SKYLINE.TGA", JO_COLOR_RGB(255, 0, 255), halves, 2);

	for (int k = 0; k < SKYLINE_STRIPS; ++k)
	{
		const int q = k & 7, turn = k >> 3; // which quarter turn, and how far into it
		const int c = quarter[q], sn = quarter[8 - q];

		// A quarter turn on: (cos, sin) becomes (-sin, cos)
		skyline_round[k][0] = (short)(turn == 0 ? c : turn == 1 ? -sn : turn == 2 ? -c : sn);
		skyline_round[k][1] = (short)(turn == 0 ? sn : turn == 1 ? c : turn == 2 ? -sn : -c);
	}
	if (id < 0)
		return;
	for (int h = 0; h < 2; ++h)
	{
		setter.data.polygon_count = 1;
		setter.data.attributes = &skyline_attribute[h];
		// As the glass's: jo_3d_create_plane's defaults, so jo's setter behaves the same
		JO_ZERO(skyline_attribute[h].sort);
		JO_ZERO(skyline_attribute[h].draw_mode);
		JO_ZERO(skyline_attribute[h].gouraud_table);
		skyline_attribute[h].plane = JO_3D_DUAL_PLANE;
		jo_3d_set_mesh_polygon_texture(&setter, id + h, 0);
		// High speed shrink (VDP1's manual; jo's setter): drawn smaller than its texture, VDP1 reads
		// every other texel, not all of them. The ring's nearly always drawn smaller
		jo_3d_set_mesh_polygon_high_speed_shrink(&setter, true, 0);
	}
	skyline_loaded = true;
}

// A textured quad, whole: corners as the texture's top left, top right, bottom right, bottom left
static void sky_textured(level_mesh *m, const frustum_point *c, const jo_3d_polygon_attribute *texture)
{
	jo_3d_polygon *q;

	if (m->point_count + 4 > m->points_max || m->quad_count + 1 > m->quads_max)
		return;
	q = &m->polygons[m->quad_count];
	for (int k = 0; k < 4; ++k)
	{
		m->points[m->point_count + k].x = jo_int2fixed(c[k].x);
		m->points[m->point_count + k].y = jo_int2fixed(c[k].y);
		m->points[m->point_count + k].z = jo_int2fixed(c[k].z);
		q->vertices[k] = (unsigned short)(m->point_count + k);
	}
	q->normal.x = JO_FIXED_0;
	q->normal.y = JO_FIXED_0;
	q->normal.z = JO_FIXED_1;
	m->attributes[m->quad_count] = *texture;
	++m->quad_count;
	m->point_count += 4;
}

static void skyline_add(level_mesh *m, int place, int side, int wall, int middle)
{
	const short (*round)[2] = skyline_round;
	const frustum_point *e = &level_view.eye;
	/*
	 * Out in the city (past the doorway: portal.outside), the near, tall ring. In the arena and in
	 * the corridor, the far one standing on the floor: from the corridor the near one filled most
	 * of the sky over its walls, and the sky there cost VDP1 every frame (busy 100%, 0 without) and
	 * the CPU 15-25%, where it had no headroom (corridor_still.steps, with and without, 2026-11-12)
	 */
	const bool city = place != PLACE_ARENA && portal.outside;
	/*
	 * Round the arena, the ring must stay outside it: the arena's walls always in front (the
	 * user, 2026-11-13: "the skyline overlaps the arena when zooming in"). Its centre follows you
	 * three quarters of the way, and on the flight down into the cycle the camera's up to 14 000
	 * out: the centre went 10 500 out with it, and the ring, 16 000 round that, cut across the
	 * arena's far side, in front of its walls. (Riding by a wall, a far corner of the arena just
	 * poked through it too.) So:
	 *   - it's further off, and more so for a bigger arena: half as far again as the arena's half
	 *     width, and 6000 (18 000 for the first rounds' arena, 16 000 across; no more than 24 000)
	 *   - its centre's kept near enough the arena's middle, each way, that the arena's furthest
	 *     corner is still inside it: 0.7 x the radius, less the half width and a margin. (A point
	 *     that far out both ways is the radius away: the ring's flat strips cut 0.5% inside that,
	 *     so the margin.) Inside that the parallax is as it was; further out it holds still
	 * And it's taller by as much as it's further, so it stands as high on the screen.
	 * World coordinates stay under 32 768 (jo's 16.16): at most 24 000 + 2400
	 */
	const int half = arena_half();
	const int r_arena = JO_MIN(half + (half >> 1) + SKYLINE_RING_CLEAR, SKYLINE_RADIUS_MAX);
	const int reach = JO_MAX(((r_arena * 181) >> 8) - half - SKYLINE_RING_MARGIN, 0); // 181 / 256: 0.707
	const int fx = (e->x * SKYLINE_FOLLOW) >> 8, fy = (e->y * SKYLINE_FOLLOW) >> 8;
	const int cx = city ? fx : JO_MAX(-reach, JO_MIN(reach, fx)), cy = city ? fy : JO_MAX(-reach, JO_MIN(reach, fy));
	const int r = city ? SKYLINE_RADIUS_CITY : r_arena;
	const int arena_top = (r_arena * SKYLINE_TOP_X256) >> 8;
	/*
	 * Its foot: in the arena the floor (its black base fills in under the walls as they sink). In
	 * the corridor, SKYLINE_CORRIDOR_FOOT: the corridor's walls hide everything lower from in
	 * there, and VDP1 has no depth buffer: drawn from the floor, it filled the whole hidden lower
	 * part of every strip behind the walls, the same polygons and VDP1 busy 100% against 0
	 * (corridor_still.steps, 2026-11-12)
	 */
	const int foot = city ? JO_MAX(e->z - SKYLINE_BELOW_CITY, CITY_GROUND) : place != PLACE_ARENA ? SKYLINE_CORRIDOR_FOOT : 0;
	const int top = city ? JO_MIN(e->z + SKYLINE_ABOVE_CITY, SKYLINE_TOP) : arena_top;

	/*
	 * But in the corridor, the strips out past its far end stand on the horizon, as the city's
	 * ring does (SKYLINE_BELOW_CITY under your eye): they're what's seen through the far doorway,
	 * over the city. Standing from SKYLINE_CORRIDOR_FOOT like the rest, the sky's pink horizon
	 * showed under them from the stairs on, until you stepped out and the city's ring took over:
	 * its black foot popped in (the user, 2026-11-13). A few strips, a band ~25 pixels high.
	 * Which they are: how far out the strip's middle is, along the corridor (wall_point to
	 * out_point is 1024 of it: a dot product against that, no division)
	 */
	const bool corridor_ring = !city && place != PLACE_ARENA;
	const world_point wall_point = corridor_point_behind(side, wall, 0, middle);
	const world_point out_point = corridor_point_behind(side, wall, 1024, middle);
	const int out_x = out_point.x - wall_point.x, out_y = out_point.y - wall_point.y; // 1024 long

	sky_ring.on = city && skyline_loaded && foot < top;
	sky_ring.cx = cx;
	sky_ring.cy = cy;
	sky_ring.r = r;
	sky_ring.foot = foot;
	if (!skyline_loaded || foot >= top)
		return;
	for (int k = 0; k < SKYLINE_STRIPS; ++k)
	{
		const int next = (k + 1) & (SKYLINE_STRIPS - 1);
		const int x0 = cx + ((round[k][0] * r) >> 10), y0 = cy + ((round[k][1] * r) >> 10);
		const int x1 = cx + ((round[next][0] * r) >> 10), y1 = cy + ((round[next][1] * r) >> 10);
		// (its middle's out, x1024: the two ends' halved. Under 2^15 x 2^10 each: inside 32 bits)
		const bool beyond = corridor_ring && (((x0 + x1) >> 1) - wall_point.x) * out_x + (((y0 + y1) >> 1) - wall_point.y) * out_y >
												 (CORRIDOR_LENGTH + SKYLINE_BEYOND) * 1024;
		const int strip_foot = beyond ? JO_MIN(foot, e->z - SKYLINE_BELOW_CITY) : foot;
		const frustum_point c[4] = {{x0, y0, top}, {x1, y1, top}, {x1, y1, strip_foot}, {x0, y0, strip_foot}};
		const unsigned crossed = frustum_classify(&level_view, c, 4, FRUSTUM_SCREEN);

		// On the screen at all, wholly in front of the eye, and inside the guard band
		if (crossed == FRUSTUM_OUTSIDE || (crossed & (1u << FRUSTUM_NEAR)) != 0 ||
			(crossed != 0 && frustum_classify(&level_view, c, 4, FRUSTUM_GUARD & ~1u) != 0))
			continue;
		/*
		 * In the city, not if a nearer, taller tower hides it (game/skyline.h: this frame's
		 * hiders, picked as the towers were drawn just before): the box round the strip on the
		 * plan, as high as its top
		 */
		if (city)
		{
			const skyline_box strip = {(short)JO_MIN(x0, x1), (short)JO_MAX(x0, x1), (short)JO_MIN(y0, y1), (short)JO_MAX(y0, y1),
									   (short)top};

			if (skyline_hides(&city_skyline, &strip))
				continue;
		}
		if (!sky_hidden(c, 4))
			sky_textured(m, c, &skyline_attribute[k & 1]);
	}
}

/*
 * The landmarks, and a trick played in the city (the user's idea, 2026-11-12). Down among the
 * towers you see little sky, and a landmark seen only now and then over a roof can be made bigger
 * and nearer than it is, so it towers over you: nobody can tell, as long as it's never seen
 * changing. So each has its own nearness and size (landmark_near, landmark_size, x256): its foot
 * is drawn that share of the way from you to where it is, and it's that much larger. They ease
 * towards the place's values (arena or city) only on frames when the landmark's not on the
 * screen. (Drawn nearer by a share of the way from you, it still slides against what's behind it
 * as it would where it is: the share's the same either side.)
 */
#define LANDMARK_NEAR_ARENA 256 // where they are,
#define LANDMARK_SIZE_ARENA 384 // half as big again, to stand over the skyline
#define LANDMARK_NEAR_CITY 160	// in the city: 5/8 of the way to it,
#define LANDMARK_SIZE_CITY 448	// and larger still: 2.8 times as big to the eye
#define LANDMARK_EASE 2			// a frame, while out of sight
static short landmark_near[LANDMARK_COUNT], landmark_size[LANDMARK_COUNT];
static bool landmark_scaled = false;

static int ease_to(int now, int to)
{
	return now < to - LANDMARK_EASE ? now + LANDMARK_EASE : now > to + LANDMARK_EASE ? now - LANDMARK_EASE : to;
}

/*
 * A landmark beyond the city's ring is behind the city: below the ring's foot, nothing of it can
 * be seen. But the ring's only a band of strips (its foot 400 under your eye, skyline_add), and
 * under it there's nothing to hide what's further. Going up the lift the foot rises with you, and
 * the sound chip's capacitors and their board showed under the skyline, stood on the black (the
 * user, 2026-11-13: "The sound landmark goes in front of the background sky when going up
 * elevator"; lift_sound.steps).
 * So it's cut, as everything's cut to the screen: by one more plane, through your eye and the foot
 * of the ring's strip it's seen over. Everything under the strip's foot, as you see it, goes; the
 * neighbouring strips' feet are the same line within a few pixels. Which strip: the one the line
 * from you to the landmark's foot crosses, in front of you (cross products' signs, multiplies
 * only: under 2^15 each side, inside 32 bits). Not if the landmark's on your side of that strip:
 * it's nearer than the ring, stood in the city.
 * Returns the plane's bit, to cut with (level_view's count must be put back after: _end), or 0.
 * The plane goes in the portal's first slot: out in the city the doorway's planes take the rest,
 * and the sky doesn't use them; that slot's kept and put back
 */
static frustum_plane ring_cut_kept;
static int ring_cut_count;

static unsigned landmark_ring_cut(int fx, int fy)
{
	const frustum_point *e = &level_view.eye;
	const int vx = fx - e->x, vy = fy - e->y;

	if (!sky_ring.on)
		return 0;
	for (int k = 0; k < SKYLINE_STRIPS; ++k)
	{
		const int next = (k + 1) & (SKYLINE_STRIPS - 1);
		const int x0 = sky_ring.cx + ((skyline_round[k][0] * sky_ring.r) >> 10), y0 = sky_ring.cy + ((skyline_round[k][1] * sky_ring.r) >> 10);
		const int x1 = sky_ring.cx + ((skyline_round[next][0] * sky_ring.r) >> 10), y1 = sky_ring.cy + ((skyline_round[next][1] * sky_ring.r) >> 10);
		// Which side of the line you-to-landmark each end of the strip is: the line passes between
		const int side0 = vx * (y0 - e->y) - vy * (x0 - e->x), side1 = vx * (y1 - e->y) - vy * (x1 - e->x);
		int you, it;

		if ((side0 < 0) == (side1 < 0))
			continue;
		if (vx * (((x0 + x1) >> 1) - e->x) + vy * (((y0 + y1) >> 1) - e->y) <= 0)
			continue; // the strip behind you, on the same line
		// Which side of the strip you are, and the landmark: the same side, it's nearer
		you = (x1 - x0) * (e->y - y0) - (y1 - y0) * (e->x - x0);
		it = (x1 - x0) * (fy - y0) - (y1 - y0) * (fx - x0);
		if ((you < 0) == (it < 0))
			return 0;
		{
			const frustum_point a = {x0, y0, sky_ring.foot}, b = {x1, y1, sky_ring.foot};
			const frustum_point above = {(x0 + x1) >> 1, (y0 + y1) >> 1, sky_ring.foot + 1000}; // the side kept
			unsigned bit;

			ring_cut_count = level_view.count;
			ring_cut_kept = level_view.planes[FRUSTUM_PORTAL_FIRST];
			level_view.count = FRUSTUM_PORTAL_FIRST;
			bit = frustum_add_edge(&level_view, a, b, above);
			if (bit == 0)
			{
				level_view.count = ring_cut_count;
				level_view.planes[FRUSTUM_PORTAL_FIRST] = ring_cut_kept;
			}
			return bit;
		}
	}
	return 0;
}

static void landmark_ring_cut_end(unsigned bit)
{
	if (bit == 0)
		return;
	level_view.planes[FRUSTUM_PORTAL_FIRST] = ring_cut_kept;
	level_view.count = ring_cut_count;
}

static void landmarks_add(level_mesh *m, int side, unsigned int frame, int place)
{
	const frustum_point *e = &level_view.eye;
	const bool city = place != PLACE_ARENA;

	if (!landmark_scaled)
	{
		for (int i = 0; i < LANDMARK_COUNT; ++i)
		{
			landmark_near[i] = LANDMARK_NEAR_ARENA;
			landmark_size[i] = LANDMARK_SIZE_ARENA;
		}
		landmark_scaled = true;
	}
	for (int i = 0; i < LANDMARK_COUNT; ++i)
	{
		const landmark *l = &landmarks[i];
		landmark_shape shapes[LANDMARK_SHAPES_MAX];
		frustum_point box[4];
		// Its foot, and the way along its plane (a unit step on x or y: corridor_point_behind is
		// a straight map), as it stands; then its foot drawn nearer, by the share
		const world_point at = corridor_point_behind(side, 0, l->out, l->across);
		const world_point one = corridor_point_behind(side, 0, l->out + (l->faces_out ? 0 : 1), l->across + (l->faces_out ? 1 : 0));
		const int ux = one.x - at.x, uy = one.y - at.y;
		const int near = landmark_near[i], size = landmark_size[i];
		const int fx = e->x + (((at.x - e->x) * near) >> 8), fy = e->y + (((at.y - e->y) * near) >> 8);
		int count;

		if (l->place == place)
			continue; // you're there: it's all round you, not on the horizon
		// Its plane's four corners (its whole width, the ground to its height)
		for (int k = 0; k < 4; ++k)
		{
			const int along = (((k == 0 || k == 3) ? -l->half_width : l->half_width) * size) >> 8;

			box[k].x = fx + along * ux;
			box[k].y = fy + along * uy;
			box[k].z = k < 2 ? 0 : (l->height * size) >> 8;
		}
		// Out of view: none of it's looked at, and it may grow or shrink unseen. Behind the
		// corridor's walls: not drawn either (it doesn't change then: it may be seen any moment)
		if (frustum_classify(&level_view, box, 4, FRUSTUM_SCREEN) == FRUSTUM_OUTSIDE)
		{
			landmark_near[i] = (short)ease_to(near, city ? LANDMARK_NEAR_CITY : LANDMARK_NEAR_ARENA);
			landmark_size[i] = (short)ease_to(size, city ? LANDMARK_SIZE_CITY : LANDMARK_SIZE_ARENA);
			continue;
		}
		if (sky_hidden(box, 4))
			continue;
		count = landmark_shapes(i, frame, shapes);
		{
			const unsigned cut = landmark_ring_cut(fx, fy); // behind the city's ring: its foot hidden

			for (int s = 0; s < count; ++s)
			{
				frustum_point c[4];

				for (int k = 0; k < 4; ++k)
				{
					const int along = (shapes[s].along[k] * size) >> 8;

					c[k].x = fx + along * ux;
					c[k].y = fy + along * uy;
					c[k].z = (shapes[s].up[k] * size) >> 8;
				}
				sky_face_cut(m, c, (jo_color)shapes[s].colour, cut);
			}
			landmark_ring_cut_end(cut);
		}
	}
}

/*
 * The A-bus: from Backup RAM's hub (the plan's 0B30, its way in) out over the city to Backup RAM
 * itself, the cartridge on the horizon (landmarks.c), so the far landmark's tied to the place you
 * reach it from (the user, 2026-11-14: "the landmark is far away"). On the Saturn the cartridge
 * slot is on the SCU's A-bus, so that's the line here: a long dark strip, rising gently from the
 * hub's roof to the cartridge's foot, data going out along it: a short lit packet sliding along
 * it, out to the cartridge, and again. The cartridge is drawn pulled in towards you (landmarks_add:
 * its near share, and its size), so the bus runs to where it's drawn, not where it stands.
 * Two quads: the strip, the packet over it (chained after it: JO_3D_SORT_PREVIOUS, so it's never
 * under it), each cut to the view as the landmarks are. Eight segments, the packet one of them,
 * cost the approach in drone.steps 52-54 fps where it had been 57-60. Out in the city only.
 * The strip's width is across its line: the line's length is found once a frame (a square root,
 * frustum_isqrt, and a division: per object, not per point)
 *
 * Not joined to the city, for now (the user, 2026-11-15: "looks like it's accessible, when it needs
 * to be blocked"): it starts ABUS_GAP out from the hub's middle, its roof's edge 250 out and then a
 * gap far wider than a jump clears (~225, walker.h), so it reads as out of reach. When Backup RAM
 * opens, set ABUS_GAP to 0 and it runs from the hub's roof again
 */
#define ABUS_GAP 900 // from the hub's middle to where the bus starts: its roof's half (250), then 650 of air
#define ABUS_HALF 120						 // half its width (5.5 m wide)
#define ABUS_DARK JO_COLOR_RGB(62, 74, 104)	 // the cartridge's slate, lighter: it reads over the dark city
#define ABUS_LIT JO_COLOR_RGB(120, 240, 170) // the data on it: the cartridge's label's green
#define ABUS_PACKET_SHIFT 4					 // the packet's a sixteenth of the bus long
#define ABUS_RUN_SHIFT 8					 // and runs its length in 256 frames (~4 s)

static void abus_add(level_mesh *m, int side, int wall, int middle, unsigned int frame)
{
	const city_tower *hub = city_tower_at(3, 11); // Backup RAM's
	const world_point from = corridor_point_behind(side, wall, (hub->out_min + hub->out_max) >> 1,
												   middle + ((hub->across_min + hub->across_max) >> 1));
	const frustum_point *e = &level_view.eye;
	int which = -1;

	for (int i = 0; i < LANDMARK_COUNT; ++i)
		if (landmarks[i].place == PLACE_BACKUP_RAM)
			which = i;
	if (which < 0)
		return;
	{
		// The cartridge's foot as it's drawn now (landmarks_add's own sums)
		const landmark *l = &landmarks[which];
		const world_point at = corridor_point_behind(side, 0, l->out, l->across);
		const int near = landmark_near[which];
		const int to_x = e->x + (((at.x - e->x) * near) >> 8), to_y = e->y + (((at.y - e->y) * near) >> 8);
		const int dx = to_x - from.x, dy = to_y - from.y;
		const int length = frustum_isqrt((unsigned)(dx * dx + dy * dy)); // under 32 000 each way: fits
		// Where the bus starts, x 256 of the way: ABUS_GAP out (a division, once a frame)
		const int start = length > ABUS_GAP ? (ABUS_GAP << 8) / length : 256;
		// Where the packet is: from the bus's start to its own length short of the far end, over 256
		// frames, round again (a mask; never past the cartridge)
		const int run = start + (int)(((frame & ((1 << ABUS_RUN_SHIFT) - 1)) * (240 - start)) >> 8);
		int across_x, across_y;

		if (length == 0 || start >= 240)
			return;
		across_x = (-dy * ABUS_HALF) / length; // square to the line, ABUS_HALF long
		across_y = (dx * ABUS_HALF) / length;
		for (int part = 0; part < 2; ++part)
		{
			// The strip from end to end; the packet from run / 256 of the way to a sixteenth on
			const int t0 = part == 0 ? start : run, t1 = part == 0 ? 256 : run + (256 >> ABUS_PACKET_SHIFT);
			const int ax = from.x + ((dx * t0) >> 8), ay = from.y + ((dy * t0) >> 8);
			const int bx = from.x + ((dx * t1) >> 8), by = from.y + ((dy * t1) >> 8);
			const int az = hub->height + (((-hub->height) * t0) >> 8), bz = hub->height + (((-hub->height) * t1) >> 8);
			const frustum_point c[4] = {{ax - across_x, ay - across_y, az + 1}, {bx - across_x, by - across_y, bz + 1},
										{bx + across_x, by + across_y, bz + 1}, {ax + across_x, ay + across_y, az + 1}};

			if (part == 0)
				sky_face(m, c, ABUS_DARK);
			else
			{
				const int before = m->quad_count;

				sky_face(m, c, ABUS_LIT);
				for (int q = before; q < m->quad_count; ++q)
					m->attributes[q].sort = (unsigned char)((m->attributes[q].sort & ~3) | JO_3D_SORT_PREVIOUS);
			}
		}
	}
}

/*
 * A box of the drive's works, lined up with the world's axes, between two opposite corners on the
 * floor plan and two heights: the sides turned to the eye, and its underside if the eye's below it
 * (they hang in the sky: nobody's above one yet). Not from inside its plan with the eye between
 * its heights (the lift's beam, when it has you)
 */
static void sky_box(level_mesh *m, world_point a, world_point b, int bottom, int top, jo_color side_x, jo_color side_y, jo_color under)
{
	const int x_min = JO_MIN(a.x, b.x), x_max = JO_MAX(a.x, b.x), y_min = JO_MIN(a.y, b.y), y_max = JO_MAX(a.y, b.y);
	const frustum_point *e = &level_view.eye;
	frustum_point c[4];

	if (e->x < x_min || e->x > x_max)
	{
		const int line = e->x < x_min ? x_min : x_max;

		level_side_corners(false, line, y_min, y_max, bottom, top, c);
		sky_face(m, c, side_x);
	}
	if (e->y < y_min || e->y > y_max)
	{
		const int line = e->y < y_min ? y_min : y_max;

		level_side_corners(true, line, x_min, x_max, bottom, top, c);
		sky_face(m, c, side_y);
	}
	if (e->z < bottom)
	{
		const frustum_point face[4] = {{x_min, y_min, bottom}, {x_max, y_min, bottom}, {x_max, y_max, bottom}, {x_min, y_max, bottom}};

		sky_face(m, face, under);
	}
}

/*
 * The CD block: the disc, and under it the drive (city.h has the measures).
 *
 * The disc is an octagon ring of 8 flat sectors in two dull slates, turn about, lying flat. It
 * doesn't turn: the sector passing the laser does, one a little paler stepping round it (only a
 * colour changes: no point moves, no sine). Dull, as the landmarks are: it's most of the city's
 * sky, and nothing to look at while you're riding or running.
 * Under its hole, the spindle. From the spindle out past the hub, the rail, and on the rail over
 * the hub the pickup: a box, its underside the hatch the lift comes up to, and from its top a thin
 * red beam up to the disc: the laser, the one bright thing here. From the hub's roof up to the
 * hatch, the lift's beam of light.
 * 8 sectors, and 2 or 3 faces each for the spindle, rail, pickup, laser and beam: under 22
 */
#define DISC_SLATE JO_COLOR_RGB(58, 66, 96)
#define DISC_SLATE_ALT JO_COLOR_RGB(46, 52, 80)
#define DISC_READING JO_COLOR_RGB(96, 108, 146)
#define DISC_READ_SHIFT 3 // the sector at the laser moves on every 8 frames: round in about a second
#define DRIVE_SIDE_X JO_COLOR_RGB(40, 44, 66)
#define DRIVE_SIDE_Y JO_COLOR_RGB(30, 34, 54)
#define DRIVE_UNDER JO_COLOR_RGB(22, 24, 40)
#define DRIVE_HATCH JO_COLOR_RGB(120, 150, 170) // the pickup's underside: where the lift arrives
#define DRIVE_LASER JO_COLOR_RGB(190, 40, 50)
#define DRIVE_BEAM JO_COLOR_RGB(110, 170, 190) // the lift's
#define DRIVE_RAIL_HALF 70
#define DRIVE_LASER_HALF 30
#define DRIVE_BEAM_HALF 12
#define DRIVE_SPINDLE_HALF 250
#define DRIVE_DETAIL 3000 // the spindle and rail only from nearer than this to the hub

static void disc_add(level_mesh *m, int side, int wall, int middle, unsigned int frame, int place, bool in_lift)
{
	// An octagon's corners: cos and sin of 0, 45, 90 ... degrees, x1024
	static const short round[8][2] = {{1024, 0}, {724, 724}, {0, 1024}, {-724, 724}, {-1024, 0}, {-724, -724}, {0, -1024}, {724, -724}};
	const int reading = (int)((frame >> DISC_READ_SHIFT) & 7);
	const world_point centre = city_hub_point(side, wall, middle, 0, CITY_DISC_OFFSET);
	// The beam's foot: the hub's roof, 1400 under the arena's floor. From the arena that part is
	// underground: drawn, it showed as a line down across the floor in front of the wall (the
	// floor's VDP2, behind every polygon: film 2026-11-12). There it starts at the floor's height
	const int hub_roof = city_tower_at(CITY_HUB_COLUMN, CITY_HUB_ROW)->height;
	/*
	 * And from inside the corridor (on foot, not out in the city), from its walls' top: the walls
	 * hide everything under that, and the far end of the walls is near as far as the beam, so VDP1
	 * (sorting by middles, no depth) can draw a low part of it over them (the user, 2026-11-14: "The
	 * elevator beam from CD ROM drive is visible from the yard after crashing"; not caught on film)
	 */
	const int roof = place == PLACE_ARENA && hub_roof < 0 ? 0
					 : place == PLACE_WORK_RAM && !portal.outside ? CORRIDOR_TOP_ON_FOOT
																 : hub_roof;

	for (int k = 0; k < 8; ++k)
	{
		const int next = (k + 1) & 7;
		// A sector: from the hole's edge out to the rim, between two corners of the octagon
		const frustum_point c[4] = {
			{centre.x + ((round[k][0] * CITY_DISC_HOLE) >> 10), centre.y + ((round[k][1] * CITY_DISC_HOLE) >> 10), CITY_DISC_HEIGHT},
			{centre.x + ((round[k][0] * CITY_DISC_RADIUS) >> 10), centre.y + ((round[k][1] * CITY_DISC_RADIUS) >> 10), CITY_DISC_HEIGHT},
			{centre.x + ((round[next][0] * CITY_DISC_RADIUS) >> 10), centre.y + ((round[next][1] * CITY_DISC_RADIUS) >> 10), CITY_DISC_HEIGHT},
			{centre.x + ((round[next][0] * CITY_DISC_HOLE) >> 10), centre.y + ((round[next][1] * CITY_DISC_HOLE) >> 10), CITY_DISC_HEIGHT}};

		sky_face(m, c, k == reading ? DISC_READING : (k & 1) ? DISC_SLATE_ALT : DISC_SLATE);
	}
	/*
	 * The spindle and the rail only near the hub (within DRIVE_DETAIL along the floor): from
	 * further they're a few pixels under the disc, and each is 2 or 3 polygons of jo's work; the
	 * corridor, which had no headroom, ran at 48-51 fps with the sky (film 2026-11-12). The pickup
	 * and its red laser always: they're what says this is a drive
	 */
	{
		const world_point hub = city_hub_middle(side, wall, middle);
		const int dx = level_view.eye.x - hub.x, dy = level_view.eye.y - hub.y;

		if ((dx < 0 ? -dx : dx) < DRIVE_DETAIL && (dy < 0 ? -dy : dy) < DRIVE_DETAIL)
		{
			// The spindle, under the hole
			sky_box(m, city_hub_point(side, wall, middle, -DRIVE_SPINDLE_HALF, CITY_DISC_OFFSET - DRIVE_SPINDLE_HALF),
					city_hub_point(side, wall, middle, DRIVE_SPINDLE_HALF, CITY_DISC_OFFSET + DRIVE_SPINDLE_HALF), CITY_PICKUP_BOTTOM,
					CITY_DISC_HEIGHT, DRIVE_SIDE_X, DRIVE_SIDE_Y, DRIVE_UNDER);
			// The rail: from the spindle out past the pickup, through its middle
			sky_box(m, city_hub_point(side, wall, middle, -DRIVE_RAIL_HALF, -1700),
					city_hub_point(side, wall, middle, DRIVE_RAIL_HALF, CITY_DISC_OFFSET), CITY_PICKUP_BOTTOM + 120,
					CITY_PICKUP_BOTTOM + 230, DRIVE_SIDE_X, DRIVE_SIDE_Y, DRIVE_UNDER);
		}
	}
	// The pickup, over the hub: its underside's the hatch
	sky_box(m, city_hub_point(side, wall, middle, -CITY_PICKUP_HALF, -CITY_PICKUP_HALF),
			city_hub_point(side, wall, middle, CITY_PICKUP_HALF, CITY_PICKUP_HALF), CITY_PICKUP_BOTTOM, CITY_PICKUP_TOP, DRIVE_SIDE_X,
			DRIVE_SIDE_Y, DRIVE_HATCH);
	// The laser: from the pickup's top up to the disc
	sky_box(m, city_hub_point(side, wall, middle, -DRIVE_LASER_HALF, -DRIVE_LASER_HALF),
			city_hub_point(side, wall, middle, DRIVE_LASER_HALF, DRIVE_LASER_HALF), CITY_PICKUP_TOP, CITY_DISC_HEIGHT, DRIVE_LASER,
			DRIVE_LASER, DRIVE_LASER);
	// The lift's beam, from the hub's roof up to the hatch: not while you're in it
	if (!in_lift)
	{
		const world_point a = city_hub_point(side, wall, middle, -DRIVE_BEAM_HALF, -DRIVE_BEAM_HALF);
		const world_point b = city_hub_point(side, wall, middle, DRIVE_BEAM_HALF, DRIVE_BEAM_HALF);
		const frustum_point *e = &level_view.eye;

		// (nor from right beside it, inside the pad: its sides would be at your nose)
		if (e->x < JO_MIN(a.x, b.x) - 100 || e->x > JO_MAX(a.x, b.x) + 100 || e->y < JO_MIN(a.y, b.y) - 100 ||
			e->y > JO_MAX(a.y, b.y) + 100)
			sky_box(m, a, b, roof, CITY_PICKUP_BOTTOM, DRIVE_BEAM, DRIVE_BEAM, DRIVE_BEAM);
	}
}

/*
 * The end of the reflection (declared above, with the mirror): a near-black wall across the
 * corridor at MIRROR_DEPTH, and what the glass shows over it.
 * The wall stood only as high as the corridor's walls, and from near the glass its top is well
 * under the glass's: in the gap, the real landmarks behind the arena showed through the mirror
 * (the user, 2026-11-13: "The landmarks are incorrect in the mirror section: it should show the CD
 * ROM"). So it's MIRROR_SKY_TOP high now (cut to the glass like the rest: it fills the pane), and
 * on it the one thing in the sky behind you: the CD block's disc over the city, its eight sectors
 * seen by the reflected eye. They're far beyond the wall, and VDP1 would draw them first and the
 * wall over them: so each is chained to the wall (JO_3D_SORT_PREVIOUS: drawn straight after it,
 * in the order added). Not cut by the reflection's depth plane (the near plane's stand-in, at the
 * wall): only by the glass's four edges.
 * And it's far wider than the corridor, and taller: the reflected corridor has openings at the
 * side, and through them, where the wall didn't reach, the real world behind the glass showed: the
 * pink of the sky's horizon and the video output's landmark (the user, 2026-11-13: "Fix Landmarks
 * in mirror to be only of the CD Rom, the video output landmark is shown"; mirror_sky.steps,
 * ms_01). Cut to the glass, only what's seen through it is drawn: one polygon however wide it is.
 * Near the glass the glass's view of the wall spreads 2300 / (your distance) times its size, so
 * it's MIRROR_SKY_WIDE either side: from 50 or more from the glass it fills the pane. Kept inside a
 * short (world_point) wherever the corridor is
 */
#define MIRROR_SKY_TOP 12000
#define MIRROR_SKY_WIDE 6000

static void mirror_far_wall(const corridor *passage)
{
	static const short round[8][2] = {{1024, 0}, {724, 724}, {0, 1024}, {-724, 724}, {-1024, 0}, {-724, -724}, {0, -1024}, {724, -724}};
	const world_point a = corridor_point(passage, mirror_depth, passage->across_min - MIRROR_SKY_WIDE);
	const world_point b = corridor_point(passage, mirror_depth, passage->across_max + MIRROR_SKY_WIDE);
	const int middle = (passage->across_min + passage->across_max) >> 1;
	const world_point centre = city_hub_point(passage->side, passage->wall, middle, 0, CITY_DISC_OFFSET);
	const unsigned edges = FRUSTUM_SCREEN & ~(1u << FRUSTUM_NEAR);
	frustum_point corners[4];
	level_mesh m;

	level_mesh_begin(&m, far_points, far_polygons, far_attributes, FAR_POINTS, FAR_QUADS);
	corners[0] = grey_point(a, MIRROR_SKY_TOP);
	corners[1] = grey_point(b, MIRROR_SKY_TOP);
	corners[2] = grey_point(b, WALL_BOTTOM_Z);
	corners[3] = grey_point(a, WALL_BOTTOM_Z);
	if (!level_face_add(&m, corners, 4, FRUSTUM_SCREEN | FRUSTUM_GUARD, JO_COLOR_RGB(16, 16, 32), LEVEL_FOG_NONE,
						JO_3D_SORT_MAX, false))
		return;
	for (int k = 0; k < 8; ++k)
	{
		const int next = (k + 1) & 7;
		const frustum_point c[4] = {
			{centre.x + ((round[k][0] * CITY_DISC_HOLE) >> 10), centre.y + ((round[k][1] * CITY_DISC_HOLE) >> 10), CITY_DISC_HEIGHT},
			{centre.x + ((round[k][0] * CITY_DISC_RADIUS) >> 10), centre.y + ((round[k][1] * CITY_DISC_RADIUS) >> 10), CITY_DISC_HEIGHT},
			{centre.x + ((round[next][0] * CITY_DISC_RADIUS) >> 10), centre.y + ((round[next][1] * CITY_DISC_RADIUS) >> 10), CITY_DISC_HEIGHT},
			{centre.x + ((round[next][0] * CITY_DISC_HOLE) >> 10), centre.y + ((round[next][1] * CITY_DISC_HOLE) >> 10), CITY_DISC_HEIGHT}};
		const unsigned crossed = frustum_classify(&level_view, c, 4, edges);

		if (crossed != FRUSTUM_OUTSIDE)
			level_face_add(&m, c, 4, crossed, (k & 1) ? DISC_SLATE_ALT : DISC_SLATE, LEVEL_FOG_NONE, JO_3D_SORT_PREVIOUS, false);
	}
	far_mesh.data.points = far_points;
	far_mesh.data.polygons = far_polygons;
	far_mesh.data.attributes = far_attributes;
	far_mesh.data.point_count = (unsigned short)m.point_count;
	far_mesh.data.polygon_count = (unsigned short)m.quad_count;
	jo_3d_mesh_draw(&far_mesh);
}

void level_sky_draw(int side, int wall, int middle, unsigned int frame, int place, bool in_lift, bool extras)
{
	level_mesh m;

	/*
	 * On the flight down into the cycle there's no view (level_view_on: nothing else is cut to it
	 * then), and the sky's furniture wasn't drawn until you'd landed: the skyline and the
	 * landmarks popped in as play began (the user, 2026-11-13). The flight's own view is borrowed
	 * for this one pass: level_view's free then (everything else that reads it asks level_view_on
	 * first), and the corridor's walls, which hide the sky from inside it, aren't up
	 */
	if (!level_view_on && !view_arrival_frustum(&level_view))
		return;
	level_mesh_begin(&m, sky_points, sky_polygons, sky_attributes, SKY_POINTS, SKY_QUADS);
	sky_ring.on = false; // (skyline_add sets it)
	skyline_add(&m, place, side, wall, middle); // always: without it the sky's pink horizon shows
	// The landmarks and the A-bus: what the draw budget may leave out (game/draw_budget.h). Not the
	// skyline above, nor the CD block's disc below: those are the background itself, and leaving
	// the skyline out showed VDP2's pink horizon where the city should be (the user, 2026-11-15)
	if (extras)
	{
		landmarks_add(&m, side, frame, place);
		if (place == PLACE_WORK_RAM && portal.outside)
			abus_add(&m, side, wall, middle, frame); // out to Backup RAM
	}
	if (place != PLACE_CD_BLOCK)
		disc_add(&m, side, wall, middle, frame, place, in_lift);
	if (m.quad_count == 0)
		return;
	sky_mesh.data.points = sky_points;
	sky_mesh.data.polygons = sky_polygons;
	sky_mesh.data.attributes = sky_attributes;
	sky_mesh.data.point_count = (unsigned short)m.point_count;
	sky_mesh.data.polygon_count = (unsigned short)m.quad_count;
	/*
	 * Drawn by this CPU (it made no difference against handing it to the slave, measured in
	 * corridor_still.steps, 2026-11-12), so the debug line's poly counts it
	 */
	jo_3d_mesh_draw(&sky_mesh);
}

/*
 * The watchdog drone's scan (drone_draw.c): a sheet of light, level, and its edges.
 *
 * The sheet's a fan, `corners` points: its apex at the sensor, then its rays' ends across it
 * (game/drone.h: each stops at the first tower in its way, so the fan's outline follows the tower
 * faces it meets, and isn't convex). Drawn as a triangle between each two rays, each cut to the
 * view as every face on foot is (level_face_add), with VDP1's mesh: only the pixels where x + y is
 * even, a screen door you see through. That's the Saturn's own see-through, and the one that works
 * here: VDP1's half-transparency blends only over pixels VDP1 has drawn (their MSB set: SKILLS.md),
 * and behind a drone in the air there's mostly VDP2, the floor and the sky, where it would come out
 * solid. And it reads the frame buffer, 6 times the cost a pixel; mesh writes half the pixels.
 * Then its outline, solid: the two outer rays from the sensor and the line along the rays' ends,
 * each a VDP1 line (one pixel wide however near or far), cut to the view by each of its planes in
 * turn. That's where a cut line's new end is found: a division, but per line (~11 of them), once a
 * frame, not per pixel
 */
#define SCAN_POINTS 128
#define SCAN_QUADS 64
static jo_pos3D_fixed scan_points[SCAN_POINTS];
static jo_3d_polygon scan_polygons[SCAN_QUADS];
static jo_3d_polygon_attribute scan_attributes[SCAN_QUADS];
static jo_3d_mesh scan_mesh;

// A line from a to b, cut to the view's planes in `planes`: false if none of it's in view
static bool scan_line_cut(frustum_point *a, frustum_point *b, unsigned planes)
{
	for (int i = 0; i < FRUSTUM_PLANES_MAX; ++i)
	{
		int da, db;

		if (!(planes & (1u << i)))
			continue;
		da = frustum_distance(&level_view, i, *a);
		db = frustum_distance(&level_view, i, *b);
		if (da < 0 && db < 0)
			return false;
		if (da < 0 || db < 0)
		{
			// The point where the line crosses the plane: a share da / (da - db) of the way from
			// a to b (the one division, see above). The distances are x1024 (FRUSTUM_ONE): a
			// line 1500 long is ~1.5 million, and times a difference of 3000 that's past 32 bits.
			// So both are taken down 8 bits first: the share's the same, to a few units in 1500
			frustum_point *out = da < 0 ? a : b;
			const frustum_point from = *a, to = *b;
			const int d = (da - db) >> 8;

			da >>= 8;
			if (d == 0)
				continue; // the line lies along the plane: nothing to cut
			out->x = from.x + (to.x - from.x) * da / d;
			out->y = from.y + (to.y - from.y) * da / d;
			out->z = from.z + (to.z - from.z) * da / d;
		}
	}
	return true;
}

static void scan_line_add(level_mesh *m, frustum_point a, frustum_point b, jo_color colour)
{
	jo_3d_polygon *q;
	jo_3d_polygon_attribute *at;

	if (m->point_count + 2 > m->points_max || m->quad_count + 1 > m->quads_max || !scan_line_cut(&a, &b, FRUSTUM_SCREEN))
		return;
	m->points[m->point_count].x = jo_int2fixed(a.x);
	m->points[m->point_count].y = jo_int2fixed(a.y);
	m->points[m->point_count].z = jo_int2fixed(a.z);
	m->points[m->point_count + 1].x = jo_int2fixed(b.x);
	m->points[m->point_count + 1].y = jo_int2fixed(b.y);
	m->points[m->point_count + 1].z = jo_int2fixed(b.z);
	q = &m->polygons[m->quad_count];
	at = &m->attributes[m->quad_count];
	q->normal.x = JO_FIXED_0;
	q->normal.y = JO_FIXED_0;
	q->normal.z = JO_FIXED_1;
	q->vertices[0] = q->vertices[3] = (unsigned short)m->point_count;
	q->vertices[1] = q->vertices[2] = (unsigned short)(m->point_count + 1);
	set_corridor_wall_attribute(at, colour);
	// A line, not a polygon: VDP1's own command (its first two corners)
	at->sort = JO_3D_SORT_CENTER | ((JO_VDP1_LINE >> 16) & 0x1c) | JO_3D_NO_OPTION;
	at->draw_mode = (JO_VDP1_COLOR_MODE_RGB | JO_VDP1_NO_GOURAUD) | ((JO_VDP1_LINE >> 24) & 0xc0);
	at->direction = JO_VDP1_LINE & 0x3f;
	m->point_count += 2;
	m->quad_count++;
}

void level_scan_draw(const frustum_point *fan, int corners, const frustum_point *rail, jo_color sheet, jo_color edge)
{
	level_mesh m;

	if (!level_view_on)
		return;
	level_mesh_begin(&m, scan_points, scan_polygons, scan_attributes, SCAN_POINTS, SCAN_QUADS);
	for (int k = 1; k + 1 < corners; ++k)
	{
		const frustum_point triangle[3] = {fan[0], fan[k], fan[k + 1]};
		const unsigned crossed = frustum_classify(&level_view, triangle, 3, FRUSTUM_SCREEN);
		const int first = m.quad_count;

		/*
		 * Not a triangle that reaches behind the eye (one the near plane would cut): that's you in
		 * its fan, close, and it filled the screen as the sheet passed your eye (a ceiling or a
		 * floor of light), 42 fps; left out, the approach in drone.steps went from 46-52 fps to
		 * 54-59 (2026-11-14). The rest of the fan still shows, and the sensor and the red
		 */
		if (crossed != FRUSTUM_OUTSIDE && !(crossed & (1u << FRUSTUM_NEAR)) &&
			level_face_add(&m, triangle, 3, crossed, sheet, LEVEL_FOG_NONE, JO_3D_SORT_CENTER, false))
			for (int i = first; i < m.quad_count; ++i)
				scan_attributes[i].draw_mode |= JO_VDP1_MESH; // the screen door
	}
	for (int k = 1; k + 1 < corners; ++k)
		scan_line_add(&m, fan[k], fan[k + 1], edge); // along the rays' ends
	scan_line_add(&m, fan[0], fan[1], edge);		   // the two outer rays
	scan_line_add(&m, fan[0], fan[corners - 1], edge);
	if (rail)
		scan_line_add(&m, rail[0], rail[1], edge); // an upright line it rides
	if (m.quad_count == 0)
		return;
	scan_mesh.data.points = scan_points;
	scan_mesh.data.polygons = scan_polygons;
	scan_mesh.data.attributes = scan_attributes;
	scan_mesh.data.point_count = (unsigned short)m.point_count;
	scan_mesh.data.polygon_count = (unsigned short)m.quad_count;
	// Drawn by the master: handed to the slave it was worse out in the city (drone_draw.c)
	jo_3d_mesh_draw(&scan_mesh);
}
