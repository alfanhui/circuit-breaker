#include "input.h"

jo_pos3Df pos;
jo_rot3Df rot;
float turn_angle = 1.5f; // 1.5f
float x_turn_angle = 0.2f; // 1.5f
float boundary = 0.020f;
bool boundary_enabled = false;

int movement_incrementor = 5;
int movement_max_level = 70;
float boost_gauge_decrementer = 2.5f;
float boost_gauge_incrementor = 0.5f;
float boost_speed_decrementer = 0.2f;
float boost_speed_incrementor = 0.5f;
int boost_max_level = 100;

char compass(int index)
{
	switch(index){
		case 0: return 'N';
		case 1: return 'E';
		case 2: return 'S';
		case 3: return 'W';
	}
}

void gamepad_input(void)
{
	static int movement_speed = 0;
	static float angle_increment = 0.0f;
	static float x_angle_increment = 0.0f;
	static int compass_index = 0;
	static float boost_gauge = 100.0f;
	static float boost_movement = 1.0f;

	jo_printf(20, 0, "Compass: %c", compass(compass_index));
	slPrint("Boost: ", slLocate(20,1));
	slPrintFX(boost_gauge, slLocate(27,1));
	//	Poll for gamepad
	if (!jo_is_pad1_available())
		return ;

	//Continuous Movement
	if (movement_speed > -movement_max_level)
	{
		movement_speed -= movement_incrementor;
	}
	else{
		movement_speed = -movement_max_level;
	}

	//Boost
	if (jo_is_pad1_key_pressed(JO_KEY_UP))
	{
		if(boost_gauge > 0.0f)
		{
			boost_gauge -= boost_gauge_decrementer;
			if(boost_movement < 1.75)
				boost_movement += (boost_speed_incrementor);
		} else {
			boost_movement = 1.0f;
		}
	} else{
		if( boost_movement > 1.0f){
			boost_movement -= (boost_speed_decrementer);
		}else {
			boost_movement = 1.0f;
		}
		if(boost_gauge < boost_max_level)
		{
			boost_gauge += boost_gauge_incrementor;
		}
	}

	//Turning with compass setting
	if (is_key_struck(DIGI_LEFT)){
		compass_index -= 1;
		if(compass_index< 0) compass_index= 3;
		angle_increment -= 1.570796f; //90degrees fixed
	}
	else if (is_key_struck(DIGI_RIGHT)){
		compass_index += 1;
		if(compass_index == 4) compass_index= 0;
		angle_increment += 1.570796f; //90degrees fixed
	}
	rot.rz = angle_increment;

	if (jo_is_pad1_key_pressed(JO_KEY_B)){
		x_angle_increment += x_turn_angle;
	}
	if (jo_is_pad1_key_pressed(JO_KEY_C) && rot.rx < 1073277126){
		x_angle_increment -= x_turn_angle;
	}

	//Pitch
	// if(rot.rx < JO_DEG_TO_RAD(90) )
	//rot.rx = -JO_DEG_TO_RAD(x_angle_increment);
	
	//Elevation TEST DEBUG ONLY
	if (jo_is_pad1_key_pressed(JO_KEY_L))
		pos.z += 1.0;
	else if (jo_is_pad1_key_pressed(JO_KEY_R))
		pos.z -= 1.0;
	// Elevation Floor boundary
	if( pos.z > -6.5536){
		pos.z = -6.5536;
	}

	//Boundary and movement
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
		if(compass_index != 2) //East
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
		//os.y -= movement_speed * jo_cos_radf_old(rot.rz) / 10;
	}
}