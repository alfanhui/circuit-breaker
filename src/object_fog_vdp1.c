#include "object_fog_vdp1.h"

/*
 * Where the Gouraud tables live: near the top of VDP1's 512 KB of RAM, from 0x70000; a quad's
 * jo_3d_polygon_attribute.gouraud_table is a table's address / 8. jo keeps its own higher up, clear
 * of ours: its sprite shading (jo_set_gouraud_shading_colors) from 0x7E4F8, its 3D's from 0x7E700
 * (the polygons it cuts) and 0x7FF00 (lighting). Until jo-engine v2026.2 its sprite shading
 * counted up from 0x70000, so ours start after TABLES_FOR_JO of them: 2 KB now unused (BACKLOG).
 *
 * Most quads use a shared table (game/depth_fog.h: a wall or trail piece's corners are two levels,
 * in one of two orders), all 512 written once at start-up. Only a quad whose corners are neither
 * gets a table of its own, written as it's drawn: each such write stops VDP1 drawing until it's
 * done (the CPU always wins: SEGA's VDP1 manual), and before the shared tables every fogged
 * quad's was written so, hundreds a frame.
 *
 * VDP1 draws one frame while the CPU builds the next. If this frame's own tables overwrote the ones
 * VDP1 is still drawing with, the picture on screen would get the new frame's shading: pieces
 * move between frames, so it would flicker. So there are two sets, used on alternate frames.
 */
#define VDP1_RAM 0x25C00000
#define GOURAUD_AREA 0x70000							  // from the start of VDP1 RAM
#define TABLE_BYTES 8									  // four 2-byte colours
#define TABLES_FOR_JO 256								  // was jo's sprite shading (before v2026.2)
#define NEUTRAL_TABLE TABLES_FOR_JO						  // all four corners unchanged: shared
#define FIRST_SHARED_TABLE (NEUTRAL_TABLE + 1)			  // DEPTH_FOG_SHARED_TABLES of them (512)
#define FRAME_TABLES 1024								  // per frame: the faces on foot cut to the view
#define FIRST_FRAME_TABLE (FIRST_SHARED_TABLE + DEPTH_FOG_SHARED_TABLES) // set 0; set 1 follows it
#define TABLES_END (FIRST_FRAME_TABLE + 2 * FRAME_TABLES) // 0x75808: below jo 2026's 3D tables (0x7E700)

// jo's textures fill VDP1 RAM upwards from 0x10000 (its user area: 0x6E4F8 bytes in v2026.2, up to
// jo's own Gouraud tables; under SGL it was 0xE1C0 and 0x71D38). They must stop below our tables; jo reports how
// much of that area is used, as a percentage
#define TEXTURE_AREA_START JO_VDP1_TEXTURE_DEF_BASE_ADDRESS // 0x10000
#define TEXTURE_AREA_SIZE JO_VDP1_USER_AREA_SIZE			// 0x6E4F8
#define TEXTURE_PERCENT_MAX ((GOURAUD_AREA - TEXTURE_AREA_START) * 100 / TEXTURE_AREA_SIZE) // 87

bool object_fog_enabled = false;

static depth_fog fog;
static bool fog_on;									   // this frame
static unsigned short colours[DEPTH_FOG_LEVELS];	   // the Gouraud colour for each level
static unsigned int next_table;						   // the next free table this frame
static unsigned int end_table;						   // one past this frame's last table
static unsigned int frame_set;						   // 0 or 1: which set of tables this frame

static volatile unsigned short *table_address(unsigned int table)
{
	return (volatile unsigned short *)(VDP1_RAM + GOURAUD_AREA + table * TABLE_BYTES);
}

static unsigned short table_gstb(unsigned int table)
{
	return (unsigned short)((GOURAUD_AREA / TABLE_BYTES) + table); // 0xE000 + table
}

void object_fog_init(void)
{
	volatile unsigned short *neutral = table_address(NEUTRAL_TABLE);

	if (jo_sprite_usage_percent() >= TEXTURE_PERCENT_MAX)
	{
		jo_core_error("Textures reach the fog tables: wall/trail fog is off");
		return;
	}
	// The colour per level, worked out once (depth_fog_gouraud_colour divides)
	for (int level = 0; level < DEPTH_FOG_LEVELS; ++level)
		colours[level] = depth_fog_gouraud_colour(level);
	for (int corner = 0; corner < 4; ++corner)
		neutral[corner] = DEPTH_FOG_GOURAUD_NEUTRAL;
	// The shared tables, once: never written again
	for (int t = 0; t < DEPTH_FOG_SHARED_TABLES; ++t)
	{
		volatile unsigned short *table = table_address(FIRST_SHARED_TABLE + t);
		int levels[4];

		depth_fog_shared_levels(t, levels);
		for (int corner = 0; corner < 4; ++corner)
			table[corner] = colours[levels[corner]];
	}
	object_fog_enabled = true;
}

void object_fog_set_distances(int fade_start, int fade_end)
{
	depth_fog_set_distances(&fog, fade_start, fade_end);
}

void object_fog_begin_frame(bool on, world_point eye, int forward_x, int forward_y)
{
	fog_on = on;
	depth_fog_set_camera(&fog, eye, forward_x, forward_y);
	frame_set ^= 1; // the other set from last frame: VDP1 may still be drawing with that one
	next_table = FIRST_FRAME_TABLE + frame_set * FRAME_TABLES;
	end_table = next_table + FRAME_TABLES;
}

int object_fog_level(int x, int y)
{
	return fog_on ? depth_fog_level(&fog, x, y) : 0;
}

unsigned short object_fog_quad(int level0, int level1, int level2, int level3)
{
	int shared;
	volatile unsigned short *table;

	// No fog at any corner (all near ones): the table that changes nothing. | is 0 only if every
	// level is 0
	if ((level0 | level1 | level2 | level3) == 0)
		return table_gstb(NEUTRAL_TABLE);
	// A wall or trail piece's two levels: its shared table. Nothing written, so VDP1 isn't stopped
	shared = depth_fog_shared_table(level0, level1, level2, level3);
	if (shared >= 0)
		return table_gstb(FIRST_SHARED_TABLE + shared);
	// Any other: one of its own this frame, or out of them, none (unfogged, a frame)
	if (next_table == end_table)
		return table_gstb(NEUTRAL_TABLE);
	table = table_address(next_table);
	table[0] = colours[level0];
	table[1] = colours[level1];
	table[2] = colours[level2];
	table[3] = colours[level3];
	return table_gstb(next_table++);
}

jo_color object_fog_flat_colour(jo_color colour, int level)
{
	// colours[]: each level's Gouraud colour, worked out once (object_fog_init); a channel's 16
	// leaves the colour alone
	return level <= 0 ? colour : (jo_color)depth_fog_darken((unsigned short)colour, 16 - (colours[level] & 31));
}

void object_fog_set_quad(jo_3d_polygon_attribute *attribute, int level0, int level1, int level2, int level3)
{
	attribute->gouraud_table = object_fog_quad(level0, level1, level2, level3);
	// Gouraud only where there's fog to shade: with the flag on, VDP1 works out every pixel's
	// shade even from the table that changes nothing, and on the huge near wall pieces that cost
	// a quarter of the frame rate (60 fps fell to ~45 riding along the wall). Near pieces are
	// never fogged (the fog starts hundreds of units out), so they draw as plain textures
	if ((level0 | level1 | level2 | level3) == 0)
		attribute->draw_mode &= ~JO_VDP1_COLOR_CALC_GOURAUD;
	else
		attribute->draw_mode |= JO_VDP1_COLOR_CALC_GOURAUD;
}
