#include "input.h"


typedef struct {
    jo_pos3Df pos;
	jo_rot3Df rot;
} GameObject;

GameObject lightcycle;

const float turn_angle = 1.5f;	 // 1.5f
const float x_turn_angle = 0.2f; // 1.5f
const bool boundary_enabled = false;
const float boundary = 0.020f;
const int movement_incrementor = 1;//5;
const int movement_max_level = 7;//70;
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
static int compass_index = 0;
static float boost_gauge = 100.0f;
static float boost_movement = 1.0f;
static float turn_left_target = 0.0f;
static float turn_right_target = 0.0f;
static bool turning_left = false;
static bool turning_right = false;

//
float speed_forward = 0.0f;     // Current forward/backward speed
float speed_strafe = 0.0f;      // Current left/right strafe speed
#define MAX_SPEED 5.0f        // Maximum movement speed
#define ACCELERATION 0.01f     // Speed increase per frame
#define DECELERATION 0.15f    // Speed decrease per frame
#define ROTATION_SPEED 2      // Rotation speed (degrees per frame)

// TEST DEBUG ONLY
void debug_buttons(void)
{
	jo_printf(20, 0, "Compass: %c", compass[compass_index]);
	slPrint("Boost: ", slLocate(20, 1));
	slPrintFX(boost_gauge, slLocate(27, 1));
	if (is_key_struck(JO_KEY_START))
	{
		debug = !debug;
		lightcycle.rot.rz = 0.0f;
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
		lightcycle.pos.z += 1.0;
	else if (jo_is_pad1_key_pressed(JO_KEY_R))
		lightcycle.pos.z -= 1.0;

	// Pitch
	if (jo_is_pad1_key_pressed(JO_KEY_B))
	{
		x_angle_increment += x_turn_angle;
		lightcycle.rot.rx = -JO_DEG_TO_RAD(x_angle_increment);
	}
	if (jo_is_pad1_key_pressed(JO_KEY_C) && lightcycle.rot.rx < 1073277126)
	{
		x_angle_increment -= x_turn_angle;
		lightcycle.rot.rx = -JO_DEG_TO_RAD(x_angle_increment);
	}

	lightcycle.rot.rz += JO_DEG_TO_RAD(angle_increment) / 2.0;
	angle_increment = angle_increment * 4.0 / 5.0;

	lightcycle.pos.x -= movement_speed * jo_sin_radf(lightcycle.rot.rz) / 10.0;
	lightcycle.pos.y -= movement_speed * jo_cos_radf(lightcycle.rot.rz) / 10.0;
}

float clamp(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

void gamepad_input3(void){
	float movement_speed = 0.0f;
	if (jo_is_pad1_key_pressed(JO_KEY_LEFT)){
		lightcycle.rot.rz += 1;
	}
 	if (jo_is_pad1_key_pressed(JO_KEY_RIGHT)){
		lightcycle.rot.rz -= 1;
	}
	if (jo_is_pad1_key_pressed(JO_KEY_UP)){
		movement_speed -= 0.1f;
	}
	else if (jo_is_pad1_key_pressed(JO_KEY_DOWN)){
		movement_speed += 0.1f;
	}

	lightcycle.pos.x -= movement_speed * jo_sin_radf(lightcycle.rot.rz) / 10.0;
	lightcycle.pos.y -= movement_speed * jo_cos_radf(lightcycle.rot.rz) / 10.0;
}

void gamepad_input2(void){
	 // Handle rotation (yaw)
	if (jo_is_pad1_key_pressed(JO_KEY_LEFT))
		lightcycle.rot.ry += ROTATION_SPEED;
 	if (jo_is_pad1_key_pressed(JO_KEY_RIGHT))
 		lightcycle.rot.ry -= ROTATION_SPEED;

 // Normalize angle (0–360 degrees)
 if (lightcycle.rot.ry >= 360) lightcycle.rot.ry -= 360;
 if (lightcycle.rot.ry < 0) lightcycle.rot.ry += 360;

 // Handle acceleration for forward/backward
 if (jo_is_pad1_key_pressed(JO_KEY_UP))
	 speed_forward += ACCELERATION; // Accelerate forward
 else if (jo_is_pad1_key_pressed(JO_KEY_DOWN))
	 speed_forward -= ACCELERATION; // Accelerate backward
 else
 {
	 // Decelerate forward/backward
	 if (speed_forward > 0)
		 speed_forward = JO_MAX(0, speed_forward - DECELERATION);
	 else if (speed_forward < 0)
		 speed_forward = JO_MIN(0, speed_forward + DECELERATION);
 }

 // Handle acceleration for strafing
 if (jo_is_pad1_key_pressed(JO_KEY_L))
	 speed_strafe -= ACCELERATION; // Accelerate left
 else if (jo_is_pad1_key_pressed(JO_KEY_R))
	 speed_strafe += ACCELERATION; // Accelerate right
 else
 {
	 // Decelerate strafing
	 if (speed_strafe > 0)
		 speed_strafe = JO_MIN(0, speed_strafe - DECELERATION);
	 else if (speed_strafe < 0)
		 speed_strafe = JO_MAX(0, speed_strafe + DECELERATION);
 }

 // Clamp speeds to max
 speed_forward = clamp(speed_forward, -MAX_SPEED, MAX_SPEED);
 speed_strafe = clamp(speed_strafe, -MAX_SPEED, MAX_SPEED);

 // Calculate movement vector using trigonometry (Jo Engine uses fixed-point)
 int angle_rad = toFIXED(lightcycle.rot.ry); // Convert degrees to Jo Engine's fixed-point angle
 float cos_val = jo_cos(angle_rad) / 4096.0f; // Cosine for forward/backward
 float sin_val = jo_sin(angle_rad) / 4096.0f; // Sine for strafing

 // Update player position
 lightcycle.pos.x += speed_forward * cos_val + speed_strafe * sin_val; // Forward + strafe (X)
 lightcycle.pos.y += speed_forward * -sin_val + speed_strafe * cos_val; // Forward + strafe (Z)

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
		lightcycle.rot.rz = turn_left_target;
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
		lightcycle.rot.rz = turn_right_target;
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

	lightcycle.rot.rz = angle_increment;

	// Elevation TEST DEBUG ONLY
	if (jo_is_pad1_key_pressed(JO_KEY_L))
		lightcycle.pos.z += 1.0;
	else if (jo_is_pad1_key_pressed(JO_KEY_R))
		lightcycle.pos.z -= 1.0;
	// Elevation Floor boundary
	if (lightcycle.pos.z > -6.5536)
	{
		lightcycle.pos.z = -6.5536;
	}

	// Boundary and movement
	if (lightcycle.pos.x > toFIXED(boundary) && boundary_enabled)
	{
		lightcycle.pos.x = toFIXED(boundary);
	}
	else if (lightcycle.pos.x < -toFIXED(0.0015) && boundary_enabled)
	{
		lightcycle.pos.x = -toFIXED(0.0015);
	}
	else
	{
		// due to inaccuracies, do not use adjust x when going backwards (y axis only)
		if (compass_index != 2) // East
		{
			lightcycle.pos.x -= movement_speed * boost_movement * jo_sin_radf(lightcycle.rot.rz) / 10;
		}
	}
	if (lightcycle.pos.y > toFIXED(boundary) && boundary_enabled)
	{
		lightcycle.pos.y = toFIXED(boundary);
	}
	else if (lightcycle.pos.y < -toFIXED(0.0015) && boundary_enabled)
	{
		lightcycle.pos.y = -toFIXED(0.0015);
	}
	else
	{
		lightcycle.pos.y -= movement_speed * boost_movement * jo_cos_radf(lightcycle.rot.rz) / 10;
		// os.y -= movement_speed * jo_cos_radf_old(rot.rz) / 10;
	}
}