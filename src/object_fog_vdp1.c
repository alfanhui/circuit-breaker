#include "object_fog_vdp1.h"

/*
 * Where the Gouraud tables live. SGL and jo keep them at the top of VDP1's 512 KB of RAM, from
 * 0x70000; a quad's ATTR.gstb is a table's address / 8. jo's own sprite shading
 * (jo_set_gouraud_shading_colors) counts up from the first table, so these start after
 * TABLES_FOR_JO of them.
 *
 * VDP1 draws one frame while the CPU builds the next. If this frame's tables overwrote the ones
 * VDP1 is still drawing with, the picture on screen would get the new frame's shading: pieces
 * move between frames, so it would flicker. So there are two sets, used on alternate frames.
 */
#define VDP1_RAM 0x25C00000
#define GOURAUD_AREA 0x70000							  // from the start of VDP1 RAM
#define TABLE_BYTES 8									  // four 2-byte colours
#define TABLES_FOR_JO 256								  // left for jo's sprite shading
#define NEUTRAL_TABLE TABLES_FOR_JO						  // all four corners unchanged: shared
#define FRAME_TABLES 1024								  // per frame: walls (~500) + trail (~200)
#define FIRST_FRAME_TABLE (NEUTRAL_TABLE + 1)			  // set 0; set 1 follows it
#define TABLES_END (FIRST_FRAME_TABLE + 2 * FRAME_TABLES) // 0x74808: what textures mustn't reach

// jo's textures fill VDP1 RAM upwards from 0xE1C0 (its user area: 0x71D38 bytes up to 0x7FEF8).
// They must stop below the tables; jo reports how much of that area is used, as a percentage
#define TEXTURE_AREA_START 0xE1C0
#define TEXTURE_AREA_SIZE 0x71D38
#define TEXTURE_PERCENT_MAX ((GOURAUD_AREA - TEXTURE_AREA_START) * 100 / TEXTURE_AREA_SIZE) // 86

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
	volatile unsigned short *table;

	// No fog at any corner (all near ones), or out of tables: the shared table that changes nothing.
	// | is 0 only if every level is 0
	if ((level0 | level1 | level2 | level3) == 0 || next_table == end_table)
		return table_gstb(NEUTRAL_TABLE);
	table = table_address(next_table);
	table[0] = colours[level0];
	table[1] = colours[level1];
	table[2] = colours[level2];
	table[3] = colours[level3];
	return table_gstb(next_table++);
}
