#include "level.h"
#include "game/arena.h"

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
}

// Draws pieces [0, pieces) of columns as one strip from z_bottom to z_top. With a camera, the
// strip is first cut to the run of pieces that are in view and min_forward in front of it.
static void draw_strip(wall_strip *strip, const world_point *columns, int pieces, int z_bottom, int z_top,
					   const wall_clip_camera *camera, int min_forward)
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
		return;

	for (int i = 0; i <= count; ++i)
	{
		// The run's ends may have slid forward along the wall; the columns in between haven't
		world_point p = columns[first + i];
		if (camera != JO_NULL && i == 0)
			p = run.start;
		else if (camera != JO_NULL && i == count)
			p = run.end;

		strip->points[i * 2][X] = jo_int2fixed(p.x);
		strip->points[i * 2][Y] = jo_int2fixed(p.y);
		strip->points[i * 2][Z] = jo_int2fixed(z_bottom);
		strip->points[i * 2 + 1][X] = strip->points[i * 2][X];
		strip->points[i * 2 + 1][Y] = strip->points[i * 2][Y];
		strip->points[i * 2 + 1][Z] = jo_int2fixed(z_top);
	}
	strip->mesh.data.nbPoint = (count + 1) * 2;
	strip->mesh.data.nbPolygon = count;
	jo_3d_mesh_draw(&strip->mesh);
}

static void draw_side(wall_side *side, const world_rect *view, const world_point *eye,
					  const wall_clip_camera *camera, bool cutaway)
{
	const int half = arena_half();
	const int wall = side->fixed * half; // the wall's line on the other axis
	const int start = -side->dir * half; // column 0, at the side's left end
	// The view rectangle on this side's own axes
	const int along_min = side->along_x ? view->x_min : view->y_min;
	const int along_max = side->along_x ? view->x_max : view->y_max;
	const int across_min = side->along_x ? view->y_min : view->x_min;
	const int across_max = side->along_x ? view->y_max : view->x_max;
	world_point columns[ARENA_WALL_COLUMNS_MAX];
	short along[ARENA_WALL_COLUMNS_MAX];
	int first;
	int count;

	if (wall < across_min || wall > across_max)
		return; // the wall's line isn't in view
	count = arena_visible_pieces(start, side->dir, along_min, along_max, &first);
	if (count == 0)
		return;

	// Columns spaced by distance from the eye: 250 apart nearby, further apart far away
	count = arena_wall_columns(start, side->dir, first, count,
							   side->along_x ? eye->x : eye->y, (side->along_x ? eye->y : eye->x) - wall,
							   along, ARENA_WALL_COLUMNS_MAX) - 1; // pieces = columns - 1
	for (int i = 0; i <= count; ++i)
	{
		columns[i].x = side->along_x ? along[i] : (short)wall;
		columns[i].y = side->along_x ? (short)wall : along[i];
	}
	if (cutaway)
	{
		// Just the glowing base, using the low strip's buffers with the base texture
		side->low.mesh.data.attbl = base_attributes;
		draw_strip(&side->low, columns, count, WALL_BOTTOM_Z, WALL_BASE_TOP_Z, camera, LOW_MIN_FORWARD);
		side->low.mesh.data.attbl = low_attributes;
		return;
	}
	draw_strip(&side->low, columns, count, WALL_BOTTOM_Z, WALL_SPLIT_Z, camera, LOW_MIN_FORWARD);
	draw_strip(&side->high, columns, count, WALL_SPLIT_Z, WALL_TOP_Z, camera, HIGH_MIN_FORWARD);
}

void draw_arena_walls(const world_rect *view, const world_point *eye, const wall_clip_camera *camera, bool cutaway)
{
	for (int s = 0; s < 4; ++s)
		// Cutaway: the south and west walls stand between the iso camera (which sits on the +x, +y
		// side of the cycle) and the cycle. Full height they'd hide it, and their tops would rise
		// behind the camera. Just the glowing base, lower than the cycle, still marks the edge
		draw_side(&sides[s], view, eye, camera, cutaway && sides[s].fixed > 0);
}
