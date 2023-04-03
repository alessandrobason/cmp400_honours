#pragma once

#include "vec.h"

struct Camera {
	Camera();
	void update();
	void updateVectors();
	float getZoom() const;

	// no need to initialize them, they are generated from the angles below
	vec3 pos;
	vec3 fwd;
	vec3 up;
	vec3 right;

	vec2 angle = 0.f;
	float zoom_exp = 1.f;
};