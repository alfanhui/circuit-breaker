#include "input.h"

//Globals
jo_pos3D pos = {1500, 850, -35};
jo_pos3D true_position;
jo_rot3Df rot;

jo_pos3D position_1st_person = {1000, 675, -35};
jo_rot3Df rotation_1st_person = {JO_DEG_TO_RAD(90), JO_DEG_TO_RAD(0), JO_DEG_TO_RAD(-1.0)};

jo_pos3D position_3rd_person = {200, 200, -768};
jo_rot3Df rotation_3rd_person = {JO_DEG_TO_RAD(40), JO_DEG_TO_RAD(0), JO_DEG_TO_RAD(45)};

int compass_index = 0;
int previous_compass_index = 0;
const float degree90_radian = 1.570796f;
const char compass[4] = "NESW";

const float turn_angle = 1.5f;
const float x_turn_angle = 0.2f;
const float turn_incrementor = 0.15707f;

// Boundary
const bool boundary_enabled = true;
jo_fixed upper_boundary;
jo_fixed lower_boundary;
const jo_fixed floor_boundary = toFIXED(-0.0001f);
const jo_fixed upper_1st_boundary = toFIXED(0.037f);
const jo_fixed lower_1st_boundary = toFIXED(0.01f);
const jo_fixed upper_3rd_boundary = toFIXED(0.026f);
const jo_fixed lower_3rd_boundary = toFIXED(0.0035f);
static bool at_x_boundary = false;
static bool at_y_boundary = false;

// boost
const float boost_gauge_decrementer = 2.5f;
const float boost_gauge_incrementor = 0.5f;
const float boost_speed_decrementer = 0.2f;
const float boost_speed_incrementor = 0.5f;
const int boost_max_level = 100;

static bool debug = false;
const int movement_incrementor = 5; //0 to stop, 5 go
const int movement_max_level = 70;
static int movement_speed = 5;
static float angle_increment = 0.0f;
static float x_angle_increment = 0.0f;
static float boost_gauge = 100.0f;
static float boost_movement = 1.0f;
static float turn_left_target = 0.0f;
static float turn_right_target = 0.0f;
static bool turning_left = false;
static bool turning_right = false;

float rotation_z = 0.0f;

void init_perspective_locations(void){
	if(first_person){
		jo_3d_perspective_angle(90);
		true_position.x = 2000;
		true_position.y = 850;
		true_position.z = 0;
		pos.x = position_1st_person.x;
		pos.y = position_1st_person.y;
		pos.z = position_1st_person.z;
		rot.rx = rotation_1st_person.rx;
		rot.ry = rotation_1st_person.ry;
		rot.rz = rotation_1st_person.rz;
		lower_boundary = lower_1st_boundary;
		upper_boundary = upper_1st_boundary;
	}else {
		jo_3d_perspective_angle(60);
		true_position.x = 1000;
		true_position.y = 850;
		true_position.z = position_3rd_person.z;
		pos.x = position_3rd_person.x;
		pos.y = position_3rd_person.y;
		pos.z = position_3rd_person.z;
		rot.rx = rotation_3rd_person.rx;
		rot.ry = rotation_3rd_person.ry;
		rot.rz = rotation_3rd_person.rz;
		lower_boundary = lower_3rd_boundary;
		upper_boundary = upper_3rd_boundary;
	}
}

