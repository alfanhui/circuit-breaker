#include "input.h"

jo_pos3Df pos;
jo_rot3Df rot;

const float turn_angle = 1.5f;	 // 1.5f
const float x_turn_angle = 0.2f; // 1.5f
const bool boundary_enabled = false;
const float boundary = 0.020f;
const int movement_incrementor = 5;
const int movement_max_level = 70;
const float boost_gauge_decrementer = 2.5f;
const float boost_gauge_incrementor = 0.5f;
const float boost_speed_decrementer = 0.2f;
const float boost_speed_incrementor = 0.5f;
const int boost_max_level = 100;

const float turn_incrementor = 0.15707f;
const float degree90_radian = 1.570796f;
const char compass[4] = "NESW";

static bool debug = true;
static int movement_speed = 0;
static float angle_increment = 0.0f;
static float x_angle_increment = 0.0f;
static int compass_index = 0;
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

void gamepad_input(void)
{
	//Debug settings
	debug_buttons();
	if (debug){
		return debug_controller();
	}

	//	Poll for gamepad
	if (!jo_is_pad1_available()){
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
			if (boost_movement < 1.75){
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

	//Gradual turning left mechanism
	if (turning_left && turn_left_target <= angle_increment)
	{
		angle_increment -= turn_incrementor;
	}
	else if (turning_left)
	{
		angle_increment = turn_left_target;
		rot.rz = turn_left_target;
		turning_left = false;
	}

	//Gradual turning right mechanism
	if (turning_right && turn_right_target >= angle_increment)
	{
		angle_increment += turn_incrementor;
	}
	else if (turning_right)
	{
		angle_increment = turn_right_target;
		rot.rz = turn_right_target;
		turning_right = false;
	}

	// Turning with compass setting
	if (is_key_struck(DIGI_LEFT) && !turning_right)
	{
		compass_index -= 1;
		if (compass_index < 0)
			compass_index = 3;
		turn_left_target = angle_increment - degree90_radian;
		turning_left = true;
	}
	else if (is_key_struck(DIGI_RIGHT) && !turning_left)
	{
		compass_index += 1;
		if (compass_index == 4)
			compass_index = 0;
		turn_right_target = angle_increment + degree90_radian;
		turning_right = true;
	}

	rot.rz = angle_increment;

	// Elevation TEST DEBUG ONLY
	if (jo_is_pad1_key_pressed(JO_KEY_L))
		pos.z += 1.0;
	else if (jo_is_pad1_key_pressed(JO_KEY_R))
		pos.z -= 1.0;
	// Elevation Floor boundary
	if (pos.z > -6.5536)
	{
		pos.z = -6.5536;
	}

	// Boundary and movement
	if (pos.x > toFIXED(boundary) && boundary_enabled)
	{
		pos.x = toFIXED(boundary);
	}
	else if (pos.x < -toFIXED(0.0015) && boundary_enabled)
	{
		pos.x = -toFIXED(0.0015);
	}
	else
	{
		// due to inaccuracies, do not use adjust x when going backwards (y axis only)
		if (compass_index != 2) // East
		{
			pos.x -= movement_speed * boost_movement * jo_sin_radf(rot.rz) / 10;
		}
	}
	if (pos.y > toFIXED(boundary) && boundary_enabled)
	{
		pos.y = toFIXED(boundary);
	}
	else if (pos.y < -toFIXED(0.0015) && boundary_enabled)
	{
		pos.y = -toFIXED(0.0015);
	}
	else
	{
		pos.y -= movement_speed * boost_movement * jo_cos_radf(rot.rz) / 10;
		// os.y -= movement_speed * jo_cos_radf_old(rot.rz) / 10;
	}
}