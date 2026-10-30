#include "input.h"
#include "game/bot.h"

#define PLAYER1_START_X 0 // the arena centre is the world origin
#define PLAYER1_START_Y 0
// The bot starts 2000 to the east of the player (east is -x), riding the same way
#define BOT1_START_X (-2000)
#define BOT1_START_Y 0
#define DEBUG_CAMERA_SPEED 8

rider player1;
rider bot1;
game_state game;
static bot_mind bot1_mind; // what the bot remembers between frames (game/bot.h)
static const rider *const bot1_rivals[] = {&player1}; // who the bot chases, if it can see them
// Every trail on the field: what every rider crashes into, the bot sees, and the radar shows.
// The order is the radar's colours: player 1 cyan, then orange
const trail *const field_trails[FIELD_TRAIL_COUNT] = {&player1.wall, &bot1.wall};
const trail *solid_trails[FIELD_TRAIL_COUNT];
int solid_trail_count;

// The player's trail always counts (a crash ends the game); the bot's only while it's alive
static void update_solid_trails(void)
{
	solid_trail_count = 0;
	solid_trails[solid_trail_count++] = &player1.wall;
	if (bot1.alive)
		solid_trails[solid_trail_count++] = &bot1.wall;
}
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
	trail_reset(&player1.wall, p);
	for (int i = 1; i < TRAIL_MAX_CORNERS; ++i)
	{
		if (i & 1)
			p.x = -p.x;
		else
			p.y += row_spacing;
		trail_set_head(&player1.wall, p);
		trail_add_corner(&player1.wall);
	}
}

void init_players(void)
{
	game_init(&game);
	rider_init(&player1, PLAYER1_START_X, PLAYER1_START_Y, HEADING_NORTH);
	rider_init(&bot1, BOT1_START_X, BOT1_START_Y, HEADING_NORTH);
	bot_init(&bot1_mind, 1); // the seed picks its reaction times: the same every run
	update_solid_trails();
	view_init(&player1.bike);
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
	rider_controls controls;

	// Slow motion: skip the game update on most frames. Skipped before the presses are taken,
	// so a press waits in the latch for the next update rather than being lost
	++frame;
	if (debug_slow_motion && (frame & (DEBUG_SLOW_MOTION_FRAMES - 1)) != 0)
		return;

	// Every press since the last game frame, even if this frame spanned several vblanks.
	// Taken first and on every frame, so presses can't pile up while the debug camera is on.
	struck = press_latch_take(&pad1_presses);

	// Terminated: everything is frozen behind the terminal screen (main.c) until a button is
	// pressed or 3 s pass, then it starts again from the beginning (game/game_state.h)
	if (game.phase == GAME_TERMINATED)
	{
		if (game_update(&game, 0, struck != 0))
			init_players();
		return;
	}

	if (struck & DIGI_START)
	{
		debug_camera = !debug_camera;
		if (!debug_camera)
			view_init(&player1.bike);
	}
	if (debug_camera)
	{
		debug_camera_input();
		return;
	}

	if (!jo_is_pad1_available())
		return;

	if (struck & DIGI_DOWN)
		view_toggle(&player1.bike);

	// The pad becomes this frame's controls: the same thing a bot decides (game/rider.h). Turns
	// are instant for the cycle and the wall; only the 1st person camera eases round with them.
	// Boost (held) comes from the same pad sample as the turns (struck), so a turn and a boost
	// pressed together land in the same frame
	controls.turn = (struck & DIGI_LEFT) ? TURN_LEFT : (struck & DIGI_RIGHT) ? TURN_RIGHT : TURN_NONE;
	controls.boost = (press_latch_held(&pad1_presses) & DIGI_UP) != 0;
	switch (rider_step(&player1, controls, solid_trails, solid_trail_count))
	{
	case TURN_LEFT:
		view_turn_left();
		break;
	case TURN_RIGHT:
		view_turn_right();
		break;
	}
	// A crash terminates the player's process: the terminal screen from the next frame
	game_update(&game, player1.crashed, 0);

	// The bot decides its buttons from what it can see, then rides by the same rules
	// A crash terminates the bot's process: it stops, and its trail sinks into the floor (and
	// from now on stops nobody)
	if (bot1.alive)
	{
		rider_step(&bot1, bot_think(&bot1_mind, &bot1, bot1_rivals, 1, solid_trails, solid_trail_count), solid_trails,
				   solid_trail_count);
		if (bot1.crashed)
		{
			rider_kill(&bot1);
			update_solid_trails();
		}
	}
	else
		rider_sink(&bot1);

	// A: clear every wall, each starting fresh where its cycle is
	if (struck & DIGI_A)
	{
		trail_reset(&player1.wall, player1.bike.position);
		trail_reset(&bot1.wall, bot1.bike.position);
	}
	// X: worst-case trail for performance checks (the live segment then runs diagonally to the cycle)
	if (struck & DIGI_X)
		stress_fill_trail();
	// Y: collision on/off (off: a ghost, through trails and out of the arena). Turning it back on
	// outside the arena snaps the cycle to the edge next frame
	if (struck & DIGI_Y)
		player1.bike.bounded = !player1.bike.bounded;
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

	view_follow(&player1.bike);
}
