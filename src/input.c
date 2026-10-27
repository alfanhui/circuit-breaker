#include "input.h"

#define PLAYER1_START_X 0 // the arena centre is the world origin
#define PLAYER1_START_Y 0
#define DEBUG_CAMERA_SPEED 8

cycle player1;
trail player1_trail;
bool debug_camera = false; // START: freeze the cycle and fly the camera around

// Debug: fills the trail with a full-length serpentine across the arena, the worst case for drawing
static void stress_fill_trail(void)
{
	world_point p;
	const short edge = WORLD_PLAYER_LIMIT - 50;
	const short row_spacing = (2 * edge) / (TRAIL_MAX_CORNERS / 2);

	p.x = -edge;
	p.y = -edge;
	trail_reset(&player1_trail, p);
	for (int i = 1; i < TRAIL_MAX_CORNERS; ++i)
	{
		if (i & 1)
			p.x = -p.x;
		else
			p.y += row_spacing;
		trail_set_head(&player1_trail, p);
		trail_add_corner(&player1_trail);
	}
}

void init_players(void)
{
	cycle_init(&player1, PLAYER1_START_X, PLAYER1_START_Y, HEADING_NORTH);
	trail_reset(&player1_trail, player1.position);
	view_init(&player1);
#ifdef PERF_STRESS_TRAIL
	stress_fill_trail();
#endif
}

void debug_pad1(void)
{
	jo_printf(20, 0, "a.pressed %d", is_key_pressed(DIGI_A));
	jo_printf(20, 1, "a.up %d", is_key_up(DIGI_A));
	jo_printf(20, 2, "a.down %d", is_key_down(DIGI_A));
	jo_printf(20, 3, "key changed %d", is_key_change(DIGI_A));
	jo_printf(20, 4, "a.release %d", is_key_release(DIGI_A));
	jo_printf(20, 5, "a.struck %d", is_key_struck(DIGI_A));
}

static void debug_camera_input(void)
{
	if (jo_is_pad1_key_pressed(JO_KEY_UP))
		view_y -= DEBUG_CAMERA_SPEED;
	else if (jo_is_pad1_key_pressed(JO_KEY_DOWN))
		view_y += DEBUG_CAMERA_SPEED;
	if (jo_is_pad1_key_pressed(JO_KEY_LEFT))
		view_x += DEBUG_CAMERA_SPEED;
	else if (jo_is_pad1_key_pressed(JO_KEY_RIGHT))
		view_x -= DEBUG_CAMERA_SPEED;

	// Elevation. view_z moves the world, so the camera's height is -view_z
	if (jo_is_pad1_key_pressed(JO_KEY_L))
		view_z += 4;
	else if (jo_is_pad1_key_pressed(JO_KEY_R))
		view_z -= 4;
	if (view_z > -WORLD_FLOOR_Z) // not through the floor
		view_z = -WORLD_FLOOR_Z;
	if (view_z < -WORLD_CEILING_Z) // not too high
		view_z = -WORLD_CEILING_Z;

	// Pitch
	if (jo_is_pad1_key_pressed(JO_KEY_B))
		view_rx += DEGtoANG(0.5);
	else if (jo_is_pad1_key_pressed(JO_KEY_C))
		view_rx -= DEGtoANG(0.5);
}

void gamepad_input(void)
{
	if (is_key_struck(DIGI_START))
	{
		debug_camera = !debug_camera;
		if (!debug_camera)
			view_init(&player1);
	}
	if (debug_camera)
	{
		debug_camera_input();
		return;
	}

	if (!jo_is_pad1_available())
		return;

	if (is_key_struck(DIGI_DOWN))
		view_toggle(&player1);

	// Turns are instant for the cycle and the wall; only the 1st person camera eases round
	if (is_key_struck(DIGI_LEFT))
	{
		trail_add_corner(&player1_trail);
		cycle_turn_left(&player1);
		view_turn_left();
	}
	else if (is_key_struck(DIGI_RIGHT))
	{
		trail_add_corner(&player1_trail);
		cycle_turn_right(&player1);
		view_turn_right();
	}

	cycle_update(&player1, jo_is_pad1_key_pressed(JO_KEY_UP));
	trail_set_head(&player1_trail, player1.position);

	// A: clear the wall and start a fresh one here
	if (is_key_struck(DIGI_A))
		trail_reset(&player1_trail, player1.position);
	// X: worst-case trail for performance checks (the live segment then runs diagonally to the cycle)
	if (is_key_struck(DIGI_X))
		stress_fill_trail();
	// Y: arena boundary on/off. Turning it back on outside the arena snaps the cycle to the edge next frame
	if (is_key_struck(DIGI_Y))
		player1.bounded = !player1.bounded;

	view_follow(&player1);
}
