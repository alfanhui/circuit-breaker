#include "input.h"
#include "game/turn_gate.h"

#define PLAYER1_START_X 0 // the arena centre is the world origin
#define PLAYER1_START_Y 0
#define DEBUG_CAMERA_SPEED 8

cycle player1;
trail player1_trail;
static turn_gate player1_turns; // a minimum gap between turns (game/turn_gate.h)
bool debug_camera = false; // START: freeze the cycle and fly the camera around
bool debug_slow_motion = false;

// Debug: fills the trail with a full-length serpentine across the arena, the worst case for drawing
static void stress_fill_trail(void)
{
	world_point p;
	const short edge = (short)(arena_player_limit() - 50);
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
	turn_gate_init(&player1_turns);
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

// Z steps through the ways to compare turn cameras: ease-out, ease-out slowed down,
// the old linear turn slowed down, the linear turn at full speed, then back to the start
static void debug_next_turn_compare(void)
{
	static int stage = 0;

	stage = (stage + 1) & 3;
	view_linear_turn = stage >= 2;
	debug_slow_motion = stage == 1 || stage == 2;
}

void gamepad_input(void)
{
	static unsigned int frame = 0;
	unsigned short struck;
	int turn;
	world_point before;

	// Slow motion: skip the game update on most frames. Skipped before the presses are taken,
	// so a press waits in the latch for the next update rather than being lost
	++frame;
	if (debug_slow_motion && (frame & (DEBUG_SLOW_MOTION_FRAMES - 1)) != 0)
		return;

	// Every press since the last game frame, even if this frame spanned several vblanks.
	// Taken first and on every frame, so presses can't pile up while the debug camera is on.
	struck = press_latch_take(&pad1_presses);

	if (struck & DIGI_START)
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

	if (struck & DIGI_DOWN)
		view_toggle(&player1);

	// Turns are instant for the cycle and the wall; only the 1st person camera eases round.
	// The gate holds back a turn pressed within a cycle-width of the last one (a quick double
	// tap) until that gap is reached, so a U-turn never lands on the cycle's own trail
	turn = turn_gate_update(&player1_turns, (struck & DIGI_LEFT)	? TURN_LEFT
											: (struck & DIGI_RIGHT) ? TURN_RIGHT
																	: TURN_NONE);
	if (turn == TURN_LEFT)
	{
		trail_add_corner(&player1_trail);
		cycle_turn_left(&player1);
		view_turn_left();
	}
	else if (turn == TURN_RIGHT)
	{
		trail_add_corner(&player1_trail);
		cycle_turn_right(&player1);
		view_turn_right();
	}

	// Boost (held) comes from the same pad sample as the turns (struck), so a turn and a boost
	// pressed together land in the same frame. It used to be jo's own pad read, filled at a
	// different point in the frame: the two could be a frame apart
	before = player1.position;
	cycle_update(&player1, (press_latch_held(&pad1_presses) & DIGI_UP) != 0);
	// How far it went: it only moves along one axis, so the two differences add up to it
	turn_gate_moved(&player1_turns, JO_ABS(player1.position.x - before.x) + JO_ABS(player1.position.y - before.y));
	trail_set_head(&player1_trail, player1.position);

	// A: clear the wall and start a fresh one here
	if (struck & DIGI_A)
		trail_reset(&player1_trail, player1.position);
	// X: worst-case trail for performance checks (the live segment then runs diagonally to the cycle)
	if (struck & DIGI_X)
		stress_fill_trail();
	// Y: arena boundary on/off. Turning it back on outside the arena snaps the cycle to the edge next frame
	if (struck & DIGI_Y)
		player1.bounded = !player1.bounded;
	// Z: compare the turn cameras (debug text line 5)
	if (struck & DIGI_Z)
		debug_next_turn_compare();

	// Tuning, to find the balance of speed and scale by feel. The debug text shows both.
	// L / R: arena smaller / larger. A cycle left outside a shrunk arena is pulled to the edge next frame
	if (struck & DIGI_L)
		arena_set_pieces(arena_pieces() - ARENA_PIECES_STEP);
	if (struck & DIGI_R)
		arena_set_pieces(arena_pieces() + ARENA_PIECES_STEP);
	// B / C: cruise speed down / up
	if (struck & DIGI_B)
		cycle_set_cruise_speed(cycle_cruise_speed() - 1);
	if (struck & DIGI_C)
		cycle_set_cruise_speed(cycle_cruise_speed() + 1);

	view_follow(&player1);
}
