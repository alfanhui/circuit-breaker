#include "input.h"
#include "game/bot.h"
#include "game/camera_shake.h"
#include "game/attract_route.h"
#include "game/arrival_flight.h"
#include "game/walker.h"
#include "level.h"
#include "scene_memory.h"
#include "sound.h"
#include "utils/perf.h"

#define PLAYER1_START_X 0 // the arena centre is the world origin
#define PLAYER1_START_Y 0
#define DEBUG_CAMERA_SPEED 8
// The round play starts in, from 0 (game/rounds.h). Like BOOT_SCENE (below): to work on a later
// round, set it and build, so its bots and layout are there from the start
#define BOOT_ROUND 0
// 1: the game boots stood up in the corridor behind round 5's flicker, as if you'd just got up
// after the designed crash (boot_on_foot): for working on Work RAM without riding there first.
// Films don't depend on it: tests/e2e/run_e2e.ps1 builds with whichever a film needs (wait_boot:
// the arena, 0; wait_up: the corridor, 1) and puts it back afterwards
#define BOOT_ON_FOOT 1

rider player1;
rider bots[BOT_COUNT_MAX];
game_state game;
status_message status;
rounds arena_rounds;
wall_flicker arena_flicker;
breach arena_breach;
corridor *arena_corridor = JO_NULL;
static bot_mind bot_minds[BOT_COUNT_MAX]; // what each bot remembers between frames (game/bot.h)
// How hard the bots play (game/bot.h: BOT_EASY, _MEDIUM, _HARD): A in play steps through them.
// Kept through rounds and restarts
int bot_skill_level = BOT_EASY;
static const char *const skill_titles[BOT_SKILL_COUNT] = {"Difficulty: Easy", "Difficulty: Medium",
															 "Difficulty: Hard"};

// How hard the bots play now: the difficulty picked, but never below the round's least
// (rounds_min_skill: Medium in rounds 4 and 5)
int bot_skill_now(void)
{
	const int least = rounds_min_skill(&arena_rounds);

	return bot_skill_level > least ? bot_skill_level : least;
}

// Who the bots chase, if they can see them: you. They're a team against you, not each other: a
// round is yours once they're all gone, and bots knocking each other out would make it a wait
static const rider *const bot_rivals[] = {&player1};
// Every trail on the field: what every rider crashes into, the bots see, and the radar shows.
// The order is the radar's colours (hud.c): player 1 cyan, then the bots orange, pink and green
const trail *const field_trails[FIELD_TRAIL_COUNT] = {&player1.wall, &bots[0].wall, &bots[1].wall, &bots[2].wall};
const trail *solid_trails[FIELD_TRAIL_COUNT];
int solid_trail_count;

// The player's trail always counts (a crash ends the game); a bot's only while it's alive
static void update_solid_trails(void)
{
	solid_trail_count = 0;
	solid_trails[solid_trail_count++] = &player1.wall;
	for (int i = 0; i < BOT_COUNT_MAX; ++i)
		if (bots[i].alive)
			solid_trails[solid_trail_count++] = &bots[i].wall;
}

// How many bots are still riding: the round's over at none (game/rounds.h)
static int bots_alive(void)
{
	int alive = 0;

	for (int i = 0; i < BOT_COUNT_MAX; ++i)
		alive += bots[i].alive;
	return alive;
}

// Each game step, the turn to think (game/bot.h: may_think) passes to the next live bot: with 3
// alive each thinks every 3rd step, with 1 every step. Danger ahead still makes any bot think at
// once. Thinking is the costly part: 3 bots at once among long trails took 45% of a frame
static void share_thinking(void)
{
	static int thinker = 0; // whose turn it was last

	for (int tries = 0; tries < BOT_COUNT_MAX; ++tries)
	{
		thinker = thinker + 1 < BOT_COUNT_MAX ? thinker + 1 : 0;
		if (bots[thinker].alive)
			break;
	}
	for (int i = 0; i < BOT_COUNT_MAX; ++i)
		bot_minds[i].may_think = (unsigned char)(i == thinker);
}

// Every bot out of the field: dead, its trail already sunk away (rider_remove)
static void remove_bots(void)
{
	for (int i = 0; i < BOT_COUNT_MAX; ++i)
		rider_remove(&bots[i]);
}

static void begin_round(void); // below, with the rounds
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
#define ATTRACT_LINE_Y (ATTRACT_ORIGIN_Y - 5 * ATTRACT_TEXT_UNIT) // the message's centre line
#define ATTRACT_STROKES_MAX 24 // the message has 21
#define ATTRACT_WORDS_MAX 4	   // and 3 words

static attract_waypoint *attract_waypoints = JO_NULL; // ATTRACT_TEXT_MAX_WAYPOINTS: the arena's memory
static int attract_waypoint_count;
static attract_route route;
trail *attract_strokes = JO_NULL; // ATTRACT_STROKES_MAX: the finished strokes, standing or sinking
unsigned char attract_stroke_word[ATTRACT_STROKES_MAX]; // the word each belongs to
int attract_stroke_count;
unsigned char attract_word_height[ATTRACT_WORDS_MAX]; // how tall each word's strokes stand
static unsigned char word_sinking[ATTRACT_WORDS_MAX];
static int current_word;
static int message_frames; // since this message began: where the spectator's steady pan is

