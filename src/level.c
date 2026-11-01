#include "level.h"
#include "game/arena.h"
#include "game/surround.h"
#include "object_fog_vdp1.h"
#include "view.h"

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
}

// Works out a strip's corners for pieces [0, pieces) of columns, from z_bottom to z_top, into
// points (column k: its bottom, then its top) and each column's fog level. With a camera, the
// strip is first cut to the run of pieces that are in view and min_forward in front of it.
// Returns how many pieces are left (0: nothing to draw)
static int fill_strip(POINT *points, unsigned char *fog_levels, const world_point *columns, int pieces, int z_bottom,
					  int z_top, const wall_clip_camera *camera, int min_forward)
{
	int first = 0;
	int count = pieces;
	wall_clip_run run;

	if (camera != JO_NULL)
	{
		run = wall_clip_strip(columns, pieces, camera, min_forward);
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
		// As init_arena_walls: left top, right top, right bottom, left bottom
		q->Vertices[0] = (Uint16)(base + j * 2 + 1);
		q->Vertices[1] = (Uint16)(base + j * 2 + 3);
		q->Vertices[2] = (Uint16)(base + j * 2 + 2);
		q->Vertices[3] = (Uint16)(base + j * 2);
		band->attributes[polygon] = *texture;
		if (object_fog_enabled)
			object_fog_set_quad(&band->attributes[polygon], fog_levels[j], fog_levels[j + 1], fog_levels[j + 1],
								fog_levels[j]);
	}
	if (count > 0)
		band->mesh.data.nbPoint = (Uint16)(base + (count + 1) * 2);
	band->mesh.data.nbPolygon = (Uint16)polygon;
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

void draw_arena_walls(const world_rect *view, const world_point *eye, const wall_clip_camera *camera, bool cutaway,
					  int rise, const wall_flicker *flicker)
{
	if (rise <= 0)
		return; // still flat in the floor
	// The blocks' bands take the flicker's pieces as well as the blocks' faces: filled by both,
	// then drawn once
	block_band_begin(&block_low);
	block_band_begin(&block_high);
	for (int s = 0; s < 4; ++s)
		// Cutaway: the south and west walls stand between the iso camera (which sits on the +x, +y
		// side of the cycle) and the cycle. Full height they'd hide it, and their tops would rise
		// behind the camera. Just the glowing base, lower than the cycle, still marks the edge
		draw_side(&sides[s], s, view, eye, camera, cutaway && sides[s].fixed > 0, rise, flicker);
	draw_blocks(view, eye, camera, cutaway, rise);
	block_band_draw(&block_low);
	block_band_draw(&block_high);
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
