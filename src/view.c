#include "view.h"
#include "floor_fade_vdp2.h"
#include "object_fog_vdp1.h"
#include "game/turn_ease.h"
#include "game/camera_shake.h"

// 1st person: camera sits on the cycle and turns with it (easing round: turn_ease.h)
#define FIRST_PERSON_FOV 90
#define FIRST_PERSON_Z (-35)
#define FIRST_PERSON_RX DEGtoANG(90.0)
// Boosting widens the 1st person view: the classic sense-of-speed kick, as if the world
// stretches past. Culling and wall_clip are told the wider view, so no wall pieces go missing at
// the screen edges. The floor fog's rows are worked out for 90 degrees, so while widened it sits
// a little nearer than it should (up to ~15%).
// The FOV eases towards where the boost level says (ease_out_step), rather than following it:
// following it, it widened 6 degrees a frame and looked like a glitch. It's kept in SGL's ANGLE
// units (65536 a circle), not whole degrees, so even its slowest steps are smooth
#define FIRST_PERSON_BOOST_FOV 12				   // extra degrees at full boost
#define FIRST_PERSON_FOV_ANGLE DEGtoANG(FIRST_PERSON_FOV) // constants: worked out by the compiler
#define BOOST_FOV_ANGLE DEGtoANG(FIRST_PERSON_BOOST_FOV)
#define FOV_EASE_SHARE 2	 // sixteenths of the way each frame: most of it in ~0.3 s, settled in ~0.6 s
#define FOV_EASE_MIN_STEP 32 // 0.18 degrees: the slowest it moves before it lands

/*
 * Looking left and right (L / R, 1st person): the head turns LOOK_ANGLE to the side, easing
 * there and back like the turn camera, and dips a little towards the floor as it turns, as a
 * person turning their head does. The cycle keeps going straight: only the camera turns.
 * The dip is in proportion to how far the head has turned: a multiply and a shift (x 27 / 256 is
 * ~8 / 75, so 7.9 degrees down at the full 75 to the side; 6 was too slight to feel, 10 too much).
 * The floor fog's rows depend on the pitch (the horizon moves up as the camera dips: ~22 rows at
 * 8 degrees, which would leave a band of far floor unfogged), so it's rebuilt for the new pitch
 * when that has changed by more than LOOK_FOG_TILT_STEP: about 14 times through a look.
 */
#define LOOK_ANGLE DEGtoANG(75.0)
#define LOOK_PITCH_X256 27
#define LOOK_EASE_SHARE 4	   // sixteenths of the way a frame: quicker than a turn, it's a glance
#define LOOK_EASE_MIN_STEP 64 // 0.35 degrees
#define LOOK_FOG_TILT_STEP 10 // tan(pitch) x 1000: about 0.6 degrees

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

/*
 * Spectating (the prologue's camera): the arena seen from far off, as if watching someone else
 * play at the arcades. High, shallow and telephoto: 9000 away along its line of sight, 25 degrees
 * below level, through a 30 degree FOV. A narrow FOV from far away flattens the depth, like a
 * long lens; a wide one from close up would look like iso. The floor stretches off into the fog
 * with no walls, as if it went on for ever.
 * It faces 30 degrees east of north and keeps that angle, so the message the cycle writes
 * across the floor (input.c) reads left to right, leaning a little like italics. Trails are walls
 * with no thickness: facing straight north, every north-south stroke was edge-on, a 1 pixel
 * hairline, and the letters read as dashes. 30 degrees round shows both kinds of stroke a face;
 * turned east (not west), the words already written are on the near side, bigger.
 * (It used to circle round the cycle, but then the cycle seemed to drift sideways and curve,
 * though it ran straight. 6000 away was too close to feel like watching from the stands.)
 * It pans after the cycle, easing a share of the way each frame, rather than sitting on it: the
 * cycle goes back and forth inside each letter, and the camera following that would jitter.
 *
 *         camera
 *           *  .
 *      3804 |      .  9000 (line of sight)
 *           |  25 deg  .
 *    floor  +-------------- cycle
 *                8157
 */
