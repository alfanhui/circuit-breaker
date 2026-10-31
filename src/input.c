#include "input.h"
#include "game/bot.h"
#include "game/camera_shake.h"
#include "game/attract_route.h"
#include "sound.h"
#include "utils/perf.h"

#define PLAYER1_START_X 0 // the arena centre is the world origin
#define PLAYER1_START_Y 0
// The bot starts 2000 to the east of the player (east is -x), riding the same way
#define BOT1_START_X (-2000)
#define BOT1_START_Y 0
#define DEBUG_CAMERA_SPEED 8

rider player1;
rider bot1;
game_state game;
status_message status;
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
/*
 * The prologue's loop (docs/STORY.md): while spectating, the cycle writes "SEGA SATURN SH-2"
 * across the floor with its trail, by itself, for ever (game/attract_text.h builds the route).
 * The trail is only laid on a letter's strokes (pen down); between them the cycle rides with
 * none, boosting. Each finished stroke is kept in strokes[], and once a word is spelt its
 * strokes start to sink into the floor (as a dead bot's trail does) while the next word is
 * written. It rides as a ghost: strokes touch and cross, and there's nothing to crash into.
 *
 * At the end of the message it's put back one ATTRACT_TEXT_PERIOD west, at the start, and so is
 * the camera: the floor's grid looks the same there, and the last word has sunk, so the jump
 * can't be seen.
 */
#define ATTRACT_ORIGIN_X 7000 // the first letter's bottom left: the message runs east (-x) from
#define ATTRACT_ORIGIN_Y 500  // here, 13800 long, centred on the arena; letters reach 1000 north
#define ATTRACT_STROKES_MAX 24 // the message has 21
#define ATTRACT_WORDS_MAX 4	   // and 3 words

static attract_waypoint attract_waypoints[ATTRACT_TEXT_MAX_WAYPOINTS];
static int attract_waypoint_count;
static attract_route route;
trail attract_strokes[ATTRACT_STROKES_MAX]; // the finished strokes, standing or sinking
unsigned char attract_stroke_word[ATTRACT_STROKES_MAX]; // the word each belongs to
int attract_stroke_count;
unsigned char attract_word_height[ATTRACT_WORDS_MAX]; // how tall each word's strokes stand
static unsigned char word_sinking[ATTRACT_WORDS_MAX];
static int current_word;

// The cycle at the start of the message, facing its first leg, with no trail and nothing
// written. Its speed carries on, so one message runs into the next with no pause
static void start_message(void)
{
	attract_route_init(&route, attract_waypoints, attract_waypoint_count);
	player1.bike.position = attract_route_start(&route);
	player1.bike.heading = (unsigned char)attract_route_start_heading(&route);
	trail_reset(&player1.wall, player1.bike.position);
	turn_gate_init(&player1.turns);
	attract_stroke_count = 0;
	current_word = 0;
	for (int w = 0; w < ATTRACT_WORDS_MAX; ++w)
	{
		attract_word_height[w] = TRAIL_HEIGHT;
		word_sinking[w] = 0;
	}
}

// One frame of the loop, before the cycle moves: the turn to press and whether to boost, and
// the strokes and words as each waypoint is reached
static void attract_frame(rider_controls *controls)
{
	int turn;
	const attract_waypoint *here = attract_route_step(&route, &player1.bike, &turn);

	for (int w = 0; w < ATTRACT_WORDS_MAX; ++w)
		if (word_sinking[w] && attract_word_height[w] > 0)
			attract_word_height[w] = (unsigned char)(attract_word_height[w] > RIDER_SINK_SPEED
														 ? attract_word_height[w] - RIDER_SINK_SPEED
														 : 0);
	if (here)
	{
		int pen_on = attract_route_pen(&route); // on the leg it rides next

		// It was put on the waypoint exactly: so is its trail, so the corner is square
		trail_set_head(&player1.wall, player1.bike.position);
		if (here->pen && !pen_on && attract_stroke_count < ATTRACT_STROKES_MAX)
		{
			// A stroke's done: keep it (a copy: ~270 bytes, 21 times a message)
			attract_strokes[attract_stroke_count] = player1.wall;
			attract_stroke_word[attract_stroke_count++] = (unsigned char)current_word;
		}
		if (!here->pen && pen_on)
			trail_reset(&player1.wall, player1.bike.position); // the pen goes down: a new stroke
		if (here->word_end && current_word < ATTRACT_WORDS_MAX)
			word_sinking[current_word++] = 1; // spelt: it starts to sink
	}
	if (attract_route_done(&route))
	{
		// Back a period west, to the start; the camera jumps with it (view.c)
		view_spectate_shift(ATTRACT_TEXT_PERIOD);
		start_message();
		turn = TURN_NONE;
	}
	controls->turn = (signed char)turn;
	controls->boost = (unsigned char)!attract_route_pen(&route); // quicker between strokes
}