// TEST DEBUG ONLY
void debug_buttons(void)
{
	jo_printf(20, 0, "Compass: %c", compass[compass_index]);
	slPrint("Boost: ", slLocate(20, 1));
	slPrintFX(boost_gauge, slLocate(27, 1));
	if (is_key_struck(JO_KEY_START))
	{
		debug = !debug;
		rotation_z = 0.0f;
	}

	// Elevation TEST DEBUG ONLY
	if (jo_is_pad1_key_pressed(JO_KEY_L))
		pos.z += 1.0;
	else if (jo_is_pad1_key_pressed(JO_KEY_R))
		pos.z -= 1.0;
	// Elevation Floor boundary
	if (pos.z >= floor_boundary)
	{
		pos.z = floor_boundary;
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

	rotation_z += JO_DEG_TO_RAD(angle_increment) / 2.0;
	angle_increment = angle_increment * 4.0 / 5.0;

	pos.x -= movement_speed * jo_sin_radf(rotation_z) / 10.0;
	pos.y -= movement_speed * jo_cos_radf(rotation_z) / 10.0;
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


/*
When resetting rotation, it jumps (do a 360 to see in 3rd person)
*/
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

	if (is_key_struck(DIGI_DOWN)){
		if(!first_person){
			first_person = true;
			jo_3d_perspective_angle(90);
			// Coordinate map is different scale, adjust accordingly
			pos.x = linearly_constrain_to_1st(pos.x);
			pos.y = linearly_constrain_to_1st(pos.y);
			pos.z = position_1st_person.z;
			rot.rx = rotation_1st_person.rx;
			rot.ry = rotation_1st_person.ry;
			rot.rz = rotation_z;
			lower_boundary = lower_1st_boundary;
			upper_boundary = upper_1st_boundary;
		}else{
			first_person = false;
			jo_3d_perspective_angle(60);
			// Coordinate map is different scale, adjust accordingly
			pos.x = linearly_constrain_to_3rd(pos.x);
			pos.y = linearly_constrain_to_3rd(pos.y);
			pos.z = position_3rd_person.z;
			rot.rx = rotation_3rd_person.rx;
			rot.ry = rotation_3rd_person.ry;
			rot.rz = rotation_3rd_person.rz;
			true_position.z = position_3rd_person.z;
			lower_boundary = lower_3rd_boundary;
			upper_boundary = upper_3rd_boundary;
		}
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

	// Turning with compass setting
	// Gradual turning left mechanism
	if (turning_left && turn_left_target <= angle_increment)
	{
		angle_increment -= turn_incrementor;
	}
	else if (turning_left)
	{
		angle_increment = turn_left_target;
		rotation_z = turn_left_target;
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
		rotation_z = turn_right_target;
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
		jo_fixed x = pos.x - movement_speed * boost_movement * jo_sin_radf(rotation_z) / 10;
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
		jo_fixed y = pos.y - movement_speed * boost_movement * jo_cos_radf(rotation_z) / 10;
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

	rotation_z = angle_increment;

	true_position.x = (-pos.x + 10) * .2f; // .2f is scale speed, +10 is to account for player box boundary 
	true_position.y = (-pos.y + 10) * .2f; // .2f is scale speed, +10 is to account for player box boundary 

	if(first_person){
		rot.rz = angle_increment;
	}else{
		true_position.z = position_3rd_person.z;
	}
}

int linearly_constrain_to_1st(int value) {
    // Calculate the slope and intercept of the linear function
    float slope = (float)(upper_1st_boundary - lower_1st_boundary) / (upper_3rd_boundary - lower_3rd_boundary);
    float intercept = lower_1st_boundary - slope * lower_3rd_boundary;

    // Apply the linear function to constrain the value
    int constrainedValue = (int)(slope * value + intercept);

    // Check if the constrained value is outside the constrained range
    if (constrainedValue < lower_1st_boundary) {
        constrainedValue = lower_1st_boundary;
    } else if (constrainedValue > upper_1st_boundary) {
        constrainedValue = upper_1st_boundary;
    }

    return constrainedValue;
}

int linearly_constrain_to_3rd(int value) {
    // Calculate the slope and intercept of the linear function
    float slope = (float)(upper_3rd_boundary - lower_3rd_boundary) / (upper_1st_boundary - lower_1st_boundary);
    float intercept = lower_3rd_boundary - slope * lower_1st_boundary;

    // Apply the linear function to constrain the value
    int constrainedValue = (int)(slope * value + intercept);

    // Check if the constrained value is outside the constrained range
    if (constrainedValue < lower_3rd_boundary) {
        constrainedValue = lower_3rd_boundary;
    } else if (constrainedValue > upper_3rd_boundary) {
        constrainedValue = upper_3rd_boundary;
    }

    return constrainedValue;
}