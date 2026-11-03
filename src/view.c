#include "view.h"
#include "floor_fade_vdp2.h"
#include "object_fog_vdp1.h"
#include "game/turn_ease.h"
#include "game/camera_shake.h"
#include "game/intro_flight.h"
#include "game/arrival_flight.h"
#include "game/city.h"

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
// On foot (view_on_foot), narrower: walking, nothing comes at you at 13 a frame, and less on the
// screen is less for VDP1 to fill (the stairs, the walls beside you). Things nearby look nearer
// their size, too: at 90 the grey box's stairs looked far off. Eased to as you get up
#define ON_FOOT_FOV 75
#define ON_FOOT_FOV_ANGLE DEGtoANG(ON_FOOT_FOV)
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

/*
 * On foot (game/walker.h, view_on_foot): the body turns freely, not in quarter turns, and at once
 * (the walker eases its own turning speed: easing the camera after it as well would lag). The
 * head's look (the face buttons) eases as on the cycle, and can look up and down as well: up
 * less far than down, as a neck does. The eye bobs with each step
 */
#define LOOK_UP_ANGLE DEGtoANG(25.0)
#define LOOK_DOWN_ANGLE DEGtoANG(35.0)

// Isometric (3rd person): fixed angle, camera follows the cycle 1:1.
// (It used to follow at 5/6 of the movement, which in a large arena left the cycle
// more than 1000 units behind the camera, off the screen.)
#define ISO_FOV 60
#define ISO_Z (-768)
#define ISO_RX DEGtoANG(40.0)
#define ISO_RZ DEGtoANG(45.0)
#define ISO_FOV_ANGLE DEGtoANG(ISO_FOV) // a constant: worked out by the compiler
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
 * It pans at one steady speed along the message (view_spectate_pan), not after the cycle: the
 * cycle goes back and forth inside each letter, and a camera easing after it sped up, slowed
 * and drifted back with it. The speed is the message's own: its length in the frames it takes
 * to write (attract_text.h, measured by a unit test), so the cycle stays within ~1365 of the
 * steady point, on screen. Across the message it stays on its centre line (view_spectate_line),
 * so the words stay still while the cycle rides up and down them.
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
// It looks at a point SPECTATE_LEAD_X west (+x) of the steady point, not at it: the message runs
// east, so what's been written is west of it. The cycle rides on the right of the screen, with
// the word so far laid out on the left (looking at the cycle, half the screen was empty floor).
// 700, not 1200: the cycle wanders up to 1365 east of the steady point, and the screen shows
// ~2400 either side of its middle at this distance (game/intro_flight.c's last key matches it)
#define SPECTATE_LEAD_X 700
// What's drawn, round the point it looks at (view_visible_rect): a little more than the screen
// shows along the message, and the message's height with room to spare across it
#define SPECTATE_VIEW_X 3000
#define SPECTATE_VIEW_Y 1500
#define SPECTATE_PAN_SHARE 1 // the opening flight eases after the cycle: sixteenths of the way a frame
#define SPECTATE_PAN_MIN_STEP 1
// The floor is drawn 2^this larger: 1000 unit tiles. From 9000 away a 125 unit tile is a few
// pixels, and its thin lines are sampled so sparsely (VDP2 has no mipmaps) that the grid broke
// up into dashes. Nobody's riding yet, so the ruler can change
#define SPECTATE_FLOOR_SHIFT 3

/*
 * The opening flight (game/intro_flight.h): from face down on the floor, alongside the cycle,
 * up to the horizon and banking round into the spectator's seat. While it flies, spectating is
 * on (the loop is writing) and the flight sets the camera every frame instead of the pan above.
 *  - The floor's tiles grow with the camera's height, a doubling at a time, so the grid never
 *    breaks into dashes nor fills the screen with one tile (INTRO_FLOOR_STEP_*)
 *  - The floor's fog is rebuilt for the moving camera every INTRO_FOG_FRAMES: each rebuild is
 *    a division a screen row, too many to do every frame. It's worked out with no roll: while
 *    the camera banks it's a little off, but by then it only starts thousands of units away
 *  - What's drawn is a box on the floor round where the middle of the screen looks, growing
 *    with how far off that is: as the spectator's box, it keeps the trails within VDP1's range
 */