// The spectator camera's steady point along the message (view.c): from where the message
// starts, moving east at the message's own average speed (attract_text.h), so the cycle wanders
// either side of it as it goes back and forth in the letters, but always keeps up
static void update_pan(void)
{
	view_spectate_pan(attract_route_start(&route).x - ((message_frames * ATTRACT_TEXT_PAN_X256) >> 8) +
					  ATTRACT_TEXT_PAN_PHASE);
}

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
	message_frames = 0;
	update_pan();
	for (int w = 0; w < ATTRACT_WORDS_MAX; ++w)
	{
		attract_word_height[w] = TRAIL_HEIGHT;
		word_sinking[w] = 0;
	}
}

// Spelt words sink a unit every ATTRACT_SINK_FRAMES frames: slower than a dead rider's trail
// (the mask is a remainder with no division, ATTRACT_SINK_FRAMES being a power of two)
static void sink_words(void)
{
	for (int w = 0; w < ATTRACT_WORDS_MAX && (message_frames & (ATTRACT_SINK_FRAMES - 1)) == 0; ++w)
		if (word_sinking[w] && attract_word_height[w] > 0)
			attract_word_height[w] = (unsigned char)(attract_word_height[w] > RIDER_SINK_SPEED
														 ? attract_word_height[w] - RIDER_SINK_SPEED
														 : 0);
}

// One frame of the loop, before the cycle moves: the turn to press and whether to boost, and
// the strokes and words as each waypoint is reached
static void attract_frame(rider_controls *controls)
{
	int turn;
	const attract_waypoint *here = attract_route_step(&route, &player1.bike, &turn);

	sink_words();
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
	// Boosting all the way, the gauge kept full: a full one only lasts 1.5 s (cycle.h), and the
	// loop's cycle is showing off, not racing
	controls->boost = 1;
	player1.bike.gauge = CYCLE_GAUGE_MAX;
	message_frames++;
	update_pan();
}

/*
 * The round's arena (game/rounds.h: rounds_layout): which blocks stand inside the square. Set
 * whenever a round starts afresh: before its walls rise, and at a restart. The last round closes
 * in (rounds_closing), pulling the walls in (arena_set_inset); setting the next round's layout
 * puts them back, so a restart or the loop going round opens it out again
 */
static void enter_round_layout(void)
{
	arena_set_inset(0);
	arena_set_layout(rounds_layout(&arena_rounds));
	wall_flicker_set_round(&arena_flicker, arena_rounds.number); // somewhere else each round
}

// Each game frame: the last round's walls lunging in (rounds.h), the camera rumbling while they
// move. A wall coming in over a cycle pulls it inside (cycle.c): a crash. Won, it stops closing
// but the walls stay where they got to until they're down (enter_round_layout)
static void close_in(void)
{
	const int closing = rounds_closing(&arena_rounds);
	const int rumble = rounds_rumble(&arena_rounds);

	if (closing > 0)
		arena_set_inset(closing);
	if (rumble > 0)
		view_rumble(rumble); // kept up each frame: it only dies away once they stop
}

// Spectating (the prologue's loop) or playing. In the loop the CPU is alone: the bot is taken
// out of the field, and comes in fresh at its start when play begins (docs/STORY.md)
static void set_spectating(bool on)
{
	view_intro_stop(); // a flight in is started afresh by the caller (enter_scene, hit_the_loop)
	view_arrival_stop();
	spectating = on;
	player1.bike.bounded = !on; // a ghost on the route; solid again when play begins
	attract_stroke_count = 0;
	if (on)
	{
		world_point origin;

		remove_bots();
		loop_break_init(&breaker);
		rounds_init(&arena_rounds); // the loop: play will start from the first round (BOOT_ROUND)
		arena_rounds.number = BOOT_ROUND;
		enter_round_layout(); // flat in the floor all through the loop: it rises at the break
		if (attract_waypoint_count == 0) // built once
		{
			origin.x = ATTRACT_ORIGIN_X;
			origin.y = ATTRACT_ORIGIN_Y;
			attract_waypoint_count = attract_text_build(ATTRACT_TEXT, origin, attract_waypoints, ATTRACT_TEXT_MAX_WAYPOINTS);
		}
		view_spectate_line(ATTRACT_LINE_Y);
		start_message();
	}
	else
	{
		// The round you're on, from its start: the first after the loop's break, the same one
		// again after a crash (the terminal screen), with its bots placed round you
		rounds_restart(&arena_rounds);
		begin_round();
	}
	update_solid_trails();
}