#define SPECTATE_FOV 30
#define SPECTATE_HEIGHT 3804		// 9000 x sin 25
#define SPECTATE_BACK 8157			// 9000 x cos 25: how far behind the cycle along the floor
#define SPECTATE_RX DEGtoANG(65.0)	// from straight down: 25 degrees below level
#define SPECTATE_RZ DEGtoANG(30.0)	// 30 degrees round from north towards east (a right turn)
// Where it stands from the cycle: SPECTATE_BACK behind it, the way it faces. Facing is
// (-sin, -cos) (heading 0 is north, -y), so behind is (+sin 30, +cos 30) x 8157. Worked out
// here, as ISO_FOLLOW_OFFSET is, so there's no trigonometry at runtime: change RZ and redo them
#define SPECTATE_BACK_X 4079 // 8157 x sin 30
#define SPECTATE_BACK_Y 7064 // 8157 x cos 30
// It looks at a point SPECTATE_LEAD_X west (+x) of the cycle, not at the cycle: the message runs
// east, so what's been written is west of it. The cycle rides on the right of the screen, with
// the word so far laid out on the left (looking at the cycle, half the screen was empty floor)
#define SPECTATE_LEAD_X 1200
// What's drawn, round the point it looks at (view_visible_rect): a little more than the screen
// shows along the message, and the message's height with room to spare across it
#define SPECTATE_VIEW_X 3000
#define SPECTATE_VIEW_Y 1500
#define SPECTATE_PAN_SHARE 1		// sixteenths of the way to the cycle each frame
#define SPECTATE_PAN_MIN_STEP 1

bool first_person = true; // the game starts in 1st person; Down switches to iso
bool spectating = false;  // the prologue's far-off camera (above); it wins over the other two
bool view_linear_turn = false;
int view_x;
int view_y;
int view_z;
ANGLE view_rx;
ANGLE view_rz;

// Accumulated heading angle, eased towards on every turn
static int heading_target = 0;
static int heading_current = 0;

// The head's turn to the side (ANGLE, negative left), on top of the heading, eased to the target
static int look_target = 0;
static int look_now = 0;
static int fog_tilt = 0; // the floor fog's tilt (tan x 1000) as last built

// A jolt that dies away (a bot crashed into your trail): this frame's share is added on top of
// everything else. The floor fog isn't rebuilt for its pitch: it's small and over in ~0.2 s
static camera_shake shake;
static int shake_yaw = 0; // this frame's (ANGLE), for wall_clip's camera too

// SGL's tangent (16.16 fixed point). jo's headers don't declare it; this is SGL's own line (sl_def.h)
extern FIXED slTan(ANGLE);

// The 1st person FOV now (ANGLE), and how far it reaches sideways per unit ahead (wall_clip.h)
static int fov_now = FIRST_PERSON_FOV_ANGLE;
static int view_width_now = WALL_CLIP_VIEW_WIDTH_90;

// One frame of easing towards the target FOV (ANGLE)
static void ease_first_person_fov(int target)
{
	int fov = ease_out_step(fov_now, target, FOV_EASE_SHARE, FOV_EASE_MIN_STEP);

	if (fov == fov_now)
		return; // only while it's moving, not every frame
	fov_now = fov;
	slPerspective((ANGLE)fov);
	// tan(fov / 2) x 16, rounded up so the culling errs wide
	view_width_now = (slTan((ANGLE)(fov >> 1)) * 16 + 0xFFFF) >> 16;
}