#define INTRO_FOG_FRAMES 4
#define INTRO_FLOOR_STEP_1 700 // heights where the floor's tiles double: 250, 500, then 1000
#define INTRO_FLOOR_STEP_2 1400
#define INTRO_FLOOR_STEP_3 2800
#define INTRO_FLOOR_STEP_4 6000 // and 2000 above this: the arrival's overview, 10000 up (its grid broke up)
#define INTRO_VIEW_REACH_MAX 9000 // the furthest the middle of the screen is counted (the seat's 8157)
#define INTRO_VIEW_HALF_MIN 400	  // the box's half size: this, plus a third of how far off it is
// The bank only leans the picture once the horizon's off the top of the screen. VDP2 does roll
// the floor, but its fog is one strength a screen row (floor_fade_vdp2.c), level: with the
// horizon in sight, the tilted floor rose out of the fog's band into the sky at one corner.
// It leans in from INTRO_ROLL_FROM below level, fully INTRO_ROLL_SHIFT's 11 degrees further down
#define INTRO_ROLL_FROM INTRO_DEG(20) // the top of the screen is ~16 degrees above the middle at 45 FOV
#define INTRO_ROLL_SHIFT 3			  // (below - from) >> 3: 0..256 over 2048 ANGLE, 11.25 degrees

bool first_person = true; // the game starts in 1st person; Down switches to iso
bool spectating = false;  // the prologue's far-off camera (above); it wins over the other two
bool view_linear_turn = false;
int view_floor_shift = 0;
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

// On foot (view_on_foot): whether, the head's look up or down (ANGLE, + down) wanted and now, and
// how far the eye's down for the step
static bool on_foot = false;
static int look_pitch_target = 0;
static int look_pitch_now = 0;
static int foot_bob = 0;

// Spectating: the message's centre line, which the camera stays across from (view_spectate_line),
// and the steady point it pans with along the line (view_spectate_pan)
static int spectate_line_y = 0;
static int spectate_pan_x = 0;
// The camera's lean (ANGLE), only ever off 0 while the opening flight banks, or in the designed
// crash (view_breach)
static ANGLE view_roll = 0;
// The designed crash's tip down towards the floor (ANGLE) and how far the eye's dropped, 1st
// person (view_breach)
static int breach_dip = 0;
static int breach_drop = 0;

// The opening flight: its frame (-1: not flying), where the cycle began, the point easing after
// the cycle (lead ahead of it, along x), the FOV as last set, frames to the fog's next rebuild,
// and what's drawn this frame
static int intro_frame = -1;
static int intro_start_x;
static int intro_start_y;
static int intro_track_x;
static int intro_fov;
static int intro_fog_wait;
static world_rect intro_rect;

static void intro_follow(const cycle *c, int yaw, int pitch);

/*
 * The flight down from the stands into the cycle, once the loop's broken (game/arrival_flight.h).
 * Like the opening flight it sets the camera, its FOV and the fog every frame, and the floor's
 * tiles grow with its height. The arena's walls rise meanwhile (level.c), and they're the hard
 * part to draw: VDP1 wraps a corner more than 1023 pixels off the screen's middle into a spike,
 * and SGL puts a corner behind the camera on the screen anyway. So:
 *  - high up (ARRIVAL_LOW_HEIGHT and above) it looks down at them all, and only the walls round
 *    what's on the screen are drawn (arrival_footprint): nothing far off the screen's edges, and
 *    nothing behind the camera
 *  - lower down it's among them, near the arena's middle (a unit test keeps it 2500 from every
 *    wall), and they're clipped as in 1st person (wall_clip.h): from where the camera stands on
 *    the floor, the way it faces, as wide as its FOV
 */
