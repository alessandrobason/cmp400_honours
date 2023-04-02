#pragma once

#include "vec.h"

struct Camera {
	Camera();
	void update();
	void updateVectors();

	// no need to initialize them, the are generated from the angles below
	vec3 pos;
	vec3 fwd;
	vec3 up;
	vec3 right;

	vec2 angle = 0.f;
	float zoom = 1.f;
};