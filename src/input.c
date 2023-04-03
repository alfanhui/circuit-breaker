#include "input.h"

jo_pos3Df pos;
jo_rot3Df rot;
int compass_index = 0;
int previous_compass_index = 0;

const float turn_angle = 1.5f;	 // 1.5f
const float x_turn_angle = 0.2f; // 1.5f

// Boundary
const bool boundary_enabled = true;
const jo_fixed upper_boundary = toFIXED(0.037f); //TODO this correlates to walls
const jo_fixed lower_boundary = toFIXED(0.01f); //TODO this correlates to walls
const jo_fixed flour_boundary = toFIXED(-0.0001f);
static bool at_x_boundary = false;
static bool at_y_boundary = false;

const int movement_incrementor = 5;
const int movement_max_level = 70;

// boost
const float boost_gauge_decrementer = 2.5f;
const float boost_gauge_incrementor = 0.5f;
const float boost_speed_decrementer = 0.2f;
const float boost_speed_incrementor = 0.5f;
const int boost_max_level = 100;

const float turn_incrementor = 0.15707f;
const float degree90_radian = 1.570796f;
const char compass[4] = "NESW";

static bool debug = false;
static int movement_speed = 0;
static float angle_increment = 0.0f;
static float x_angle_increment = 0.0f;
static float boost_gauge = 100.0f;
static float boost_movement = 1.0f;
static float turn_left_target = 0.0f;
static float turn_right_target = 0.0f;
static bool turning_left = false;
static bool turning_right = false;

// TEST DEBUG ONLY
void debug_buttons(void)
{
	jo_printf(20, 0, "Compass: %c", compass[compass_index]);
	slPrint("Boost: ", slLocate(20, 1));
	slPrintFX(boost_gauge, slLocate(27, 1));
	if (is_key_struck(JO_KEY_START))
	{
		debug = !debug;
		rot.rz = 0.0f;
	}
}