#define ARRIVAL_WALL_TOP 1500	  // level.c's WALL_TOP_Z: the wall tops are a second floor to see
#define ARRIVAL_VIEW_REACH 30000 // the furthest a ray over the horizon is counted (the draw distance)
#define ARRIVAL_VIEW_MARGIN 300	 // round what's seen, so nothing pops at the screen's edges
#define ARRIVAL_FLOOR_FOG_END (ARRIVAL_VIEW_REACH - 2000) // the floor's black before the raised ground ends
#define ARRIVAL_FLOOR_FOG_MIN 2000 // the shortest fade: start at least this much before the end
#define VIEW_QUAD_MIN_ABOVE 100	 // view_floor_quad: a plane less far below the camera is seen edge on
static int arrival_frame = -1; // -1: not flying
static intro_shot arrival_from; // the spectator's camera when the loop broke: the first key
static world_point arrival_to;	// the cycle, at its start
static int arrival_fov;
static int arrival_fog_wait;
static int arrival_rise = ARRIVAL_RISE_FULL; // the walls: full height but while arriving
static bool arrival_low;					// among the walls: clipped as in 1st person
static int arrival_view_width;				// for that clipping: tan(half the FOV) x 16
static world_rect arrival_rect;				// up high: the walls round what's on the screen

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

	view_roll = 0;
	breach_dip = 0;
	breach_drop = 0;
	on_foot = false; // until the next view_on_foot
	look_pitch_target = 0;
	look_pitch_now = 0;
	foot_bob = 0;
	view_floor_shift = 0;
	if (arrival_frame >= 0)
	{
		// Coming down into the cycle: the flight sets the camera, from its next frame
		arrival_fov = -1;
		arrival_fog_wait = 0;
		view_follow(c);
		return;
	}
	if (spectating && intro_frame >= 0)
	{
		// Flying in: the flight sets the camera, its FOV and the floor's fog from its next frame
		intro_fov = -1;
		intro_fog_wait = 0;
		view_follow(c);
		return;
	}
	if (spectating)
	{
		jo_3d_perspective_angle(SPECTATE_FOV);
		view_z = -SPECTATE_HEIGHT;
		view_rx = SPECTATE_RX;
		view_rz = SPECTATE_RZ;
		view_x = spectate_pan_x + SPECTATE_LEAD_X + SPECTATE_BACK_X; // straight there
		view_y = spectate_line_y + SPECTATE_BACK_Y;
		view_floor_shift = SPECTATE_FLOOR_SHIFT;
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

	if (arrival_frame >= 0 && !free_camera)
	{
		world_point eye;

		if (!arrival_low)
			return arrival_rect; // worked out with the camera, each frame (arrival_footprint)
		// Among the walls: all round the camera's spot (it's turning), as far as range
		eye.x = (short)view_x;
		eye.y = (short)view_y;
		return view_cull_rect(eye, ((view_rz + 0x2000) >> 14) & 3, VIEW_CULL_TURNING, range);
	}
	if (spectating && !free_camera && intro_frame >= 0)
		return intro_rect; // worked out with the camera, each frame (intro_follow)
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
	else if (on_foot || heading_current != heading_target || look_now != 0 || camera_shake_active(&shake))
		mode = VIEW_CULL_TURNING; // on foot (any way at all), easing round, looking aside or shaking:
								  // it faces between headings
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

	if (arrival_frame >= 0)
	{
		// Coming down: only once it's among the walls, from its spot on the floor (view.c's top)
		if (free_camera || !arrival_low)
			return false;
		out->eye.x = (short)view_x;
		out->eye.y = (short)view_y;
		out->forward_x = -(slSin(view_rz) >> 6);
		out->forward_y = -(slCos(view_rz) >> 6);
		out->view_width = arrival_view_width;
		out->side_extra16 = 0;
		return true;
	}
	if (free_camera || spectating || !first_person)
		return false;
	out->eye = c->position;
	// Heading 0 faces north (-y) and turning right goes east (-x): forward = (-sin, -cos).
	// slSin/slCos give 16.16 fixed point; >> 6 makes it x1024
	out->forward_x = -(slSin(yaw) >> 6);
	out->forward_y = -(slCos(yaw) >> 6);
	out->view_width = view_width_now;
	out->side_extra16 = 0;
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

void view_on_foot(int yaw, int look_x, int look_y, int bob)
{
	on_foot = true;
	heading_target = yaw;
	heading_current = yaw; // no easing after it: the walker's turn is already eased
	look_target = look_x * LOOK_ANGLE;
	look_pitch_target = look_y > 0 ? -LOOK_UP_ANGLE : look_y < 0 ? LOOK_DOWN_ANGLE : 0;
	foot_bob = bob;
}

int view_look_pitch(void)
{
	return look_pitch_now;
}

int view_ground_height(void)
{
	// On foot you're in the corridor or beyond it: its floor is the glass (VDP1), and the grid's
	// only seen through the glass's seams, and below the city. Moved down at once as you get up
	return on_foot ? CITY_GROUND : 0;
}

void view_shake(int strength)
{
	camera_shake_start(&shake, strength);
}

void view_rumble(int strength)
{
	camera_shake_hold(&shake, strength);
}

void view_breach(int roll, int dip, int drop)
{
	view_roll = (ANGLE)roll;
	breach_dip = dip;
	breach_drop = drop;
}

void view_spectate_shift(int dx)
{
	view_x += dx;
	intro_start_x += dx;
	intro_track_x += dx;
}

void view_spectate_move(int dx, int dy)
{
	view_x += dx;
	view_y += dy;
	spectate_pan_x += dx;
	spectate_line_y += dy;
}

void view_spectate_line(int y)
{
	spectate_line_y = y;
}

void view_spectate_pan(int x)
{
	spectate_pan_x = x;
}

void view_intro_start(const cycle *c)
{
	intro_frame = 0;
	intro_start_x = c->position.x;
	intro_start_y = c->position.y;
	intro_track_x = c->position.x;
}

void view_intro_stop(void)
{
	intro_frame = -1;
	view_roll = 0;
}

bool view_intro_flying(void)
{
	return intro_frame >= 0;
}

int view_intro_frame(void)
{
	return intro_frame;
}

// What's drawn while flying: a box on the floor taking in the camera's spot and the point the
// middle of the screen looks at (height x tan(pitch) ahead), with a third of that distance (and a
// little) to spare all round. Near, steep views see little floor; looking out to the horizon,
// the letters close by are in it too; and at the seat it takes in the spectator's box (+-3000).
// The flight never has strokes behind the camera while it looks out level (they're all ahead,
// across the message), so the box reaching a little behind it can't hand VDP1 a point there
static void intro_view_rect(const intro_shot *s)
{
	int pitch = s->rx > INTRO_DEG(88) ? INTRO_DEG(88) : s->rx; // tan 90 is endless
	int reach = (s->height * (slTan((ANGLE)pitch) >> 4)) >> 12; // tan is 16.16: >> 4 then >> 12
	int half;
	int look_x;
	int look_y;

	if (reach > INTRO_VIEW_REACH_MAX)
		reach = INTRO_VIEW_REACH_MAX;
	half = INTRO_VIEW_HALF_MIN + ((reach * 85) >> 8); // 85 / 256: a third, with no division
	// Ahead is (-sin, -cos) (heading 0 is north, -y); slSin is 16.16, >> 6 makes it x1024
	look_x = view_x + ((-(slSin((ANGLE)s->rz) >> 6) * reach) >> 10);
	look_y = view_y + ((-(slCos((ANGLE)s->rz) >> 6) * reach) >> 10);
	intro_rect.x_min = (short)(JO_MIN(look_x, view_x) - half);
	intro_rect.x_max = (short)(JO_MAX(look_x, view_x) + half);
	intro_rect.y_min = (short)(JO_MIN(look_y, view_y) - half);
	intro_rect.y_max = (short)(JO_MAX(look_y, view_y) + half);
}

// The floor's tiles for a camera this high: doubling with height (INTRO_FLOOR_STEP_*)
static int floor_shift_for(int height)
{
	return height < INTRO_FLOOR_STEP_1 ? 0
		   : height < INTRO_FLOOR_STEP_2 ? 1
		   : height < INTRO_FLOOR_STEP_3 ? 2
		   : height < INTRO_FLOOR_STEP_4 ? SPECTATE_FLOOR_SHIFT
										 : SPECTATE_FLOOR_SHIFT + 1;
}

// The floor's fog for the camera now: its pitch below level, height and FOV (floor_fade.h)
static void intro_floor_fog(const intro_shot *s)
{
	int below = INTRO_DEG(90) - s->rx;
	int tilt;
	int focal;

	if (below > INTRO_DEG(60))
		below = INTRO_DEG(60); // steeper, the fog's far off the screen anyway (and the sums overflow)
	tilt = (slTan((ANGLE)below) * 1000) >> 16;
	focal = (JO_TV_WIDTH_2 << 16) / slTan((ANGLE)(s->fov >> 1)); // once a rebuild: a division
	floor_fade_set_camera(tilt, s->height, focal, s->fade_start, s->fade_end);
}

// One frame of the opening flight: the camera from the flight's keys, round the point it follows
static void intro_follow(const cycle *c, int yaw, int pitch)
{
	intro_shot s;
	int base_x;
	int base_y;

	intro_flight_at(intro_frame, &s);
	// The point it follows: from the cycle's start, held still, sliding over (as follow rises)
	// to a point easing after the cycle along the message's centre line, as the spectator does
	intro_track_x = ease_out_step(intro_track_x, c->position.x + s.lead, SPECTATE_PAN_SHARE, SPECTATE_PAN_MIN_STEP);
	base_x = intro_start_x + (((intro_track_x - intro_start_x) * s.follow) >> 8);
	// Over the last segment it slides from easing after the cycle onto the spectator's steady
	// point, so it lands panning with it. Smoothly: 3u^2 - 2u^3 of the way (u 0..1, here x256)
	if (intro_frame > INTRO_FLIGHT_FRAMES - INTRO_SEGMENT_FRAMES)
	{
		int u = (intro_frame - (INTRO_FLIGHT_FRAMES - INTRO_SEGMENT_FRAMES)) << (8 - INTRO_SEGMENT_SHIFT);
		int steady = (u * u * (768 - 2 * u)) >> 16; // 0..256

		base_x += ((spectate_pan_x + s.lead - base_x) * steady) >> 8;
	}
	base_y = intro_start_y + (((spectate_line_y - intro_start_y) * s.follow) >> 8);
	view_x = base_x + s.ox;
	view_y = base_y + s.oy;
	view_z = -s.height;
	view_rx = (ANGLE)(s.rx + pitch);
	view_rz = (ANGLE)(s.rz + yaw);
	{
		int below = INTRO_DEG(90) - s.rx; // how far below level it looks
		int lean = (below - INTRO_ROLL_FROM) >> INTRO_ROLL_SHIFT; // 0..256: how much of the bank shows

		lean = lean < 0 ? 0 : lean > 256 ? 256 : lean;
		view_roll = (ANGLE)((s.roll * lean) >> 8);
	}
	if (s.fov != intro_fov)
	{
		intro_fov = s.fov;
		slPerspective((ANGLE)s.fov);
	}
	view_floor_shift = floor_shift_for(s.height);
	intro_view_rect(&s);
	if (--intro_fog_wait <= 0)
	{
		intro_fog_wait = INTRO_FOG_FRAMES;
		intro_floor_fog(&s);
	}
	// Landed: the last frame was the spectator's view exactly, and from here it pans as usual
	if (++intro_frame > INTRO_FLIGHT_FRAMES)
	{
		intro_frame = -1;
		floor_fade_set_view(FLOOR_VIEW_SPECTATOR);
	}
}

void view_arrival_start(const cycle *c)
{
	int fade_start;
	int fade_end;

	// The spectator's camera as it stands (view_follow), but for the jolt of the press, which
	// carries on on top of the flight
	floor_fade_distances(FLOOR_VIEW_SPECTATOR, &fade_start, &fade_end);
	arrival_from.ox = spectate_pan_x + SPECTATE_LEAD_X + SPECTATE_BACK_X;
	arrival_from.oy = spectate_line_y + SPECTATE_BACK_Y;
	arrival_from.height = SPECTATE_HEIGHT;
	arrival_from.rx = SPECTATE_RX;
	arrival_from.rz = SPECTATE_RZ;
	arrival_from.roll = 0;
	arrival_from.fov = DEGtoANG(SPECTATE_FOV);
	arrival_from.lead = 0;
	arrival_from.follow = 0;
	arrival_from.fade_start = fade_start;
	arrival_from.fade_end = fade_end;
	arrival_to = c->position;
	arrival_frame = 0;
	arrival_rise = 0;
}

void view_arrival_stop(void)
{
	arrival_frame = -1;
	arrival_rise = ARRIVAL_RISE_FULL;
}

bool view_arriving(void)
{
	return arrival_frame >= 0;
}

int view_arrival_frame(void)
{
	return arrival_frame;
}

int view_wall_rise(void)
{
	return arrival_rise;
}

/*
 * Where the rays through the screen's four corners meet a level plane `plane` high (the floor is
 * 0), from a camera at s (its spot on the floor ox, oy, its height, pitch, turn and FOV). A ray
 * over the horizon counts out to ARRIVAL_VIEW_REACH along the floor. widen8: the screen's size in
 * eighths (8: the screen exactly; 9: an eighth wider all round). Writes the points in order round
 * them: bottom left, bottom right, top right, top left. 2 divisions (one per row of corners)
 *
 *   a ray, per unit along the line of sight:  down  = sin(below) - up x cos(below)
 *                                             ahead = cos(below) + up x sin(below)
 *   (up: how far up the screen, tan(half the FOV) x 0.7 at the top). It meets a plane `eye`
 *   below the camera after t = eye / down: ahead x t along the floor, and side x t to the side.
 */
static void corner_rays_meet(const intro_shot *s, int plane, int widen8, world_point *quad)
{
	int below = INTRO_DEG(90) - s->rx;
	int sin_d = slSin((ANGLE)below) >> 6; // x1024
	int cos_d = slCos((ANGLE)below) >> 6;
	int side = ((slTan((ANGLE)(s->fov >> 1)) >> 6) * widen8) >> 3; // sideways reach per unit ahead
	int up = (side * 179) >> 8;										// the screen is 224 / 320 = 0.7 as tall as wide
	// Ahead on the floor (-sin, -cos), and right: heading 0 faces north (-y), right is east (-x)
	int ahead_x = -(slSin((ANGLE)s->rz) >> 6);
	int ahead_y = -(slCos((ANGLE)s->rz) >> 6);
	int right_x = -(slCos((ANGLE)s->rz) >> 6);
	int right_y = slSin((ANGLE)s->rz) >> 6;
	int eye = s->height - plane;

	for (int v = -1; v <= 1; v += 2) // the bottom row, then the top
	{
		int down = (sin_d * 1024 - v * up * cos_d) >> 10;
		int ahead = (cos_d * 1024 + v * up * sin_d) >> 10;
		int ahead_size = ahead < 0 ? -ahead : ahead;
		int t;

		if (down <= 16 || eye * ahead_size > ARRIVAL_VIEW_REACH * down)
			t = (ARRIVAL_VIEW_REACH << 10) / (ahead_size > 16 ? ahead_size : 16); // over the horizon
		else
			t = (eye << 10) / down;
		for (int h = -1; h <= 1; h += 2)
		{
			int along = (ahead * t) >> 10;
			int across = (h * side * t) >> 10;
			// Bottom: left then right; top: right then left. So the four go round
			world_point *p = &quad[v < 0 ? (h < 0 ? 0 : 1) : (h > 0 ? 2 : 3)];
			int x = s->ox + ((along * ahead_x + across * right_x) >> 10);
			int y = s->oy + ((along * ahead_y + across * right_y) >> 10);

			p->x = (short)JO_MAX(JO_MIN(x, 32000), -32000);
			p->y = (short)JO_MAX(JO_MIN(y, 32000), -32000);
		}
	}
}

/*
 * Up high: the box on the floor round everything on the screen. The four rays through the
 * screen's corners are followed down to the floor, and to a second floor at the walls' tops (a
 * wall standing just off the bottom of the screen still shows its top); the box takes in all
 * eight points, ARRIVAL_VIEW_MARGIN more all round. 4 divisions, once a frame (not per wall piece)
 */
static void arrival_footprint(const intro_shot *s)
{
	int x_min = 32000, x_max = -32000, y_min = 32000, y_max = -32000;

	for (int plane = 0; plane < 2; ++plane)
	{
		world_point quad[4];
		int height = plane ? ARRIVAL_WALL_TOP : 0;

		if (s->height - height < 200)
			continue; // level with the wall tops, or under them: not up high
		corner_rays_meet(s, height, 8, quad);
		for (int i = 0; i < 4; ++i)
		{
			x_min = JO_MIN(x_min, quad[i].x);
			x_max = JO_MAX(x_max, quad[i].x);
			y_min = JO_MIN(y_min, quad[i].y);
			y_max = JO_MAX(y_max, quad[i].y);
		}
	}
	arrival_rect.x_min = (short)JO_MAX(x_min - ARRIVAL_VIEW_MARGIN, -32000);
	arrival_rect.x_max = (short)JO_MIN(x_max + ARRIVAL_VIEW_MARGIN, 32000);
	arrival_rect.y_min = (short)JO_MAX(y_min - ARRIVAL_VIEW_MARGIN, -32000);
	arrival_rect.y_max = (short)JO_MIN(y_max + ARRIVAL_VIEW_MARGIN, 32000);
}

bool view_floor_quad(int height, world_point *quad)
{
	intro_shot s;

	// Spectating, nothing is drawn with it (no walls); the opening flight keeps its own FOV
	if (spectating)
		return false;
	s.ox = view_x;
	s.oy = view_y;
	s.height = -view_z; // view_z moves the world: the camera's height is -view_z
	if (s.height - height < VIEW_QUAD_MIN_ABOVE)
		return false; // under the plane, or level with it: its top can't be seen
	s.rx = view_rx;
	s.rz = view_rz;
	// The FOV now: the flight's, the 1st person one (it widens with the boost; the debug camera
	// keeps the view's), or the iso view's
	s.fov = arrival_frame >= 0 ? arrival_fov : first_person ? fov_now : ISO_FOV_ANGLE;
	corner_rays_meet(&s, height, 9, quad);
	return true;
}

int view_sky_rise(void)
{
	// In 1st person only: the camera's tilt from level (+ down), as tan x 100 (the sky's 100
	// away, main.c), negative looking down: the horizon goes up the screen (SGL's y is down it),
	// and the sky with it
	const int tilt = FIRST_PERSON_RX - view_rx;

	if (!first_person || spectating || arrival_frame >= 0)
		return 0;
	return -((slTan((ANGLE)tilt) * 100) >> 16);
}

bool view_roof_quad(int height, world_point *quad)
{
	intro_shot s;

	if (spectating || -view_z <= height)
		return false; // not drawn spectating; or the eye's level with the roof or under it
	s.ox = view_x;
	s.oy = view_y;
	s.height = -view_z;
	s.rx = view_rx;
	s.rz = view_rz;
	s.fov = arrival_frame >= 0 ? arrival_fov : first_person ? fov_now : ISO_FOV_ANGLE;
	corner_rays_meet(&s, height, 10, quad); // a quarter wider all round, as view_ground_quad's
	return true;
}

bool view_ground_quad(world_point *quad)
{
	intro_shot s;

	if (spectating || view_z >= 0)
		return false; // not drawn spectating; or the eye's on the floor or under it
	s.ox = view_x;
	s.oy = view_y;
	s.height = -view_z;
	s.rx = view_rx;
	s.rz = view_rz;
	s.fov = arrival_frame >= 0 ? arrival_fov : first_person ? fov_now : ISO_FOV_ANGLE;
	// A quarter wider all round, not an eighth: the corners don't follow the camera's roll (the
	// designed crash leans it up to 11 degrees), so they're kept well off the screen
	corner_rays_meet(&s, 0, 10, quad);
	return true;
}

// One frame of the flight down into the cycle
static void arrival_follow(const cycle *c, int yaw, int pitch)
{
	intro_shot s;

	arrival_flight_at(&arrival_from, arrival_to, arrival_frame, &s);
	view_x = s.ox;
	view_y = s.oy;
	view_z = -s.height;
	view_rx = (ANGLE)(s.rx + pitch);
	view_rz = (ANGLE)(s.rz + yaw);
	if (s.fov != arrival_fov)
	{
		arrival_fov = s.fov;
		slPerspective((ANGLE)s.fov);
		arrival_view_width = (slTan((ANGLE)(s.fov >> 1)) * 16 + 0xFFFF) >> 16; // rounded up: errs wide
	}
	view_floor_shift = floor_shift_for(s.height);
	arrival_low = s.height < ARRIVAL_LOW_HEIGHT;
	if (!arrival_low)
		arrival_footprint(&s);
	arrival_rise = arrival_wall_rise(arrival_frame);
	if (--arrival_fog_wait <= 0)
	{
		// The floor's fog, and the walls' and trails' with it, over the flight's distances
		intro_shot floor = s;

		arrival_fog_wait = INTRO_FOG_FRAMES;
		// But the floor's is black by ARRIVAL_FLOOR_FOG_END: the raised ground round the arena is
		// only drawn out to ARRIVAL_VIEW_REACH ahead (view_floor_quad), and past it, up high, the
		// bare floor showed: a band of grid along the top of the screen. Nearer than that, all
		// that's floor is the arena's (its far corners are ~24000 off from the overview)
		if (floor.fade_end > ARRIVAL_FLOOR_FOG_END)
			floor.fade_end = ARRIVAL_FLOOR_FOG_END;
		if (floor.fade_start > floor.fade_end - ARRIVAL_FLOOR_FOG_MIN)
			floor.fade_start = floor.fade_end - ARRIVAL_FLOOR_FOG_MIN;
		intro_floor_fog(&floor);
		object_fog_set_distances(s.fade_start, s.fade_end);
	}
	// Landed: the last frame was the 1st person camera exactly. From here it's play
	if (++arrival_frame > ARRIVAL_FRAMES)
	{
		view_arrival_stop();
		apply_view(c);
	}
}

void view_follow(const cycle *c)
{
	int shake_pitch;

	// This frame's jolt (0, 0 when still), then it dies away a little
	camera_shake_step(&shake, &shake_yaw, &shake_pitch);
	if (arrival_frame >= 0)
		arrival_follow(c, shake_yaw, shake_pitch); // the last press's jolt at full size, as it began
	else if (spectating && intro_frame >= 0)
		intro_follow(c, shake_yaw, shake_pitch); // the jolt at full size, as the spectator has it
	else if (spectating)
	{
		// With the steady point along x, SPECTATE_BACK behind it; across from the line
		view_x = spectate_pan_x + SPECTATE_LEAD_X + SPECTATE_BACK_X;
		view_y = spectate_line_y + SPECTATE_BACK_Y;
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
		look_pitch_now = ease_out_step(look_pitch_now, look_pitch_target, LOOK_EASE_SHARE, LOOK_EASE_MIN_STEP);
		view_rz = (ANGLE)(heading_current + look_now + shake_yaw);
		{
			// The designed crash's tip forward is a dip of the head like any other, and so is
			// looking up or down on foot (negative: up). The fog follows them
			int pitch = (((look_now < 0 ? -look_now : look_now) * LOOK_PITCH_X256) >> 8) + breach_dip + look_pitch_now;
			int tilt = (slTan((ANGLE)pitch) * 1000) >> 16; // tan x 1000, as the floor fog keeps it

			// Less than level (90 degrees) looks down. The shake's pitch isn't in the fog's tilt
			view_rx = (ANGLE)(FIRST_PERSON_RX - pitch + shake_pitch);
			view_z = FIRST_PERSON_Z + breach_drop + foot_bob; // view_z moves the world: + lowers the eye
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
		ease_first_person_fov(on_foot ? ON_FOOT_FOV_ANGLE : FIRST_PERSON_FOV_ANGLE + ((c->boost * BOOST_FOV_ANGLE) >> 4));
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
	// The lean first: a turn about the line of sight (the opening flight's bank)
	if (view_roll != 0)
		slRotZ(view_roll);
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