static void apply_view(const cycle *c)
{
	int fade_start;
	int fade_end;
	int floor_view;

	if (spectating)
	{
		jo_3d_perspective_angle(SPECTATE_FOV);
		view_z = -SPECTATE_HEIGHT;
		view_rx = SPECTATE_RX;
		view_rz = SPECTATE_RZ;
		view_x = c->position.x + SPECTATE_LEAD_X + SPECTATE_BACK_X; // straight there, not easing in from wherever it was
		view_y = c->position.y + SPECTATE_BACK_Y;
		floor_view = FLOOR_VIEW_SPECTATOR;
	}
	else if (first_person)
	{
		jo_3d_perspective_angle(FIRST_PERSON_FOV);
		fov_now = FIRST_PERSON_FOV_ANGLE;
		view_width_now = WALL_CLIP_VIEW_WIDTH_90;
		view_z = FIRST_PERSON_Z;
		view_rx = FIRST_PERSON_RX;
		heading_current = heading_target;
		look_now = look_target;
		fog_tilt = 0; // floor_fade_set_view (below) builds it level
		floor_view = FLOOR_VIEW_FIRST_PERSON;
	}
	else
	{
		jo_3d_perspective_angle(ISO_FOV);
		view_z = ISO_Z;
		view_rx = ISO_RX;
		view_rz = ISO_RZ;
		floor_view = FLOOR_VIEW_ISO;
	}
	floor_fade_set_view(floor_view); // each camera sees the floor at different distances
	// Walls and trails fade over the same distances as the floor, so they fade together
	floor_fade_distances(floor_view, &fade_start, &fade_end);
	object_fog_set_distances(fade_start, fade_end);
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

	if (spectating && !free_camera)
	{
		// Only round the point it's looking at (the middle of the screen): SPECTATE_VIEW_X along
		// the message either way, SPECTATE_VIEW_Y across it. Not "everything in range": through
		// the narrow FOV, a stroke thousands of units to the side lands over 1000 pixels off the
		// screen's centre, past what VDP1 can hold, and was drawn as a spike. Every corner of
		// this box stays within ~250 pixels of the centre; the trails are cut to it
		world_rect r;
		int look_x = view_x - SPECTATE_BACK_X;
		int look_y = view_y - SPECTATE_BACK_Y;

		r.x_min = (short)(look_x - SPECTATE_VIEW_X);
		r.x_max = (short)(look_x + SPECTATE_VIEW_X);
		r.y_min = (short)(look_y - SPECTATE_VIEW_Y);
		r.y_max = (short)(look_y + SPECTATE_VIEW_Y);
		return r;
	}
	if (free_camera)
		mode = VIEW_CULL_ALL;
	else if (!first_person)
		mode = VIEW_CULL_ISO;
	else if (heading_current != heading_target || look_now != 0 || camera_shake_active(&shake))
		mode = VIEW_CULL_TURNING; // easing round, looking aside or shaking: it faces between headings
	else if (fov_now != FIRST_PERSON_FOV_ANGLE)
		mode = VIEW_CULL_TURNING; // boosting: wider than the 90 degree rectangle, so look all round
	else
		mode = VIEW_CULL_FIRST_PERSON;
	return view_cull_rect(c->position, c->heading, mode, range);
}

bool view_wall_clip_camera(const cycle *c, bool free_camera, wall_clip_camera *out)
{
	// Where the camera faces now: mid-turn, looking aside or shaking
	ANGLE yaw = (ANGLE)(heading_current + look_now + shake_yaw);

	if (free_camera || spectating || !first_person)
		return false;
	out->eye = c->position;
	// Heading 0 faces north (-y) and turning right goes east (-x): forward = (-sin, -cos).
	// slSin/slCos give 16.16 fixed point; >> 6 makes it x1024
	out->forward_x = -(slSin(yaw) >> 6);
	out->forward_y = -(slCos(yaw) >> 6);
	out->view_width = view_width_now;
	return true;
}

bool view_fog_camera(bool free_camera, world_point *eye, int *forward_x, int *forward_y)
{
	// The point on the floor under the camera, and the way it faces: the floor fog measures its
	// distances from there (game/floor_fade.h). In 1st person that's the cycle; in iso, the
	// camera's spot behind and above it. view_rz is the camera's turn in both (mid-turn too)
	eye->x = (short)view_x;
	eye->y = (short)view_y;
	// Heading 0 faces north (-y), turning right goes east (-x): forward = (-sin, -cos), x1024
	*forward_x = -(slSin(view_rz) >> 6);
	*forward_y = -(slCos(view_rz) >> 6);
	// No fog for the debug camera: it looks anywhere, so the fog's distances don't fit it.
	// None spectating either: the prologue's message stands about where the floor's fog only
	// begins, and with the fog's Gouraud shading on, its strokes going off the left of the screen
	// (from that far, through the narrow FOV) were drawn as spikes. Without it they're clean
	return !free_camera && !spectating;
}

