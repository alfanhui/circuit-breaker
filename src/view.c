#include "view.h"
#include "floor_fade_vdp2.h"

// 1st person: camera sits on the cycle and turns with it
#define FIRST_PERSON_FOV 90
#define FIRST_PERSON_Z (-35)
#define FIRST_PERSON_RX DEGtoANG(90.0)
#define FIRST_PERSON_TURN_STEP (0x4000 / 10) // a 90 degree turn eases over 10 frames

// Isometric (3rd person): fixed angle, camera follows the cycle 1:1.
// (It used to follow at 5/6 of the movement, which in a large arena left the cycle
// more than 1000 units behind the camera, off the screen.)
#define ISO_FOV 60
#define ISO_Z (-768)
#define ISO_RX DEGtoANG(40.0)
#define ISO_RZ DEGtoANG(45.0)
// How far behind the cycle the camera sits, on each axis, so the cycle is in the middle of the
// screen. ISO_RX is measured from straight down, so the camera looks 50 degrees below level and
// its line of sight meets the floor 768 / tan(50) = 644 ahead, along the 45 degree diagonal:
// 644 / sqrt(2) = 456 on each axis. Change ISO_Z or ISO_RX and this needs working out again.
#define ISO_FOLLOW_OFFSET 456

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
	floor_fade_set_first_person(first_person); // each camera sees the floor at different distances
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

world_rect view_visible_rect(const cycle *c, bool free_camera, int range)
{
	view_cull_mode mode;

	if (free_camera)
		mode = VIEW_CULL_ALL;
	else if (!first_person)
		mode = VIEW_CULL_ISO;
	else if (heading_current != heading_target)
		mode = VIEW_CULL_TURNING; // easing round: the camera faces somewhere between two headings
	else
		mode = VIEW_CULL_FIRST_PERSON;
	return view_cull_rect(c->position, c->heading, mode, range);
}

bool view_wall_clip_camera(const cycle *c, bool free_camera, wall_clip_camera *out)
{
	ANGLE yaw = (ANGLE)heading_current; // where the camera faces now, even mid-turn

	if (free_camera || !first_person)
		return false;
	out->eye = c->position;
	// Heading 0 faces north (-y) and turning right goes east (-x): forward = (-sin, -cos).
	// slSin/slCos give 16.16 fixed point; >> 6 makes it x1024
	out->forward_x = -(slSin(yaw) >> 6);
	out->forward_y = -(slCos(yaw) >> 6);
	return true;
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
		view_x = c->position.x + ISO_FOLLOW_OFFSET;
		view_y = c->position.y + ISO_FOLLOW_OFFSET;
	}
}

void view_push_world_matrix(void)
{
	jo_3d_push_matrix();
	slRotX(view_rx);
	slRotZ(view_rz);
	// Hand SGL the world at 1/VIEW_WORLD_SCALE size. SGL stops drawing polygons a little under
	// 8192 units deep (likely a 16.16 overflow inside it: 8192 << 16 = 0x20000000), but the arena
	// is up to 28000 across. Shrinking everything round the camera doesn't change the picture
	// (screen position is x / depth, and both shrink together); only SGL's depths get smaller.
	// Game logic and culling stay in world units.
	slScale(JO_FIXED_1 / VIEW_WORLD_SCALE, JO_FIXED_1 / VIEW_WORLD_SCALE, JO_FIXED_1 / VIEW_WORLD_SCALE);
	jo_3d_translate_matrix(-view_x, -view_y, view_z);
}