/*
 * Breaking the loop (game/loop_break.h, docs/STORY.md): while spectating, a direction pressed is
 * a hit on the loop: a burst of light and a flash (main.c), a synth stab and a jolt of the
 * camera, harder each time. The third breaks it: under its flash the world slides so the cycle
 * that was writing is at the arena's centre (slide_to_the_centre), it turns to face north, and
 * the camera flies down out of the stands into it in 3 s (game/arrival_flight.h) while the
 * arena's walls rise round it and the message sinks away. Everything waits for the camera; then
 * it's the player's: the same cycle you watched, not a new one.
 */
loop_break breaker;
world_point loop_break_at; // where the cycle was when the loop broke: the last burst is there
static const short hit_shake[LOOP_BREAK_PRESSES + 1] = {0, 364, 637, 910}; // 2, 3.5 and 5 degrees
#define DIRECTIONS (DIGI_LEFT | DIGI_RIGHT | DIGI_UP | DIGI_DOWN)

// The slide is by whole tiles of the spectator's floor (view.c's SPECTATE_FLOOR_SHIFT: 125 x 8),
// so the grid under the camera lands on itself and the slide can't be seen. The cycle ends up
// within half a tile of the centre
#define SLIDE_STEP 1000

// v to the nearest multiple of SLIDE_STEP. A division, but once, at the break
static int nearest_step(int v)
{
	return (v >= 0 ? v + SLIDE_STEP / 2 : v - SLIDE_STEP / 2) / SLIDE_STEP * SLIDE_STEP;
}

// How far to slide the world, in whole steps, on one axis, to bring v within half a step of to:
// the layout's start (the middle isn't clear in every layout: the donut's block stands there)
static int slide_towards(int v, int to)
{
	return -nearest_step(v - to);
}

// The arena is centred on the world's origin, and the flight down is made for a cycle there
// (it frames the whole arena from the overview, and stays well clear of the walls when low).
// The loop's cycle could be anywhere along the message, so rather than move the arena, move
// everything else: the cycle, the message's strokes and the camera, all by the same amount
static void slide_to_the_centre(void)
{
	const world_point to = arena_start(); // the centre, unless BOOT_ROUND's layout has a block there
	int dx = slide_towards(player1.bike.position.x, to.x);
	int dy = slide_towards(player1.bike.position.y, to.y);

	// Mid-stroke, keep the stroke being written: it sinks with the rest of its word
	if (attract_route_pen(&route) && attract_stroke_count < ATTRACT_STROKES_MAX && current_word < ATTRACT_WORDS_MAX)
	{
		trail_set_head(&player1.wall, player1.bike.position);
		attract_strokes[attract_stroke_count] = player1.wall;
		attract_stroke_word[attract_stroke_count++] = (unsigned char)current_word;
	}
	for (int i = 0; i < attract_stroke_count; ++i)
		trail_shift(&attract_strokes[i], dx, dy);
	player1.bike.position.x = (short)(player1.bike.position.x + dx);
	player1.bike.position.y = (short)(player1.bike.position.y + dy);
	view_spectate_move(dx, dy);
}

// A direction pressed while spectating. Returns the presses, changed: the one that breaks the
// loop becomes a turn (it's the first thing the free cycle does); the others are spent on the hit
static unsigned short hit_the_loop(unsigned short struck)
{
	int press = loop_break_press(&breaker);

	sound_play_hit(press);
	view_shake(hit_shake[press]);
	if (press < LOOP_BREAK_PRESSES)
		return (unsigned short)(struck & ~DIRECTIONS);
	// Broken: play from where the cycle is, in 1st person, the bot coming in, once the camera's down
	slide_to_the_centre();
	loop_break_at = player1.bike.position;
	// The same cycle, where it was, but turned north (the way the flight down lands) and with no
	// trail: its stroke is kept above. From the stands, under the flash, the turn can't be seen.
	// Turned before play begins, as the round's bots are placed by the way it faces
	{
		world_point at = player1.bike.position;

		rider_init(&player1, at.x, at.y, HEADING_NORTH);
	}
	{
		int strokes = attract_stroke_count; // the message is left standing, to sink (game_step)

		set_spectating(false);
		attract_stroke_count = strokes;
	}
	for (int w = 0; w < ATTRACT_WORDS_MAX; ++w)
		word_sinking[w] = 1; // the word being written too
	first_person = true;
	view_arrival_start(&player1.bike);
	view_init(&player1.bike);
	return (unsigned short)(struck & ~DIRECTIONS); // spent on the hit: it starts riding straight
}

bool debug_camera = false; // START: freeze the cycle and fly the camera around
bool debug_slow_motion = false;
bool debug_no_termination = false; // debug camera Z: a crash only stops the cycle

