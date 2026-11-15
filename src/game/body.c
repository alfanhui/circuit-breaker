#include "body.h"
#include "frustum.h" // frustum_isqrt
#include "walker.h"	 // walker_sin, the walker's speeds

#define U(units) ((units)*BODY_SUB) // whole units to the body's x16

// The head (jaw to brow, brow to crown), the torso, then each side's arm (upper, fore, palm,
// fingers) and leg (thigh, shin, foot). Widths x16: each strip ends as wide as the next begins
#define ARM(s)                                                 \
	{BODY_SHOULDER + (s), BODY_ELBOW + (s), 46, 36, BODY_PART_ARM}, \
		{BODY_ELBOW + (s), BODY_WRIST + (s), 36, 25, BODY_PART_ARM}, \
		{BODY_WRIST + (s), BODY_KNUCKLES + (s), 25, 36, BODY_PART_HAND}, \
	{                                                          \
		BODY_KNUCKLES + (s), BODY_FINGERS + (s), 36, 14, BODY_PART_HAND \
	}
#define LEG(s)                                                 \
	{BODY_HIP + (s), BODY_KNEE + (s), 62, 44, BODY_PART_LEG},  \
		{BODY_KNEE + (s), BODY_ANKLE + (s), 44, 27, BODY_PART_LEG}, \
	{                                                          \
		BODY_ANKLE + (s), BODY_TOES + (s), 27, 36, BODY_PART_FOOT \
	}
const body_bone body_bones[BODY_BONES] = {{BODY_NECK, BODY_BROW, 36, 76, BODY_PART_HEAD},
										  {BODY_BROW, BODY_CROWN, 76, 50, BODY_PART_HEAD},
										  {BODY_NECK, BODY_PELVIS, 168, 100, BODY_PART_TORSO},
										  ARM(0),
										  LEG(0),
										  ARM(BODY_SIDE),
										  LEG(BODY_SIDE)};

// Dark glass, its edges caught in cyan light; the hands and the head's rim a little brighter, as
// they're what you see most of
const body_skin body_skin_glass = {
	{BODY_RGB(20, 24, 44), BODY_RGB(14, 16, 32), BODY_RGB(16, 20, 40), BODY_RGB(24, 32, 60), BODY_RGB(14, 16, 32),
	 BODY_RGB(20, 24, 44)},
	{BODY_RGB(140, 240, 255), BODY_RGB(60, 170, 230), BODY_RGB(90, 210, 250), BODY_RGB(140, 240, 255),
	 BODY_RGB(60, 170, 230), BODY_RGB(90, 210, 250)}};

void body_init(body *b)
{
	b->mode = BODY_MOVING;
	b->mode_frames = 0;
	b->posed = 0;
}

static body_point point(int x, int y, int z)
{
	body_point p;

	p.x = (short)x;
	p.y = (short)y;
	p.z = (short)z;
	return p;
}

/*
 * Where a limb bends. Two bones, a and b long, run from `root` (a shoulder, a hip) to `end` (the
 * wrist, the ankle): the joint between them (the elbow, the knee) lies on a circle round the line
 * from root to end, and `pole` says which side of it: the way the joint sticks out (elbows back
 * and down, knees forward). With d the distance from root to end, the joint's
 *     along = (a^2 - b^2 + d^2) / 2d   along that line from the root (the cosine rule), and
 *     h = sqrt(a^2 - along^2)          out from it, towards the pole.
 * If `end` is further than the limb reaches (or nearer than it folds), it's pulled in (or pushed
 * out) along the line: the limb's straight, or folded, pointing the way it was asked to go.
 *
 * Divisions: one for the line's direction, one for the pole's (reciprocals, then multiplies),
 * and one more only when `end` was out of reach. Four limbs a frame: once per body, never per
 * polygon (CLAUDE.md)
 */
