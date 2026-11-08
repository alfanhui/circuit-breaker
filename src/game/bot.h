#ifndef __BOT_H__
#define __BOT_H__

#include "rider.h"

/*
 * A computer rider's brain. Pure C, unit-tested on the PC.
 *
 * A bot never moves itself: each frame it decides which buttons to press (rider_controls), and
 * its rider is then driven exactly like a player's (rider.h). So it plays by the same rules.
 *
 * It only knows what it could see: it asks the same collision code the cycles use
 * (collision.h) how far it could go each way, and it sees other riders within BOT_SIGHT in the
 * half in front of it. Behind it, it believes they carried on the way it last saw them going
 * (so a turn behind its back changes nothing until it sees them again). Further off it knows
 * only which way they are, as the player's radar shows the bots. Nothing else: not the
 * player's buttons, not the whole map.
 *
 * How it chooses: utility scoring. Every few frames it scores its three options, straight on,
 * left and right, and picks the best:
 *
 *   score = room x BOT_WEIGHT_ROOM        how far it could go that way (up to its side look)
 *         + approach x BOT_WEIGHT_CHASE   how much that way closes on where it aims to cut off
 *                                         the nearest rival it can see (below)
 *         - a turn cost                   turning has to be worth it: no twitching
 *
 * But safety comes first: a way that would hit something within its look-ahead (BOT_LOOK_FRAMES)
 * isn't scored at all while there's a safe one. Only when every way is dangerous does it pick
 * the one with the most room.
 *
 * Each part is a distance in world units, so weights are plain whole numbers and the sums are
 * a few multiplies and adds: no division. Changing the weights changes its character (step 5:
 * personalities). It boosts when it has picked straight on with the way clear, and either where it
 * aims is ahead (racing a rival, or crossing their line) or it's squeezed by something close
 * beside it: from a full gauge, letting go when it runs dry, as a player would.
 *
 * It reacts like a person, not a machine. A person takes about 200 ms (12 frames) to react, and
 * not exactly the same time every time. So once it has picked a turn, it presses it a reaction
 * time later, picked at random between BOT_REACTION_MIN and BOT_REACTION_MAX frames: it acts
 * on what it saw a moment ago.
 *
 * Steps 1 to 3 of the plan (issue #37).
 */

// How far it looks, in frames of travel at cruise speed, so it behaves the same at any cruise
// speed (210 units at cruise 7 for 30 frames). Anything closer than BOT_LOOK_FRAMES ahead is
// danger: it must be well beyond the slowest reaction, or it would notice too late to turn.
// Room is judged further out, BOT_SIDE_LOOK_FRAMES, to see which way opens up
#define BOT_LOOK_FRAMES 30
#define BOT_SIDE_LOOK_FRAMES 120

// Rivals it can see: within this many units across plus along (|dx| + |dy|, no square root).
// About what's drawn round a player; a person sees as far, if only ahead. Beyond it, it only
// knows which way they are, and heads that way (as the player's radar shows the bots)
#define BOT_SIGHT 4096

// Out of sight behind it, it believes a rival carried on straight at cruise speed for up to this
// many frames (2 s); after that it believes they're about there. An unsigned char, so at most 255
#define BOT_MEMORY_FRAMES 120

// A way that's blocked within its side look needs a way out near the end, at least this many
// frames of travel to one side, or it's a dead end: as bad as a wall right ahead
#define BOT_WAY_OUT_FRAMES 60
/*
 * Cutting a rival off. Aiming at a point just ahead of them was pursuit: the bot drew level and
 * rode alongside, or zigzagged towards them, but never got across. A cut-off is two moves:
 *   - race: get ahead of them, aiming far along their path (and boosting for it)
 *   - cut: once it's further ahead of them than it is across from their line, turn across
 *     their path. Everyone moves at right angles at the same speed, so then it reaches their
 *     line first. A margin of BOT_CUT_MARGIN_FRAMES (more than its slowest reaction) makes sure
 *     it's in time. Once something blocks their way before that point, they're cut off: it
 *     stops aiming at them (and doesn't weave back and forth across their line).
 * Ahead minus across only changes when someone boosts or turns: riding the same way, or closing
 * in sideways, leaves it the same. So against a rival riding straight it can only get across by
 * out-boosting them (slow); its chances come when they turn.
 * "Ahead" and "across" are measured along the rival's heading: a subtraction and a sign each.
 */
#define BOT_RACE_FRAMES 120	   // racing, it aims 2 s of travel ahead of them
#define BOT_CUT_MARGIN_FRAMES 20 // cuts when this much further ahead than across
// Racing, it rides a lane this far to its side of their line, not on it: close beside their trail,
// a turn of theirs lays a new trail across its nose before it can react
#define BOT_LANE 400

// The weights: what it cares about, and how much
#define BOT_WEIGHT_ROOM 2
#define BOT_WEIGHT_CHASE 1
// A turn costs this many frames of travel's worth of score
#define BOT_TURN_COST_FRAMES 20