// Debug: fills a trail with a full-length serpentine across the arena, the worst case for
// drawing, its rows `offset` north of the player's (so several can lie side by side). Its last
// corner is lined up with the rider's cycle, so the live end (corner to cycle) runs straight, as
// in play: a diagonal one is cut into pieces with two divisions each (trail_pieces.c: point_at),
// a cost no real trail has, which made this test far slower than play
static void stress_fill_one(rider *r, int offset)
{
	world_point p;
	const short edge = (short)(arena_player_limit() - 50);
	const short row_spacing = (2 * edge) / (TRAIL_MAX_CORNERS / 2);
	const world_point at = r->bike.position;

	p.x = -edge;
	p.y = (short)(-edge - offset);
	trail_reset(&r->wall, p);
	for (int i = 1; i < TRAIL_MAX_CORNERS - 1; ++i)
	{
		if (i & 1)
			p.x = -p.x;
		else
			p.y += row_spacing;
		trail_set_head(&r->wall, p);
		trail_add_corner(&r->wall);
	}
	// Riding north or south, the live end runs along x = the cycle's; east or west, along its y
	if (WORLD_HEADING_DX(r->bike.heading) == 0)
		p.x = at.x;
	else
		p.y = at.y;
	trail_set_head(&r->wall, p);
	trail_add_corner(&r->wall);
	trail_set_head(&r->wall, at);
}

// The player's trail, and every live bot's beside it, 120 apart: long trails side by side near
// you, as riding along a bot's trail late in a round (the frame rate fell to ~35 there)
static void stress_fill_trail(void)
{
	stress_fill_one(&player1, 0);
	for (int i = 0; i < BOT_COUNT_MAX; ++i)
		if (bots[i].alive)
			stress_fill_one(&bots[i], 120 * (i + 1));
}

/*
 * The parts of the game, in the order they're played:
 *   SCENE_INTRO  the camera flies down to the floor and in to the spectator's seat (view.c),
 *                while the CPU starts writing its message
 *   SCENE_LOOP   the spectator's view of the loop, until three presses break it
 *   SCENE_PLAY   play, in 1st person, the bot in the arena
 * The game boots into BOOT_SCENE. To work on a later part, set it to that part and build;
 * or, while it runs, jump: START (the debug camera), then A for the next part, START again.
 * For now it boots straight into play, the prologue left out (it's the game's opening, its
 * "menu"): set SCENE_INTRO to have it back. The E2E scripts find their way from whichever it
 * boots into (tests/e2e/run.sh reads this line: skip_prologue, start_intro)
 */
typedef enum
{
	SCENE_INTRO,
	SCENE_LOOP,
	SCENE_PLAY,
	SCENE_COUNT
} scene;
#define BOOT_SCENE SCENE_PLAY

/*
 * During the opening flight you're only watching: the loop can't be hit until the camera has
 * landed in the spectator's seat. Any button instead hurries the flight along, once: it runs at
 * INTRO_HURRY_SPEED from then, the whole game stepping that many times a frame, so the cycle
 * writes faster too and the flight lands where it always does. Further presses do nothing. Not
 * START (the debug camera, which the E2E scripts use to jump past it) nor Z (the slow motion,
 * to film it)
 */
#define INTRO_HURRY_SPEED 3
#define INTRO_HURRY_BUTTONS (DIGI_LEFT | DIGI_RIGHT | DIGI_UP | DIGI_DOWN | DIGI_A | DIGI_B | DIGI_C | DIGI_X | DIGI_Y | DIGI_L | DIGI_R)
int intro_hurry = 1; // the flight's speed: 1, or INTRO_HURRY_SPEED once hurried (main.c shows it)

/*
 * Each part of the game's memory (scene_memory.h): the arena's (the trails' meshes, main.c, and
 * the prologue's message, here) and Work RAM's (the corridor, here, and its meshes, level.c).
 * jo_malloc'd and zeroed, as a static array would be
 */
static void *taken(int size)
{
	void *memory = jo_malloc(size);

	if (memory == JO_NULL)
		jo_core_error("No memory for the next part");
	else
		jo_memset(memory, 0, size);
	return memory;
}

static void arena_memory_take(void)
{
	if (attract_strokes == JO_NULL)
	{
		attract_strokes = (trail *)taken(sizeof(trail) * ATTRACT_STROKES_MAX);
		attract_waypoints = (attract_waypoint *)taken(sizeof(attract_waypoint) * ATTRACT_TEXT_MAX_WAYPOINTS);
	}
	arena_trail_meshes_take();
}

static void arena_memory_release(void)
{
	if (attract_strokes != JO_NULL)
	{
		jo_free(attract_strokes);
		jo_free(attract_waypoints);
		attract_strokes = JO_NULL;
		attract_waypoints = JO_NULL;
	}
	attract_stroke_count = 0;
	attract_waypoint_count = 0;
	arena_trail_meshes_release();
}

static void corridor_memory_take(void)
{
	if (arena_corridor == JO_NULL)
	{
		arena_corridor = (corridor *)taken(sizeof(corridor));
		if (arena_corridor != JO_NULL)
			corridor_init(arena_corridor);
	}
	corridor_meshes_take();
}

static void corridor_memory_release(void)
{
	if (arena_corridor != JO_NULL)
	{
		jo_free(arena_corridor);
		arena_corridor = JO_NULL;
	}
	corridor_meshes_release();
}