static void solve(body_point root, body_point *end, int a, int b, int px, int py, int pz, body_point *mid)
{
	int dx = end->x - root.x, dy = end->y - root.y, dz = end->z - root.z;
	int d = frustum_isqrt((unsigned int)(dx * dx + dy * dy + dz * dz));
	const int reach = a + b - 2, least = (a > b ? a - b : b - a) + BODY_SUB;
	int recip, ux, uy, uz, along, h, dot, qx, qy, qz, len;

	if (d < 4)
	{
		// On the root itself: no line to speak of. Straight down, then
		dx = dy = 0;
		dz = -BODY_SUB;
		d = BODY_SUB;
	}
	recip = (1 << 20) / d;
	ux = (dx * recip) >> 10; // the line's direction, 1024 long
	uy = (dy * recip) >> 10;
	uz = (dz * recip) >> 10;
	if (d > reach || d < least)
	{
		d = d > reach ? reach : least;
		end->x = (short)(root.x + ((ux * d) >> 10));
		end->y = (short)(root.y + ((uy * d) >> 10));
		end->z = (short)(root.z + ((uz * d) >> 10));
		recip = (1 << 20) / d;
	}
	along = (((a * a - b * b + d * d) >> 1) * recip) >> 20;
	h = a * a - along * along;
	h = h > 0 ? frustum_isqrt((unsigned int)h) : 0;
	// The pole, less its part along the line: what's left is square to it. If the pole runs along
	// the line (nothing left), any way out will do: up, or to the side if the line's upright
	for (int pass = 0;; ++pass)
	{
		dot = (px * ux + py * uy + pz * uz) >> 10;
		qx = px - ((ux * dot) >> 10);
		qy = py - ((uy * dot) >> 10);
		qz = pz - ((uz * dot) >> 10);
		len = frustum_isqrt((unsigned int)(qx * qx + qy * qy + qz * qz));
		if (len >= 64 || pass == 1)
			break;
		px = uz > 700 || uz < -700 ? 1024 : 0;
		py = 0;
		pz = 1024 - px;
	}
	if (len < 1)
		len = 1;
	recip = (1 << 20) / len;
	qx = (qx * recip) >> 10; // 1024 long
	qy = (qy * recip) >> 10;
	qz = (qz * recip) >> 10;
	mid->x = (short)(root.x + ((ux * along + qx * h) >> 10));
	mid->y = (short)(root.y + ((uy * along + qy * h) >> 10));
	mid->z = (short)(root.z + ((uz * along + qz * h) >> 10));
}

// A point a share (0 .. 256) of the way from a to b
static body_point between(body_point a, body_point b, int share)
{
	return point(a.x + (((b.x - a.x) * share) >> 8), a.y + (((b.y - a.y) * share) >> 8),
				 a.z + (((b.z - a.z) * share) >> 8));
}

// One step of easing: a point moves 1 / 2^shift of the way to where it's wanted
static void ease(body_point *p, body_point want, int shift)
{
	p->x = (short)(p->x + ((want.x - p->x) >> shift));
	p->y = (short)(p->y + ((want.y - p->y) >> shift));
	p->z = (short)(p->z + ((want.z - p->z) >> shift));
}

