#include "level.h"
#include "utils/angles.h"
#include "scene_memory.h"
#include "game/arena.h"
#include "game/surround.h"
#include "game/city.h"
#include "game/occluder.h"
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

void load_arena_textures(void)
{
	// 24-bit, 33 KB of VDP1 RAM between them (they replace the four debug BOX textures)
	low_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "WALL_LO.TGA", JO_COLOR_Transparent);
	high_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "WALL_HI.TGA", JO_COLOR_Transparent);
	base_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "WALLBASE.TGA", JO_COLOR_Transparent);
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
} grey_box;
/*
 * The boxes in groups that stand together, each with a box round them all: tested first, as one
 * (CULLING.md: a coarse cull before the fine one). Wholly out of view, none of a group's boxes is
 * looked at; wholly in view, none of them is tested. The stairs (the corridor's first twenty boxes:
 * corridor.c lists them first), the corridor's other five, and the city's rows of three (city.c
 * lists them a row at a time). Tested one by one, every box every frame was a sixth of the frame
 */
#define GREY_STAIRS 20
#define GREY_CITY_ROW 3
#define GREY_GROUPS (2 + CITY_TOWERS / GREY_CITY_ROW)
typedef struct
{
	unsigned char first, last; // its boxes, first to last
	frustum_point middle, half; // the box round them all: its middle, and half its size each way
} grey_group;
static world_rect grey_corridor_floor; // the corridor's floor, for grey_side_bottom
static int grey_boxes_for = -1; // the corridor they're for: its wall and side (-1: none yet)

/*
 * The corridor's meshes and the grey box's, in Work RAM's memory (scene_memory.h): taken as the
 * breach starts, given back when the arena starts again. NULL meanwhile: nothing's drawn
 */
// The boxes' faces, sides and tops, each cut to the view: most a quad, some up to 7 (draw_corridor_boxes)
#define GREY_POINTS_MAX 640
#define GREY_QUADS_MAX 288
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
	for (int i = 0; i < GREY_BOXES; ++i)
	{
		world_point a, b;
		world_rect *r = &grey_boxes[i].rect;

		if (i < CORRIDOR_BOXES)
		{
			corridor_box_corners(passage, &corridor_boxes[i], &a, &b);
			grey_boxes[i].height = corridor_boxes[i].height;
			grey_boxes[i].floor = corridor_boxes[i].floor; // a bar's above the floor
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

		group->first = (unsigned char)(g == 0 ? 0 : g == 1 ? GREY_STAIRS : CORRIDOR_BOXES + (g - 2) * GREY_CITY_ROW);
		group->last = (unsigned char)(g == 0 ? GREY_STAIRS - 1 : g == 1 ? CORRIDOR_BOXES - 1 : group->first + GREY_CITY_ROW - 1);
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
	portal.outside = out > CORRIDOR_LENGTH + GREY_DOORWAY;
	portal.inside = out < CORRIDOR_LENGTH - GREY_DOORWAY;
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
}

void level_view_begin(const corridor *passage, bool free_camera)
{
	level_view_on = view_frustum(free_camera, &level_view);
	portal.inside = portal.outside = false;
	portal.planes = 0;
	if (passage == JO_NULL || !passage->open || corridor_meshes == JO_NULL)
		return;
	grey_boxes_work_out(passage);
	grey_portal_set(passage);
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

	if (!glass_loaded || corridor_out(passage, *eye) <= 0)
		return; // not loaded yet, or you're not in the corridor: nothing to look back at
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

static bool grey_stairs_profiled(int i, bool along_x, int line); // below

// One side of box i: from what hides its foot (bottom: grey_side_bottom) to its top. test: the
// planes to test it against (0: its box is wholly in view, so it is too)
static void grey_side_add(int i, int which, bool along_x, int line, int from, int to, unsigned test)
{
	const grey_box *b = &grey_boxes[i];
	frustum_point c[4];
	jo_color colour;

	if (b->bottom[which] >= b->height || grey_stairs_profiled(i, along_x, line))
		return; // hidden all the way up, or the stairs' profile stands in for it
	level_side_corners(along_x, line, from, to, b->bottom[which], b->height, c);
	if (i & 1)
		colour = along_x ? GREY_SIDE_Y_ALT : GREY_SIDE_X_ALT;
	else
		colour = along_x ? GREY_SIDE_Y : GREY_SIDE_X;
	level_face_add(&grey_out, c, 4, test != 0 ? level_planes_rect(&c[0], &c[2], test) : 0, colour, LEVEL_FOG_FLAT,
				   JO_3D_SORT_CENTER, level_view_on && level_side_lies(c));
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

static void grey_boxes_add(void)
{
	const frustum_point *e = &level_view.eye;

	for (int g = 0; g < GREY_GROUPS; ++g)
	{
		const grey_group *group = &grey_groups[g];
		const bool city = group->first >= CORRIDOR_BOXES;
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

			if (crossed != 0)
			{
				const frustum_point middle = {b->centre.x, b->centre.y, b->middle};
				const frustum_point half = {((r->x_max - r->x_min) >> 1) + 1, ((r->y_max - r->y_min) >> 1) + 1,
											((b->height - b->floor) >> 1) + 1};

				test = frustum_classify_box(&level_view, middle, half, crossed);
				if (test == FRUSTUM_OUTSIDE)
					continue; // none of it can be seen from here
			}
			if (e->x < r->x_min || !level_view_on)
				grey_side_add(i, GREY_X_MIN, false, r->x_min, r->y_min, r->y_max, test);
			if (e->x > r->x_max || !level_view_on)
				grey_side_add(i, GREY_X_MAX, false, r->x_max, r->y_min, r->y_max, test);
			if (e->y < r->y_min || !level_view_on)
				grey_side_add(i, GREY_Y_MIN, true, r->y_min, r->x_min, r->x_max, test);
			if (e->y > r->y_max || !level_view_on)
				grey_side_add(i, GREY_Y_MAX, true, r->y_max, r->x_min, r->x_max, test);
			if (-view_z > b->height)
			{
				const frustum_point top[4] = {{r->x_min, r->y_min, b->height},
											  {r->x_max, r->y_min, b->height},
											  {r->x_max, r->y_max, b->height},
											  {r->x_min, r->y_max, b->height}};

				level_face_add(&grey_out, top, 4, test != 0 ? level_planes_rect(&top[0], &top[2], test) : 0,
							   grey_top_colour(b->height), LEVEL_FOG_NONE, JO_3D_SORT_CENTER, false);
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
			int step = dir * (off < 1000 ? 250 : off < 2000 ? 500 : 1000);
			int b;

			if (off < 1000)
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
	slave_draw_mesh(&grey_mesh); // the slave draws it (slave_draw.h)
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

static void draw_glass_floor(const corridor *passage)
{
	// From the city, only the doorway's planes that can hide the floor (grey_portal)
	const unsigned planes = FRUSTUM_SCREEN | (portal.outside ? portal.floor_planes : 0);
	world_point eye_floor;
	int eye_out, near_from, near_to, tiles_from;
	// The column of tiles being worked through (they're laid a column at a time, along out), and
	// the planes it crosses: tested once as a box, its tiles only against those
	int column_out = -1;
	unsigned column_planes = 0;

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
	tiles_from = glass_out.point_count;
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
	slave_draw_mesh(&corridor_mesh); // the slave draws it (slave_draw.h)
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