// Spectating (the prologue's loop) or playing. In the loop the CPU is alone: the bot is taken
// out of the field, and comes in fresh at its start when play begins (docs/STORY.md)
static void set_spectating(bool on)
{
	spectating = on;
	player1.bike.bounded = !on; // a ghost on the route; solid again when play begins
	attract_stroke_count = 0;
	if (on)
	{
		world_point origin;

		rider_remove(&bot1);
		loop_break_init(&breaker);
		if (attract_waypoint_count == 0) // built once
		{
			origin.x = ATTRACT_ORIGIN_X;
			origin.y = ATTRACT_ORIGIN_Y;
			attract_waypoint_count = attract_text_build(ATTRACT_TEXT, origin, attract_waypoints, ATTRACT_TEXT_MAX_WAYPOINTS);
		}
		start_message();
	}
	else
	{
		rider_init(&bot1, BOT1_START_X, BOT1_START_Y, HEADING_NORTH);
		bot_init(&bot1_mind, 1);
	}
	update_solid_trails();
}

/*
 * Breaking the loop (game/loop_break.h, docs/STORY.md): while spectating, a direction pressed is
 * a hit on the loop: a burst of light and a flash (main.c), a synth stab and a jolt of the
 * camera, harder each time. The third breaks it: the cycle turns off its route (the way pressed,
 * or right for up / down), and from there it's the player's. For now the camera cuts straight
 * to 1st person; it'll fall out of the stands into the cycle later.
 */
loop_break breaker;
static const short hit_shake[LOOP_BREAK_PRESSES + 1] = {0, 364, 637, 910}; // 2, 3.5 and 5 degrees
#define DIRECTIONS (DIGI_LEFT | DIGI_RIGHT | DIGI_UP | DIGI_DOWN)

// A direction pressed while spectating. Returns the presses, changed: the one that breaks the
// loop becomes a turn (it's the first thing the free cycle does); the others are spent on the hit
static unsigned short hit_the_loop(unsigned short struck)
{
	int press = loop_break_press(&breaker);

	sound_play_hit(press);
	view_shake(hit_shake[press]);
	if (press < LOOP_BREAK_PRESSES)
		return (unsigned short)(struck & ~DIRECTIONS);
	// Broken: play from here, in 1st person, the bot coming in
	set_spectating(false);
	first_person = true;
	view_init(&player1.bike);
	// A fresh trail from here: turning off the route, it mustn't run into the stroke it was writing
	trail_reset(&player1.wall, player1.bike.position);
	if (!(struck & (DIGI_LEFT | DIGI_RIGHT)))
		struck |= DIGI_RIGHT; // up or down: it still turns off the route
	return (unsigned short)(struck & ~(DIGI_UP | DIGI_DOWN)); // not a view switch
}

bool debug_camera = false; // START: freeze the cycle and fly the camera around
bool debug_slow_motion = false;
bool debug_no_termination = false; // debug camera Z: a crash only stops the cycle

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
	status_init(&status);
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