void view_turn_left(void)
{
	heading_target -= 0x4000;
}

void view_turn_right(void)
{
	heading_target += 0x4000;
}

void view_look(int direction)
{
	look_target = direction * LOOK_ANGLE;
}

void view_shake(int strength)
{
	camera_shake_start(&shake, strength);
}

void view_spectate_shift(int dx)
{
	view_x += dx;
}

void view_follow(const cycle *c)
{
	int shake_pitch;

	// This frame's jolt (0, 0 when still), then it dies away a little
	camera_shake_step(&shake, &shake_yaw, &shake_pitch);
	if (spectating)
	{
		// Easing after the cycle, from SPECTATE_BACK behind it
		view_x = ease_out_step(view_x, c->position.x + SPECTATE_LEAD_X + SPECTATE_BACK_X, SPECTATE_PAN_SHARE, SPECTATE_PAN_MIN_STEP);
		view_y = ease_out_step(view_y, c->position.y + SPECTATE_BACK_Y, SPECTATE_PAN_SHARE, SPECTATE_PAN_MIN_STEP);
		// The jolt of a press breaking at the loop (input.c), at full size: through the narrow FOV
		// it swings the picture a tenth of the screen, a hit
		view_rz = (ANGLE)(SPECTATE_RZ + shake_yaw);
		view_rx = (ANGLE)(SPECTATE_RX + shake_pitch);
	}
	else if (first_person)
	{
		view_x = c->position.x;
		view_y = c->position.y;

		// Ease-out: a big swing on the frame of the turn, settling over the next few (turn_ease.h)
		if (view_linear_turn)
			heading_current = turn_linear_step(heading_current, heading_target);
		else
			heading_current = turn_ease_step(heading_current, heading_target);
		// The head's turn to the side, and its dip towards the floor
		look_now = ease_out_step(look_now, look_target, LOOK_EASE_SHARE, LOOK_EASE_MIN_STEP);
		view_rz = (ANGLE)(heading_current + look_now + shake_yaw);
		{
			int pitch = ((look_now < 0 ? -look_now : look_now) * LOOK_PITCH_X256) >> 8;
			int tilt = (slTan((ANGLE)pitch) * 1000) >> 16; // tan x 1000, as the floor fog keeps it

			// Less than level (90 degrees) looks down. The shake's pitch isn't in the fog's tilt
			view_rx = (ANGLE)(FIRST_PERSON_RX - pitch + shake_pitch);
			int change = tilt > fog_tilt ? tilt - fog_tilt : fog_tilt - tilt;

			// Rebuilt in steps, and once more on landing level, so it ends exactly right
			if (change > LOOK_FOG_TILT_STEP || (tilt == 0 && fog_tilt != 0))
			{
				fog_tilt = tilt;
				floor_fade_set_first_person_tilt(tilt);
			}
		}

		// Ease towards the width the boost level asks for (0..CYCLE_BOOST_FULL, which is 16: >> 4
		// divides by it). The level ramps up over 8 frames; easing on top rounds both ends off
		ease_first_person_fov(FIRST_PERSON_FOV_ANGLE + ((c->boost * BOOST_FOV_ANGLE) >> 4));
	}
	else
	{
		view_x = c->position.x + ISO_FOLLOW_OFFSET;
		view_y = c->position.y + ISO_FOLLOW_OFFSET;
		// Half the jolt: from up here a degree swings the whole arena
		view_rz = (ANGLE)(ISO_RZ + (shake_yaw >> 1));
		view_rx = (ANGLE)(ISO_RX + (shake_pitch >> 1));
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
