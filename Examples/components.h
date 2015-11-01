#pragma once
#include <aurumecs/component.h>

struct TransformComponent {
	COMPONENT_INFO(Transform, 0);

	float Position[3];
	float Rotation[3];
	float Velocity[3];
	float AngularVelocity[3];

	static TransformComponent Create()
	{
		TransformComponent c = {};
		return c;
	}

	void Destroy()
	{
	}
};

struct RandomThingComponent {
	COMPONENT_INFO(RandomThing, 1);

	int RandomThing;

	static RandomThingComponent Create()
	{
		RandomThingComponent c = {};
		return c;
	}

	void Destroy()
	{
	}
};