void body_update(body *b, const body_state *s)
{
	const int prone = s->mode == BODY_BRACE || s->mode == BODY_LIE; // face down, legs out behind
	body_point want_hand[2], want_foot[2];
	body_point neck, pelvis;
	int drop, lean;
	/*
	 * A side-step (strafing: walker.h, L / R). strafe: its share of a full one, 0 .. 256 (the
	 * speed / 576, as x 114 >> 8: no division); way: 1 to your right, -1 left. Only on your feet
	 */
	const int side_speed = s->mode == BODY_MOVING ? (s->side < 0 ? -s->side : s->side) : 0;
	const int strafe = side_speed >= WALKER_STRAFE_SPEED ? 256 : (side_speed * 114) >> 8;
	const int way = s->side < 0 ? -1 : 1;

	if (b->mode != s->mode)
	{
		b->mode = (unsigned char)s->mode;
		b->mode_frames = 0;
		b->plant = U(BODY_PLANT_AHEAD); // a vault's edge: just ahead of your feet
		/*
		 * Into a vault or a climb the ground under you changes: it was the floor, it's the top
		 * now (your feet are on it), so the eye is suddenly that much lower above "the ground"
		 * though it hasn't moved. The hands and feet haven't moved either: they're put down by the
		 * same amount, to be where they were, and ease on from there
		 */
		if (b->posed && (s->mode == BODY_VAULT || s->mode == BODY_CLIMB))
			for (int k = 0; k < 2; ++k)
			{
				b->hand[k].z = (short)(b->hand[k].z + s->eye - b->eye_was);
				b->foot[k].z = (short)(b->foot[k].z + s->eye - b->eye_was);
			}
	}
	else
	{
		if (b->mode_frames < 255)
			b->mode_frames++;
		// A vault's edge stays where it is; you fly on: it falls behind by your speed (x256 to x16)
		if (b->plant > -U(BODY_PLANT_BEHIND + 1))
			b->plant = (short)(b->plant - (s->speed >> 4));
	}

	/*
	 * The spine hangs from the eye: the head round it, the neck just under. The pelvis is a torso's
	 * length from the neck: straight below it while there's room. As the eye comes down there isn't,
	 * and the torso leans instead: Pythagoras gives how far (lean^2 = torso^2 - drop^2). Back, for a
	 * crouch or lying face down; forward under you in a slide (you lean back on it)
	 */
	neck = point(0, -U(1), s->eye - U(BODY_EYE_TO_NECK));
	drop = neck.z - (prone || s->mode == BODY_SLIDE ? U(4) : U(14)); // how low the pelvis may go
	if (drop > U(BODY_TORSO) || s->mode == BODY_CLIMB)
		drop = U(BODY_TORSO); // climbing, the ground's the ledge's top: you hang below it, upright
	if (drop < 0)
		drop = 0;
	lean = frustum_isqrt((unsigned int)(U(BODY_TORSO) * U(BODY_TORSO) - drop * drop));
	// (side-stepping, you lean into it: the head leads and the hips trail, 3 units behind the way
	// you go. The torso's 24 long: a lean of 3 shortens its drop by a fifth of a unit, inside the
	// bones' rounding)
	pelvis = point(-way * ((U(3) * strafe) >> 8), neck.y + (s->mode == BODY_SLIDE ? lean : -lean), neck.z - drop);
	b->joint[BODY_CROWN] = point(0, 0, s->eye + U(BODY_EYE_TO_CROWN));
	b->joint[BODY_BROW] = point(0, 0, s->eye + U(1));
	b->joint[BODY_NECK] = neck;
	b->joint[BODY_PELVIS] = pelvis;

	for (int k = 0; k < 2; ++k)
	{
		const int side = k == 0 ? -1 : 1; // left's -x
		const int base = k * BODY_SIDE;
		const body_point shoulder = point(side * U(BODY_SHOULDERS), neck.y, neck.z - U(2));
		const body_point hip = point(pelvis.x + side * U(BODY_HIPS), pelvis.y, pelvis.z);
		// Arms hanging: the wrist a little out and forward of the shoulder, nearly straight down
		const body_point hang = point(shoulder.x + side * U(3), shoulder.y + U(2), shoulder.z - U(26));

		b->joint[BODY_SHOULDER + base] = shoulder;
		b->joint[BODY_HIP + base] = hip;
		switch (s->mode)
		{
		case BODY_AIR:
			// Arms up and forward for balance, into the lower corners of your view; legs tucked,
			// one ahead of the other
			want_hand[k] = point(shoulder.x + side * U(3), shoulder.y + U(22), shoulder.z + U(1));
			want_foot[k] = k == 0 ? point(-U(5), U(10), U(9)) : point(U(5), -U(7), U(5));
			break;
		case BODY_SLIDE:
			// Legs out ahead along the floor, one knee up; the left hand on the floor behind you,
			// the right out in front
			want_foot[k] = k == 0 ? point(-U(4), U(54), U(3)) : point(U(5), U(44), U(10));
			want_hand[k] = k == 0 ? point(-U(15), -U(6), U(1)) : point(shoulder.x + U(2), shoulder.y + U(18), shoulder.z - U(2));
			break;
		case BODY_BRACE:
			// Thrown from the cycle: both hands out in front of your face
			want_hand[k] = point(shoulder.x - side * U(3), shoulder.y + U(25), shoulder.z + U(4));
			want_foot[k] = point(side * U(6), pelvis.y - U(40), U(2));
			break;
		case BODY_LIE:
			// Lying: hands on the floor ahead. Then the right one lifts in front of your face:
			// you look at it (you have hands)
			want_hand[k] = point(side * U(10), U(27), U(1));
			if (k == 1 && b->mode_frames >= BODY_LIE_LOOK)
				want_hand[k] = point(U(4), U(21), s->eye - U(7));
			want_foot[k] = point(side * U(6), pelvis.y - U(40), U(2));
			break;
		case BODY_RISE:
			// Pushing up off the floor until it's out of reach, then the arms hang; the feet come
			// in under you
			want_hand[k] = s->eye < U(46) ? point(side * U(11), U(14), 0) : hang;
			want_foot[k] = point(side * U(BODY_HIPS), 0, U(2));
			break;
		case BODY_VAULT:
			/*
			 * Over something low: the left hand on its top where you took off (the ground here is
			 * that top: your feet are on it or just above), while it's in reach; the right arm
			 * and the legs as a jump's, the legs swung to the right, clear of it
			 */
			want_hand[k] = k == 0 && b->plant > -U(BODY_PLANT_BEHIND) ? point(-U(9), b->plant, 0)
																	   : point(shoulder.x + side * U(3), shoulder.y + U(22), shoulder.z + U(1));
			want_foot[k] = k == 0 ? point(U(2), U(12), U(10)) : point(U(9), -U(4), U(7));
			break;
		case BODY_CLIMB:
			/*
			 * Up onto a ledge: both hands on its edge (the ground here is its top), and you below
			 * them, hanging, the eye rising past your hands as you pull up. Your feet hang under
			 * you until they reach the top. Once your eye's BODY_CLIMB_UP over it the top's out of
			 * your arms' reach below you: they let go, and hang
			 */
			want_hand[k] = s->eye < U(BODY_CLIMB_UP) ? point(side * U(9), U(BODY_CLIMB_AHEAD), 0) : hang;
			want_foot[k] = point(side * U(BODY_HIPS), pelvis.z < U(34) ? U(3) : 0, pelvis.z < U(34) ? pelvis.z - U(32) : U(2));
			break;
		default:
		{
			/*
			 * Standing, walking, running: all one pose, by speed. The walker's stride (an ANGLE) is
			 * the step's phase: a whole turn is a step of each foot. The left foot's forward of
			 * you by sin(stride) and lifts while it swings forward (cos > 0); the right's half a
			 * turn behind. The arms swing against the legs, as arms do.
			 * walk, run: shares of 256. The speed / 768 is x 85 >> 8 (1 / 3.01): no division
			 */
			const int speed = s->speed < 0 ? -s->speed : s->speed;
			const int walk = speed >= WALKER_FORWARD_SPEED ? 256 : (speed * 85) >> 8;
			const int past = speed - WALKER_FORWARD_SPEED;
			const int run = past <= 0 ? 0 : past >= WALKER_FORWARD_SPEED ? 256 : (past * 85) >> 8;
			const int swing = side * -walker_sin(s->stride);		   // this leg: 1024 fully forward
			const int lift = side * -walker_cos(s->stride);			   // > 0 while it swings forward
			const int reach = (walk * U(11) + run * U(11)) >> 8;	   // half a stride: 11 walking, 22 running
			const int height = (walk * U(4) + run * U(10)) >> 8;	   // how high the foot lifts
			const int arm = -swing;									   // this arm: 1024 fully forward
			const int up = arm > 0 ? arm : 0, back = arm < 0 ? -arm : 0;
			// Walking, the arm swings a little as it hangs. Running, the elbow's bent and the hand
			// pumps: forward it comes up to shoulder height, where your eyes can see it
			const body_point walking = point(hang.x, hang.y + ((((arm * U(7)) >> 10) * walk) >> 8), hang.z);
			const body_point running = point(shoulder.x - side * U(2), shoulder.y + U(3) + ((arm * U(17)) >> 10),
											 shoulder.z - U(15) + ((up * U(15)) >> 10) + ((back * U(3)) >> 10));

			/*
			 * The side-step: the same step turned a quarter. Each foot swings the way you go
			 * (lifting as it does) and is carried back under you, the two half a turn apart, as
			 * walking: so the leading foot steps out, and the trailing one closes up to it. 4
			 * units each way, and the hips are 5 from the middle: the feet never cross (a shuffle,
			 * not a grapevine: ready to go either way). With a walk it adds up to a diagonal step.
			 * The arms come up a little from hanging, out and forward, elbows bent: a guard, for
			 * balance, swaying against the feet. Not enough to rise into view: a run's pump
			 * takes over from it as it does from the walk's swing
			 */
			const int step = (way * swing * ((U(4) * strafe) >> 8)) >> 10;
			// (it lifts while it swings the way you go: step grows with swing either way, so that's
			// the same half of the turn as a walk's lift)
			const int step_up = lift > 0 ? (lift * ((U(3) * strafe) >> 8)) >> 10 : 0;
			const body_point guard = point(hang.x + side * U(4) - ((way * swing * U(2)) >> 10), hang.y + U(5), hang.z + U(7));

			want_foot[k] = point(side * U(BODY_HIPS) + step, (swing * reach) >> 10,
								 U(2) + (lift > 0 ? (lift * height) >> 10 : 0) + step_up);
			want_hand[k] = between(between(walking, guard, strafe), running, run);
			break;
		}
		}
	}

	// The hands and feet ease to where they're wanted: quickly on the move (the stride's own
	// motion mustn't lag), slower into and out of the other poses, so they read as moves
	for (int k = 0; k < 2; ++k)
	{
		if (!b->posed)
		{
			b->hand[k] = want_hand[k];
			b->foot[k] = want_foot[k];
		}
		else if (b->mode_frames == 0 && (s->mode == BODY_CLIMB || (s->mode == BODY_VAULT && k == 0)))
		{
			// A hand taking an edge is there at once: it has only a few frames on it (the eye's up
			// over a ledge in 6, a vault's hand is behind you in 5), and eased, it never arrived
			b->hand[k] = want_hand[k];
			ease(&b->foot[k], want_foot[k], 2);
		}
		else
		{
			// (a vault's hand has 5 frames on the top: it must get there in one or two)
			const int shift = (s->mode == BODY_MOVING && b->mode_frames > 12) || s->mode == BODY_VAULT || s->mode == BODY_CLIMB ? 1 : 2;

			ease(&b->hand[k], want_hand[k], shift);
			ease(&b->foot[k], want_foot[k], shift);
		}
	}
	b->posed = 1;
	b->eye_was = (short)s->eye;

	for (int k = 0; k < 2; ++k)
	{
		const int side = k == 0 ? -1 : 1;
		const int base = k * BODY_SIDE;
		body_point wrist = b->hand[k], ankle = b->foot[k];
		body_point elbow, knee;

		// Elbows: back, down and out. Knees: forward (lying face down: at the floor)
		solve(b->joint[BODY_SHOULDER + base], &wrist, U(BODY_UPPER_ARM), U(BODY_FOREARM), side * 500, -700, -500, &elbow);
		if (prone)
			solve(b->joint[BODY_HIP + base], &ankle, U(BODY_THIGH), U(BODY_SHIN), 0, 200, -1000, &knee);
		else
			solve(b->joint[BODY_HIP + base], &ankle, U(BODY_THIGH), U(BODY_SHIN), 0, 900, 400, &knee);
		b->joint[BODY_ELBOW + base] = elbow;
		b->joint[BODY_WRIST + base] = wrist;
		// The hand goes on the way the forearm points: the palm 3.5 units of its 13 (x 69 >> 8),
		// the fingers 4.5 more (x 158 >> 8 from the wrist: at 6 in all it was a stub), their tips dropped a little: curled
		b->joint[BODY_KNUCKLES + base] = point(wrist.x + (((wrist.x - elbow.x) * 69) >> 8), wrist.y + (((wrist.y - elbow.y) * 69) >> 8),
											   wrist.z + (((wrist.z - elbow.z) * 69) >> 8));
		b->joint[BODY_FINGERS + base] = point(wrist.x + (((wrist.x - elbow.x) * 158) >> 8), wrist.y + (((wrist.y - elbow.y) * 158) >> 8),
											  wrist.z + (((wrist.z - elbow.z) * 158) >> 8) - U(1));
		b->joint[BODY_KNEE + base] = knee;
		b->joint[BODY_ANKLE + base] = ankle;
		// The foot: flat on the ground ahead of the ankle; trailing behind it lying face down;
		// toes up in a slide
		if (prone)
			b->joint[BODY_TOES + base] = point(ankle.x, ankle.y - U(6), ankle.z - U(2));
		else if (s->mode == BODY_SLIDE)
			b->joint[BODY_TOES + base] = point(ankle.x, ankle.y + U(3), ankle.z + U(6));
		else
			b->joint[BODY_TOES + base] = point(ankle.x, ankle.y + U(BODY_FOOT), ankle.z - U(2));
	}
}