static scene current_scene(void)
{
	if (!spectating)
		return SCENE_PLAY;
	return view_intro_flying() ? SCENE_INTRO : SCENE_LOOP;
}

// A part of the game from its start: the terminal screen gone, the riders at their starts
// (the player's put on the message's route if it's the CPU's turn), and the camera set for it.
// In the debug camera the camera's left alone: it takes effect on leaving (view_init there)
static void enter_scene(scene part)
{
	game_init(&game);
	breach_init(&arena_breach);
	// The arena's memory back in place of Work RAM's (scene_memory.h): after a breach, or at boot
	corridor_memory_release();
	arena_memory_take();
	status_init(&status);
	if (part == SCENE_PLAY)
	{
		// The round's layout first: where you start depends on it (the middle isn't always clear)
		world_point at;

		enter_round_layout();
		at = arena_start();
		rider_init(&player1, at.x, at.y, (unsigned char)arena_start_heading());
	}
	else
		rider_init(&player1, PLAYER1_START_X, PLAYER1_START_Y, HEADING_NORTH);
	set_spectating(part != SCENE_PLAY); // the bots: out for the loop, or the round's in for play
	intro_hurry = 1;
	if (part == SCENE_INTRO)
		view_intro_start(&player1.bike);
	if (!debug_camera)
		view_init(&player1.bike);
#ifdef PERF_STRESS_TRAIL
	if (part == SCENE_PLAY)
		stress_fill_trail();
#endif
}

int arena_wall_rise(void)
{
	int arriving = view_wall_rise(), between = rounds_wall_rise(&arena_rounds);

	return arriving < between ? arriving : between;
}

/*
 * Between rounds (game/rounds.h, docs/STORY.md). Once the last rival's gone the walls sink, and
 * every trail with them; you ride on over the endless floor, a ghost, as there's nothing to hit.
 * Just before the next round's walls rise, with nothing standing anywhere, the world slides so
 * you're near its layout's start (the same slide as at the loop's break: you may have ridden a
 * long way out); then the walls and its blocks rise round you, and once they're up its rivals
 * come in.
 */

// Spawn points are kept SPAWN_MARGIN inside the arena's walls and clear of its blocks
// (arena_clear_spot): in a small arena (the debug sizes) 2000 out could be outside the walls,
// and in the donut 3000 ahead can be inside its block, an instant crash
#define SPAWN_MARGIN 500

// Where each bot starts, from you: how far to your right (negative: left) and ahead, riding the
// way you are. The first is where round 1's bot has always been (2000 to your right); the
// others close you in from the left and ahead
static const short bot_start_right[BOT_COUNT_MAX] = {2000, -2000, 0};
static const short bot_start_ahead[BOT_COUNT_MAX] = {0, 0, 3000};

// The round's bots (rounds_rivals: 1 to 3) placed round you, the rest out of the field, and the
// status line saying which round it is. You're near the layout's start whenever this is called
// (play's start, or the walls just risen round you), so they're well inside the walls too
static void begin_round(void)
{
	int h = player1.bike.heading, right = (h + 1) & 3;
	int count = rounds_rivals(&arena_rounds);

	remove_bots();
	for (int i = 0; i < count && i < BOT_COUNT_MAX; ++i)
	{
		world_point at;

		at.x = (short)(player1.bike.position.x + WORLD_HEADING_DX(right) * bot_start_right[i] +
					   WORLD_HEADING_DX(h) * bot_start_ahead[i]);
		at.y = (short)(player1.bike.position.y + WORLD_HEADING_DY(right) * bot_start_right[i] +
					   WORLD_HEADING_DY(h) * bot_start_ahead[i]);
		at = arena_clear_spot(at, SPAWN_MARGIN);
		rider_init(&bots[i], at.x, at.y, (unsigned char)h);
		// The seed picks its reaction times: the same every run, but different for each bot and
		// round (round 1's first bot keeps seed 1, which the E2E scripts were made with)
		bot_init(&bot_minds[i], 1 + arena_rounds.number * BOT_COUNT_MAX + i);
		bot_set_skill(&bot_minds[i], bot_skill_now());
	}
	update_solid_trails();
	status_show(&status, rounds_title(&arena_rounds));
}