// TEST DEBUG ONLY
void debug_controller(void)
{
	if (jo_is_pad1_key_pressed(JO_KEY_UP))
	{
		if ((movement_speed -= 10) < -60)
			movement_speed = -60;
	}
	else if (jo_is_pad1_key_pressed(JO_KEY_DOWN))
	{
		if ((movement_speed += 10) > 60)
			movement_speed = 60;
	}
	else if (movement_speed > 5)
		movement_speed -= 5;
	else if (movement_speed < -5)
		movement_speed += 5;
	else
		movement_speed = 0;

	if (jo_is_pad1_key_pressed(JO_KEY_RIGHT))
		angle_increment += 1.5;
	else if (jo_is_pad1_key_pressed(JO_KEY_LEFT))
		angle_increment -= 1.5;

	if (jo_is_pad1_key_pressed(JO_KEY_L))
		pos.z += 1.0;
	else if (jo_is_pad1_key_pressed(JO_KEY_R))
		pos.z -= 1.0;

	// Pitch
	if (jo_is_pad1_key_pressed(JO_KEY_B))
	{
		x_angle_increment += x_turn_angle;
		rot.rx = -JO_DEG_TO_RAD(x_angle_increment);
	}
	if (jo_is_pad1_key_pressed(JO_KEY_C) && rot.rx < 1073277126)
	{
		x_angle_increment -= x_turn_angle;
		rot.rx = -JO_DEG_TO_RAD(x_angle_increment);
	}

	rot.rz += JO_DEG_TO_RAD(angle_increment) / 2.0;
	angle_increment = angle_increment * 4.0 / 5.0;

	pos.x -= movement_speed * jo_sin_radf(rot.rz) / 10.0;
	pos.y -= movement_speed * jo_cos_radf(rot.rz) / 10.0;
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

void gamepad_input(void)
{
	// Debug settings
	debug_buttons();
	if (debug)
	{
		return debug_controller();
	}

	//	Poll for gamepad
	if (!jo_is_pad1_available())
	{
		return;
	}

	// Continuous Movement
	if (movement_speed > -movement_max_level)
	{
		movement_speed -= movement_incrementor;
	}
	else
	{
		movement_speed = -movement_max_level;
	}

	// Boost
	if (jo_is_pad1_key_pressed(JO_KEY_UP))
	{
		if (boost_gauge > 0.0f)
		{
			boost_gauge -= boost_gauge_decrementer;
			if (boost_movement < 1.75)
			{
				boost_movement += (boost_speed_incrementor);
			}
		}
		else
		{
			boost_movement = 1.0f;
		}
	}
	else
	{
		if (boost_movement > 1.0f)
		{
			boost_movement -= (boost_speed_decrementer);
		}
		else
		{
			boost_movement = 1.0f;
		}
		if (boost_gauge < boost_max_level)
		{
			boost_gauge += boost_gauge_incrementor;
		}
	}

	rot.rz = angle_increment;

	// Turning with compass setting
	// Gradual turning left mechanism
	if (turning_left && turn_left_target <= angle_increment)
	{
		angle_increment -= turn_incrementor;
	}
	else if (turning_left)
	{
		angle_increment = turn_left_target;
		rot.rz = turn_left_target;
		turn_left_target = 0;
		turning_left = false;
		// trail stuff
		player1_previous_coordinate.x = pos.x;
		player1_previous_coordinate.y = pos.y;
	}
	else if (is_key_struck(DIGI_LEFT) && !turning_right)
	{
		previous_compass_index = compass_index;
		compass_index -= 1;
		if (compass_index < 0)
			compass_index = 3;
		turn_left_target = angle_increment - degree90_radian;
		turning_left = true;
		// trigger trail creation
		create_trail = true;
	}

	// Gradual turning right mechanism
	if (turning_right && turn_right_target >= angle_increment)
	{
		angle_increment += turn_incrementor;
	}
	else if (turning_right)
	{
		angle_increment = turn_right_target;
		rot.rz = turn_right_target;
		turn_right_target = 0;
		turning_right = false;
		// trail stuff
		player1_previous_coordinate.x = pos.x;
		player1_previous_coordinate.y = pos.y;
	}
	else if (is_key_struck(DIGI_RIGHT) && !turning_left)
	{
		previous_compass_index = compass_index;
		compass_index += 1;
		if (compass_index == 4)
			compass_index = 0;
		turn_right_target = angle_increment + degree90_radian;
		turning_right = true;
		// trigger trail creation
		create_trail = true;
	}

	// Elevation TEST DEBUG ONLY
	if (jo_is_pad1_key_pressed(JO_KEY_L))
		pos.z += 1.0;
	else if (jo_is_pad1_key_pressed(JO_KEY_R))
		pos.z -= 1.0;
	// Elevation Floor boundary
	if (pos.z >= flour_boundary)
	{
		pos.z = flour_boundary;
	}

	// Boundary and movement
	if (boundary_enabled){
		if( pos.x >= upper_boundary && !at_x_boundary)
		{
			pos.x = upper_boundary;
			at_x_boundary = true;
		}
		else if (pos.x <= lower_boundary && !at_x_boundary)
		{
			pos.x = lower_boundary;
			at_x_boundary = true;
		}

		if (pos.y >= upper_boundary && !at_y_boundary)
		{
			pos.y = upper_boundary;
			at_y_boundary = true;
		}
		else if (pos.y <= lower_boundary && !at_y_boundary)
		{
			pos.y = lower_boundary;
			at_y_boundary = true;
		}
	}
	
	if (compass_index == 1 || compass_index == 3)
	{
		jo_fixed x = pos.x - movement_speed * boost_movement * jo_sin_radf(rot.rz) / 10;
		if (!boundary_enabled)
		{
			pos.x = x;
		}
		else if (x > lower_boundary && x < upper_boundary)
		{
			at_x_boundary = false;
			pos.x = x;
		}
	}else {
		jo_fixed y = pos.y - movement_speed * boost_movement * jo_cos_radf(rot.rz) / 10;
		if (!boundary_enabled)
		{
			pos.y = y;
		}
		else if (y > lower_boundary && y < upper_boundary)
		{
			at_y_boundary = false;
			pos.y = y;
		}
	}
}