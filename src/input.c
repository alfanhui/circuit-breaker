#include "input.h"

jo_pos3Df pos;
jo_rot3Df rot;
float turn_angle = 1.5f; // 1.5f
float x_turn_angle = 0.2f; // 1.5f
float boundary = 0.020f;
bool boundary_enabled = false;

void gamepad_input(void)
{
	//	Poll for gamepad
	if (!jo_is_pad1_available())
		return ;

	static int movement_speed = 0;
	static float angle_increment = 0.0f;
	static float x_angle_increment = 0.0f;

	if (jo_is_pad1_key_pressed(JO_KEY_DOWN))
	{
		if ((movement_speed += 10) > 60)
			movement_speed = 60;
	}
	else if (jo_is_pad1_key_pressed(JO_KEY_UP))
	{
		if ((movement_speed -= 10) < -60)
			movement_speed = -60;
	}
	else if (movement_speed > 5)
		movement_speed -= 5;
	else if (movement_speed < -5)
		movement_speed += 5;
	else
		movement_speed = 0;

	if (jo_is_pad1_key_pressed(JO_KEY_RIGHT))
		angle_increment += turn_angle;
	else if (jo_is_pad1_key_pressed(JO_KEY_LEFT))
		angle_increment -= turn_angle;

	if (jo_is_pad1_key_pressed(JO_KEY_B)){
		x_angle_increment += x_turn_angle;
	}
	if (jo_is_pad1_key_pressed(JO_KEY_C) && rot.rx < 1073277126){
		x_angle_increment -= x_turn_angle;
	}

	// if(rot.rx < JO_DEG_TO_RAD(90) )
	//rot.rx = -JO_DEG_TO_RAD(x_angle_increment);
	
	if (jo_is_pad1_key_pressed(JO_KEY_L))
		pos.z += 1.0;
	else if (jo_is_pad1_key_pressed(JO_KEY_R))
		pos.z -= 1.0;

	rot.rz = JO_DEG_TO_RAD(angle_increment); /// 2.0;

	// angle_increment = angle_increment * 4.0 / 5.0;

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
		pos.x -= movement_speed * jo_sin_radf(rot.rz) / 10.0;
	};
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
		pos.y -= movement_speed * jo_cos_radf(rot.rz) / 10.0;
	}

	if( pos.z > -toFIXED(0.0001)){
		pos.z = -toFIXED(0.0001);
	}
}