static void between_rounds(void)
{
	switch (rounds_update(&arena_rounds, bots_alive()))
	{
	case ROUND_CLEARED:
		player1.bike.bounded = 0; // nothing left to hit, and soon no edge
		break;
	case ROUND_RISES:
	{
		// The next round's layout rises: you're slid near its start. Whole floor tiles, so the
		// grid under you lands on itself. Your trail starts afresh here: what you laid out there
		// is flat in the floor, and would be left far off
		world_point to;
		int dx, dy;

		enter_round_layout();
		to = arena_start();
		dx = slide_towards(player1.bike.position.x, to.x);
		dy = slide_towards(player1.bike.position.y, to.y);

		player1.bike.position.x = (short)(player1.bike.position.x + dx);
		player1.bike.position.y = (short)(player1.bike.position.y + dy);
		view_spectate_move(dx, dy); // the camera with you (in play it follows you anyway)
		trail_reset(&player1.wall, player1.bike.position);
		remove_bots(); // their trails have sunk by now; gone for good
		break;
	}
	case ROUND_BEGINS:
		player1.bike.bounded = 1;
		player1.trail_height = TRAIL_HEIGHT;
		begin_round();
		break;
	}
	close_in();
	wall_flicker_step(&arena_flicker);
	// Your trail sinks and rises with the walls
	if (arena_rounds.phase != ROUND_PLAYING)
		player1.trail_height = (unsigned char)((TRAIL_HEIGHT * rounds_wall_rise(&arena_rounds)) >> 8);
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
	// A: jump to the next part of the game (intro, loop, play, then the intro again), taking
	// effect on leaving the debug camera
	if (struck & DIGI_A)
		enter_scene((scene)((current_scene() + 1) % SCENE_COUNT));

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
		view_rx += JO_DEG_TO_ANGLE(0.5);
	else if (jo_is_pad1_key_pressed(JO_KEY_C))
		view_rx -= JO_DEG_TO_ANGLE(0.5);
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
 * The designed crash (game/breach.h, docs/STORY.md). Round 5's flicker gives way: ride into it
 * and, instead of the terminal screen, the Watchdog's reset fails. Everyone else is frozen
 * where they were while the screen tears (main.c draws the glitches). Then the section's a hole
 * with a corridor behind it (game/corridor.h): you ride on through, laying no trail (it ends at
 * the wall, where the arena's rules stopped holding), out over the grid tailing off into dark
 * glass. The cycle goes down, skids, and crashes to a stop, and lies there: where Work RAM
 * begins. A button gets you up, and from then on you're on foot (game/walker.h).
 */

// On foot: you, and the floor you can stand on (the corridor's, for now)
static walker player_feet;
static walker_area feet_area;

// Getting up: stood where the cycle came to rest, facing the way it was going, on the corridor's
// floor and the grey box's roofs (corridor_walker_area)
static void stand_up(void)
{
	walker_init(&player_feet, player1.bike.position, player1.bike.heading * 0x4000);
	corridor_walker_area(arena_corridor, &feet_area);
}

// One frame on foot: the pad (game/walker.h has the scheme) moves you, and the camera's yours
static void walk_step(void)
{
	const unsigned short held = press_latch_held(&pad1_presses);
	walker_controls controls;
	// Both of a pair held at once cancel out: neither way
	const int left = (held & DIGI_LEFT) != 0, right = (held & DIGI_RIGHT) != 0;
	const int up = (held & DIGI_UP) != 0, down = (held & DIGI_DOWN) != 0;
	const int l = (held & DIGI_L) != 0, r = (held & DIGI_R) != 0;

	controls.turn = (signed char)(right - left);
	controls.move = (signed char)(up - down);
	controls.strafe = (signed char)(r - l);
	walker_look_from_buttons((held & DIGI_A) != 0, (held & DIGI_B) != 0, (held & DIGI_C) != 0, (held & DIGI_X) != 0,
							 (held & DIGI_Y) != 0, (held & DIGI_Z) != 0, &controls.look_x, &controls.look_y);
	walker_update(&player_feet, controls, &feet_area);
	// The cycle's position stands in for yours: the camera, culling and the walls all go by it.
	// Its heading is the quarter turn nearest your facing, for anything that only knows those
	player1.bike.position = walker_position(&player_feet);
	player1.bike.heading = (unsigned char)(((player_feet.yaw + 0x2000) >> 14) & 3);
	// The eye: down for the step and a landing's dip, and still down a stair (or a vault) your feet
	// are already up (walker_step_lag); up with the roof you're on and with standing up (the
	// view's eye is the cycle's, 35 up: walker_eye's above it)
	view_on_foot(player_feet.yaw, controls.look_x, controls.look_y,
				 walker_bob(&player_feet) + walker_dip(&player_feet) + walker_step_lag(&player_feet) -
					 walker_height(&player_feet) - (walker_eye(&player_feet) - WALKER_EYE_RIDING));
}

// Whether the crash this step was into the flicker where it gives way: then it's the breach
static bool crashed_through_the_flicker(void)
{
	return player1.crashed && arena_flicker.gives_way && arena_rounds.phase == ROUND_PLAYING &&
		   wall_flicker_hit(&arena_flicker, player1.bike.position, player1.bike.heading, arena_player_limit());
}

// struck: this step's presses. Any (but START: the debug camera's) gets you up once you're lying still
/*
 * The hidden load (2026-11-02): as the reset loses and the hole opens (the tear's end), the screen
 * freezes on the hole's first frame for a moment, as if the machine had hung, and what's next is
 * loaded from the disc while nobody can tell: the glass behind you (level.c), and where you can
 * stand is worked out (corridor_walker_area). Later, Work RAM's city too: its tables and textures,
 * in place of the arena's.
 * Frozen by not giving SGL a new frame: the game step waits here, so main.c doesn't draw, and
 * VDP1 goes on showing the last picture it drew (VDP2 its layers). Held at least
 * HIDDEN_LOAD_VBLANKS on top of the load: a hang, not a stutter. The waiting counts vblanks off
 * VDP2's TVSTAT (bit 3: in the vblank), as jo's core does. The frame after it runs at most
 * FRAME_STEPS_MAX steps, not the half second's worth
 */
#define HIDDEN_LOAD_VBLANKS 30
static bool hidden_loaded = false; // this breach's: back to false as the next starts
// What the hidden load does, frozen or not (boot_on_foot doesn't freeze)
static void hidden_load_work(void)
{
	// The arena's left behind: its memory goes back to jo's heap (scene_memory.h), for Work RAM's
	arena_memory_release();
	load_corridor_glass();
	corridor_walker_area(arena_corridor, &feet_area);
}

static void hidden_load(void)
{
	hidden_load_work();
	for (int i = 0; i < HIDDEN_LOAD_VBLANKS; ++i)
	{
		while ((JO_VDP2_TVSTAT & 8) == 8)
			; // out of the last one
		while ((JO_VDP2_TVSTAT & 8) == 0)
			; // into the next
	}
}

static void breach_step(unsigned short struck)
{
	const int h = player1.bike.heading;
	const int distance = breach_distance(&arena_breach, cycle_cruise_speed());

	if ((struck & (unsigned short)~DIGI_START) != 0)
		breach_get_up(&arena_breach);
	if (arena_breach.phase == BREACH_ON_FOOT)
	{
		const int walker_frc = jo_time_get_frc(); // the debug line's "wk" (utils/perf.h)

		walk_step();
		perf_part_add(PERF_WALKER, walker_frc);
		view_follow(&player1.bike);
		return;
	}
	// The hidden load, once the hole's open and its first frame's been drawn: frozen on that, the
	// world. On the tear's last frame instead, it froze on one of the terminal screen's flashes:
	// an empty terminal, the white floor and a cursor
	if (arena_breach.phase == BREACH_THROUGH && !hidden_loaded)
	{
		hidden_load();
		hidden_loaded = true;
	}
	breach_update(&arena_breach);
	if (arena_breach.phase == BREACH_ON_FOOT)
		stand_up(); // up this frame: the pad's yours from the next
	wall_flicker_open(&arena_flicker); // the section's gone: nothing puts a piece back now
	// Straight on through the hole: nothing to hit out there. The cycle's moved here, not by
	// cycle_update, so its trail isn't stretched after it
	player1.bike.position.x = (short)(player1.bike.position.x + WORLD_HEADING_DX(h) * distance);
	player1.bike.position.y = (short)(player1.bike.position.y + WORLD_HEADING_DY(h) * distance);
	// Going down: a jolt as it hits the glass, then it rumbles as it skids, dying with the motion
	if (breach_landed(&arena_breach))
		view_shake(CAMERA_SHAKE_BREACH);
	if (breach_rumble(&arena_breach) > 0)
		view_rumble(breach_rumble(&arena_breach));
	view_breach(breach_roll(&arena_breach), breach_dip(&arena_breach), breach_drop(&arena_breach));
	view_follow(&player1.bike);
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
			enter_scene(SCENE_PLAY); // straight back to play: the prologue is only at boot
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
	// The designed crash takes over from the pad (but the debug camera's START, above, so the
	// corridor can be looked round, and A in it back to the game's start), and once you're up, the
	// pad's yours again, on foot
	if (arena_breach.phase != BREACH_NONE)
	{
		breach_step(struck);
		return;
	}

	if (!jo_is_pad1_available())
		return;

	// The opening flight: the first press hurries it; nothing else acts on it (but Z, below)
	if (spectating && view_intro_flying() && (struck & INTRO_HURRY_BUTTONS))
	{
		intro_hurry = INTRO_HURRY_SPEED;
		struck &= (unsigned short)~INTRO_HURRY_BUTTONS;
	}
	loop_break_update(&breaker);
	// The camera coming down into the cycle: everyone waits for it, and the pad does nothing.
	// Only the message sinks away (gone before it lands, as nobody rides through it)
	if (view_arriving())
	{
		message_frames++;
		sink_words();
		view_follow(&player1.bike);
		if (!view_arriving())
			attract_stroke_count = 0; // landed: it's sunk
		return;
	}
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
	// Round 5, into the flicker: not a crash, the breach. The jolt of the hit at once
	if (crashed_through_the_flicker())
	{
		player1.crashed = 0;
		breach_start(&arena_breach);
		hidden_loaded = false;
		corridor_memory_take(); // Work RAM's memory, while the arena's is still in use (scene_memory.h)
		corridor_open(arena_corridor, &arena_flicker, arena_half());
		view_shake(CAMERA_SHAKE_BREACH);
		return; // the world stops here: from the next step it's the breach's (breach_step)
	}
	// A crash terminates the player's process: the terminal screen from the next frame
	game_update(&game, player1.crashed && !debug_no_termination, 0);

	// The loop: between strokes (pen up) it lays no trail: just a point that follows it
	if (spectating && !attract_route_pen(&route))
		trail_reset(&player1.wall, player1.bike.position);

	// Each bot decides its buttons from what it can see, then rides by the same rules. A crash
	// terminates the bot's process: it stops, and its trail sinks into the floor (and from now on
	// stops nobody)
	int bots_frc = jo_time_get_frc(); // the debug line's "bots" figure (utils/perf.h)

	share_thinking();
	for (int i = 0; i < BOT_COUNT_MAX; ++i)
	{
		rider *bot = &bots[i];

		if (!bot->alive)
		{
			rider_sink(bot);
			continue;
		}
		rider_step(bot, bot_think(&bot_minds[i], bot, bot_rivals, 1, solid_trails, solid_trail_count), solid_trails,
				   solid_trail_count);
		if (bot->crashed)
		{
			// Whatever it hit (a trail, the arena edge), make it felt and say so: a jolt of the
			// camera, and the status line. It may have been a long way off, or behind you
			view_shake(CAMERA_SHAKE_BOT_KILL);
			status_show(&status, STATUS_ENEMY_ELIMINATED);
			rider_kill(bot);
			update_solid_trails(); // from the next bot on, its trail stops nobody
		}
	}
	perf_part_add(PERF_BOTS, bots_frc);
	if (!spectating)
		between_rounds();
	status_update(&status); // the status line types on, and goes once it's been read

	// A: the bots' difficulty, Easy, Medium, Hard and round again (a toggle for now: it may be
	// chosen in a menu later). The bots riding change at once, and the status line says which
	if (struck & DIGI_A)
	{
		bot_skill_level = bot_skill_level + 1 < BOT_SKILL_COUNT ? bot_skill_level + 1 : BOT_EASY;
		for (int i = 0; i < BOT_COUNT_MAX; ++i)
			bot_set_skill(&bot_minds[i], bot_skill_now());
		status_show(&status, skill_titles[bot_skill_now()]); // what they play at, not just what was picked
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

	// B (debug): every bot riding is terminated, as if it had crashed: the jolt, the status line,
	// its trail sinking. With none left the round's won, so it skips through the rounds to the
	// one being worked on
	if ((struck & DIGI_B) && bots_alive() > 0)
	{
		for (int i = 0; i < BOT_COUNT_MAX; ++i)
			if (bots[i].alive)
				rider_kill(&bots[i]);
		view_shake(CAMERA_SHAKE_BOT_KILL);
		status_show(&status, STATUS_ENEMY_ELIMINATED);
		update_solid_trails(); // their trails stop nobody from now on
	}
	// Tuning, to find the balance of speed and scale by feel. The debug text shows both.
	// (The arena's size is in the debug camera: START, then X / Y.) C: cruise speed up (B turned it
	// down, before it terminated the bots)
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
	// Hurried, the opening flight takes 3 times the steps; back to normal once it lands
	for (int i = 0; i < steps * (view_intro_flying() ? intro_hurry : 1); ++i)
	{
		if (i >= steps && !view_intro_flying())
			break;
		game_step(i == 0 ? struck : 0);
	}
}

/*
 * Booting on foot (BOOT_ON_FOOT): round 5 entered as usual, then everything the designed crash
 * leaves behind, at once: the corridor open behind the flicker, the hidden load's swap of memory
 * (no freeze: nothing's on the screen yet), and you stood on the corridor's floor where the cycle
 * comes to rest, facing out along it, stood up already
 */
#define BOOT_ON_FOOT_OUT 1300 // where the cycle comes to rest: out from the hole
static void boot_on_foot(void)
{
	corridor_memory_take();
	corridor_open(arena_corridor, &arena_flicker, arena_half());
	wall_flicker_open(&arena_flicker);
	breach_start(&arena_breach);
	arena_breach.phase = BREACH_ON_FOOT;
	hidden_loaded = true;
	hidden_load_work();
	// On the left half, where the stairs are: a quarter of the way across. The headings are
	// numbered as the sides (wall_flicker.h): the side's heading is the way out
	player1.bike.position = corridor_point(arena_corridor, BOOT_ON_FOOT_OUT,
										   arena_corridor->across_min + ((arena_corridor->across_max - arena_corridor->across_min) >> 2));
	player1.bike.heading = arena_corridor->side;
	stand_up();
	player_feet.eye = WALKER_EYE; // up already
	// The round's title, typing on as round 5's entered: the breach stops the status line, so it
	// would stay, cursor and all
	status_init(&status);
}

void init_players(void)
{
	rounds_init(&arena_rounds); // booting straight into play (BOOT_SCENE) skips the loop's
	arena_rounds.number = BOOT_ON_FOOT ? ROUND_COUNT - 1 : BOOT_ROUND;
	enter_scene(BOOT_ON_FOOT ? SCENE_PLAY : BOOT_SCENE);
	if (BOOT_ON_FOOT)
		boot_on_foot();
}