// It scores its options every BOT_THINK_FRAMES frames (a person doesn't rethink every 60th of
// a second either). Danger ahead is checked every frame: one collision query
#define BOT_THINK_FRAMES 4
// After a turn of its own choosing, it holds its line this long before choosing another
// (danger can still make it turn): a person settles into a line before changing it
#define BOT_CALM_FRAMES 30

// Reaction time: 9 + (0..7) frames, so 9 to 16 (150 to 267 ms), 12.5 on average. The 7 is a
// mask, so picking it is a single AND: 8 values, a power of two
#define BOT_REACTION_MIN 9
#define BOT_REACTION_SPREAD_MASK 7
#define BOT_REACTION_MAX (BOT_REACTION_MIN + BOT_REACTION_SPREAD_MASK)

/*
 * Difficulty. The numbers above are Easy's. Harder bots are the same brain with a quicker hand
 * and more nerve, not new rules: they still only press buttons a player can, and still play
 * safety first. Picked by measuring in the PC simulation (300 one-minute rounds against a
 * scripted player that weaves and takes 10-16 frames to react, as a person does): against
 * Easy, Medium and Hard survive longer (252 and 264 of 300, from 238) and kill that player
 * more often (80 and 77, from 63; more against one weaving close by). What changes:
 *   - reaction: quicker to press a turn once it has chosen one (Hard: 3-10 frames, 50-167 ms,
 *     a sharp human; Easy's 9-16 is an average one). The biggest single gain, in both
 *   - boost from: the gauge it needs before it starts a boost. From half: more bursts to get
 *     ahead and cut across
 *   - calm: how long it holds its line after a turn before choosing another. Shorter: it
 *     answers your turns sooner (much shorter, or cheaper turns, made it twitchy: it boxed
 *     itself in)
 *   - cut margin: how much further ahead than across it must be before it cuts across your
 *     path (kept above its slowest reaction, so it's still in time)
 *   - chase: how much closing on where it aims to cut you off counts against room
 * Tried and left alone, as no help: seeing all round (it kept answering your every move and
 * committed to fewer cuts: half the kills), thinking more often, judging room further out.
 */
#define BOT_EASY 0
#define BOT_MEDIUM 1
#define BOT_HARD 2
#define BOT_SKILL_COUNT 3

typedef struct
{
	const char *name;				 // what the status line says when it's picked
	unsigned char reaction_min;		 // frames; plus 0 to BOT_REACTION_SPREAD_MASK more
	unsigned char calm_frames;		 // BOT_CALM_FRAMES on Easy
	unsigned char cut_margin_frames; // BOT_CUT_MARGIN_FRAMES on Easy; above its slowest reaction
	unsigned char boost_from;		 // gauge needed to start a boost (CYCLE_GAUGE_MAX: full)
	unsigned char chase_weight;		 // BOT_WEIGHT_CHASE on Easy
} bot_skill;

extern const bot_skill bot_skills[BOT_SKILL_COUNT];

// What a bot remembers between frames
typedef struct
{
	unsigned int random;	// its own random number sequence (xorshift: never 0)
	unsigned char reacting; // frames until it presses the turn it has planned; 0: none planned
	signed char planned;	// the turn it will press: TURN_LEFT / TURN_RIGHT
	unsigned char thinking; // frames until it next scores its options
	unsigned char calm;		// frames until it may choose a turn again (danger aside)
	unsigned char boosting; // 1 while it holds boost
	// 1 if it may look round and decide this frame (bot_init sets it). The game takes turns
	// between its bots, one a frame (input.c): thinking is the costly part, ~12 questions about
	// every trail, and 3 bots at once among long trails took 45% of a frame. Off its turn it rides
	// on as it was, and still presses a turn it's reacting to. Up to 2 frames late noticing
	// something, beside its 9-16 frame reaction: the PC simulation's survival barely moved
	unsigned char may_think;
	unsigned char in_danger; // 1 if something was in its way last frame (danger seen afresh: think now)
	unsigned char skill;	 // BOT_EASY, BOT_MEDIUM or BOT_HARD: its row of bot_skills
	// What it believes of the nearest rival while they're behind it (find_target): where it last
	// saw them and which way they were going. One rival's worth: there are two riders so far
	unsigned char believes;		 // 0 until it has a belief (the round's start gives it one)
	unsigned char unseen;		 // frames since it last saw them, up to BOT_MEMORY_FRAMES
	unsigned char rival_heading;
	world_point rival_seen;
} bot_mind;

// A fresh mind. seed: any number, picking its sequence of reaction times. The same seed always
// gives the same bot, so a run can be repeated exactly (tests, and the attract mode later)
void bot_init(bot_mind *mind, unsigned int seed);

// How hard it plays: BOT_EASY (bot_init's), BOT_MEDIUM or BOT_HARD. Can change mid-round
void bot_set_skill(bot_mind *mind, int skill);

// This frame's buttons for the bot riding me.
// rivals: the other riders on the field (it only uses those it can see). trails: every trail on
// the field, its own included (the same list its cycle collides with)
rider_controls bot_think(bot_mind *mind, const rider *me, const rider *const *rivals, int rival_count,
						 const trail *const *trails, int trail_count);

#endif