static void debug_camera_input(unsigned short struck)
{
	// X / Y: arena smaller / bigger (L and R are the camera's height here, and look aside in play).
	// A cycle left outside a shrunk arena is pulled to the edge when play resumes: a crash
	if (struck & DIGI_X)
		arena_set_pieces(arena_pieces() - ARENA_PIECES_STEP);
	if (struck & DIGI_Y)
		arena_set_pieces(arena_pieces() + ARENA_PIECES_STEP);
	// Z: termination on / off. Off, a crash just stops the cycle, as it did before crashes
	// terminated the player: for the E2E steps that crash on purpose to look at what's left
	if (struck & DIGI_Z)
		debug_no_termination = !debug_no_termination;
	// A: the spectator camera on / off (the prologue's, view.c), taking effect on leaving the
	// debug camera. Until the prologue's done the game starts in 1st person, so the E2E steps
	// written for that still work
	if (struck & DIGI_A)
		set_spectating(!spectating);

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

/*
 * One step of the game: a sixtieth of a second. struck: the presses to act on (each acted on in
 * one step only: the first of a frame's steps; held buttons, like boost, count in every step).
 */
static void game_step(unsigned short struck)
{
	rider_controls controls;

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
		debug_camera_input(struck);
		return;
	}

	if (!jo_is_pad1_available())
		return;

	loop_break_update(&breaker);
	if (spectating && (struck & DIRECTIONS))
		struck = hit_the_loop(struck);

	if (struck & DIGI_DOWN)
		view_toggle(&player1.bike);

	// The pad becomes this frame's controls: the same thing a bot decides (game/rider.h). Turns
	// are instant for the cycle and the wall; only the 1st person camera eases round with them.
	// Boost (held) comes from the same pad sample as the turns (struck), so a turn and a boost
	// pressed together land in the same frame
	controls.turn = (struck & DIGI_LEFT) ? TURN_LEFT : (struck & DIGI_RIGHT) ? TURN_RIGHT : TURN_NONE;
	controls.boost = (press_latch_held(&pad1_presses) & DIGI_UP) != 0;
	// Spectating, the loop rides the cycle, not the pad (the presses that break it: next step)
	if (spectating)
		attract_frame(&controls);
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
	game_update(&game, player1.crashed && !debug_no_termination, 0);

	// The loop: between strokes (pen up) it lays no trail: just a point that follows it
	if (spectating && !attract_route_pen(&route))
		trail_reset(&player1.wall, player1.bike.position);

	// The bot decides its buttons from what it can see, then rides by the same rules
	// A crash terminates the bot's process: it stops, and its trail sinks into the floor (and
	// from now on stops nobody)
	if (bot1.alive)
	{
		rider_step(&bot1, bot_think(&bot1_mind, &bot1, bot1_rivals, 1, solid_trails, solid_trail_count), solid_trails,
				   solid_trail_count);
		if (bot1.crashed)
		{
			// Whatever it hit (your trail, its own, the arena edge), make it felt and say so: a
			// jolt of the camera, and the status line. It may have been a long way off, or behind you
			view_shake(CAMERA_SHAKE_BOT_KILL);
			status_show(&status, STATUS_ENEMY_ELIMINATED);
			rider_kill(&bot1);
			update_solid_trails();
		}
	}
	else
		rider_sink(&bot1);
	status_update(&status); // the status line types on, and goes once it's been read

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
	// (The arena's size is in the debug camera: START, then X / Y.) B / C: cruise speed down / up
	if (struck & DIGI_B)
		cycle_set_cruise_speed(cycle_cruise_speed() - 1);
	if (struck & DIGI_C)
		cycle_set_cruise_speed(cycle_cruise_speed() + 1);

	// L / R held: look left / right (1st person; both or neither: ahead). The head eases round
	{
		unsigned short held = press_latch_held(&pad1_presses);
		int left = (held & DIGI_L) != 0;
		int right = (held & DIGI_R) != 0;

		view_look(left == right ? 0 : left ? -1 : 1);
	}
	view_follow(&player1.bike);
}

/*
 * Each frame: the presses, then as many game steps as the last frame took vblanks. Everything
 * moves a fixed amount a step (a cycle's speed is units per step), so at a steady 60 frames a
 * second it's one step a frame. When a frame takes longer (VDP1 can't finish drawing in time),
 * the game catches up with an extra step or two, and keeps its speed: a slowdown shows as
 * choppier motion, not as the whole game running slow (it used to: at 45 fps, 75% speed).
 * At most FRAME_STEPS_MAX steps: a very slow frame mustn't bring on even more work.
 */
#define FRAME_STEPS_MAX 3

void gamepad_input(void)
{
	static unsigned int frame = 0;
	unsigned short struck;
	int steps = perf_last_frame_vblanks;

	// Slow motion: skip the game update on most frames. Skipped before the presses are taken,
	// so a press waits in the latch for the next update rather than being lost. One step a frame
	// then: slow motion is for watching it, frame by frame
	++frame;
	if (debug_slow_motion)
	{
		if ((frame & (DEBUG_SLOW_MOTION_FRAMES - 1)) != 0)
			return;
		steps = 1;
	}

	// Every press since the last frame, even if it spanned several vblanks. Taken first and on
	// every frame, so presses can't pile up while the debug camera is on.
	struck = press_latch_take(&pad1_presses);

	if (steps > FRAME_STEPS_MAX)
		steps = FRAME_STEPS_MAX;
	for (int i = 0; i < steps; ++i)
		game_step(i == 0 ? struck : 0);
}
