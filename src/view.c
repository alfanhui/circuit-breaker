#include "view.h"

// 1st person: camera sits on the cycle and turns with it
#define FIRST_PERSON_FOV 90
#define FIRST_PERSON_Z (-35)
#define FIRST_PERSON_RX DEGtoANG(90.0)
#define FIRST_PERSON_TURN_STEP (0x4000 / 10) // a 90 degree turn eases over 10 frames

// Isometric (3rd person): fixed angle, camera trails the cycle at 5/6 of its movement
#define ISO_FOV 60
#define ISO_Z (-768)
#define ISO_RX DEGtoANG(40.0)
#define ISO_RZ DEGtoANG(45.0)
#define ISO_FOLLOW_NUM 5
#define ISO_FOLLOW_DEN 6
#define ISO_FOLLOW_OFFSET 590 // puts the arena where the old 3rd person view had it

bool first_person = false;
int view_x;
int view_y;
int view_z;
ANGLE view_rx;
ANGLE view_rz;

// Accumulated heading angle, eased towards on every turn
static int heading_target = 0;
static int heading_current = 0;

static void apply_view(const cycle *c)
{
	if (first_person)
	{
		jo_3d_perspective_angle(FIRST_PERSON_FOV);
		view_z = FIRST_PERSON_Z;
		view_rx = FIRST_PERSON_RX;
		heading_current = heading_target;
	}
	else
	{
		jo_3d_perspective_angle(ISO_FOV);
		view_z = ISO_Z;
		view_rx = ISO_RX;
		view_rz = ISO_RZ;
	}
	view_follow(c);
}

void view_init(const cycle *c)
{
	heading_target = c->heading * 0x4000;
	heading_current = heading_target;
	apply_view(c);
}

void view_toggle(const cycle *c)
{
	first_person = !first_person;
	apply_view(c);
}

void view_turn_left(void)
{
	heading_target -= 0x4000;
}

void view_turn_right(void)
{
	heading_target += 0x4000;
}

void view_follow(const cycle *c)
{
	if (first_person)
	{
		view_x = c->position.x;
		view_y = c->position.y;

		if (heading_current < heading_target - FIRST_PERSON_TURN_STEP)
			heading_current += FIRST_PERSON_TURN_STEP;
		else if (heading_current > heading_target + FIRST_PERSON_TURN_STEP)
			heading_current -= FIRST_PERSON_TURN_STEP;
		else
			heading_current = heading_target;
		view_rz = (ANGLE)heading_current;
	}
	else
	{
		view_x = c->position.x * ISO_FOLLOW_NUM / ISO_FOLLOW_DEN + ISO_FOLLOW_OFFSET;
		view_y = c->position.y * ISO_FOLLOW_NUM / ISO_FOLLOW_DEN + ISO_FOLLOW_OFFSET;
	}
}

void view_push_world_matrix(void)
{
	jo_3d_push_matrix();
	slRotX(view_rx);
	slRotZ(view_rz);
	jo_3d_translate_matrix(-view_x, -view_y, view_z);
}
