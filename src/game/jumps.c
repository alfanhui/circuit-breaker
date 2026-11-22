#include "jumps.h"
#include "city.h"

/*
 * Each place: in the corridor (out from its hole, across in sixteenths of its width from its first
 * side, as corridor.c's boxes), or on a tower (its column and row, and how far back from its middle,
 * out along the corridor: a hub's lift takes you from its middle). Faces: a CITY_FACES_
 */
typedef struct
{
	const char *name;
	bool on_tower;
	short out_or_column, across_or_row;
	short short_of_middle; // on a tower only
	unsigned char faces;
} jump_spot;

static const jump_spot spots[JUMP_COUNT] = {
	{"stay", false, 0, 0, 0, 0},
	{"start", false, 1300, 4, 0, CITY_FACES_OUT},	// boot_on_foot's (input.c): a quarter across
	{"yard", false, 1450, 12, 0, CITY_FACES_OUT},	// the rail's 200 ahead (corridor.c: 1650)
	{"stairs", false, 3250, 4, 0, CITY_FACES_OUT},	// past the crate (3000-3060), the steps from 3360
	{"entry", true, 0, CITY_ENTRY_ROW, 0, CITY_FACES_OUT},
	{"1st drone", true, 0, 11, 0, CITY_FACES_NORTH}, // its loop's stops at rows A and C round you
	{"2nd drone", true, 3, 12, 0, CITY_FACES_OUT},	 // its first stop over (4, C), next door
	{"CD hub", true, CITY_HUB_COLUMN, CITY_HUB_ROW, 150, CITY_FACES_OUT}, // input.c's HUB_SHORT_OF_PAD
	{"Backup", true, CITY_BACKUP_COLUMN, CITY_BACKUP_ROW, 150, CITY_FACES_OUT},
	{"SCSP", true, 14, 14, 0, CITY_FACES_OUT},
};

const char *jump_name(int place)
{
	return place >= 0 && place < JUMP_COUNT ? spots[place].name : "";
}

bool jump_where(const corridor *c, int place, world_point *at, int *height, int *yaw)
{
	const jump_spot *s;

	if (place <= JUMP_NONE || place >= JUMP_COUNT)
		return false;
	s = &spots[place];
	if (s->on_tower)
	{
		const city_tower *t = city_tower_at(s->out_or_column, s->across_or_row);
		const int middle = (c->across_min + c->across_max) >> 1;

		*at = corridor_point(c, ((t->out_min + t->out_max) >> 1) - s->short_of_middle,
							 middle + ((t->across_min + t->across_max) >> 1));
		*height = t->height;
	}
	else
	{
		*at = corridor_point(c, s->out_or_column, c->across_min + (((c->across_max - c->across_min) * s->across_or_row) >> 4));
		*height = 0; // the corridor's floor
	}
	// A heading is a quarter turn: 0x4000 in a binary angle (headings and yaws count the same way)
	*yaw = city_faces_heading(c, s->faces) * 0x4000;
	return true;
}
