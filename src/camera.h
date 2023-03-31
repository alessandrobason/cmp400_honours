#pragma once

#include "vec.h"
#include <string.h>

struct matrix {
	matrix() = default;
	matrix(float* m) { memcpy(data, m, sizeof(data)); }
	float data[16];
};

struct Camera {
	//vec3 pos   = vec3(0, 0, -100);
	//vec3 fwd   = vec3(0, 0, 1);
	//vec3 right = vec3(1, 0, 0);
	//vec3 up    = vec3(0, 1, 0);

	vec3 fwd = vec3(0, 0, 1);
	vec3 right = vec3(-1, 0, 0);

	vec3 pos = vec3(0, 0, -200);
	vec3 target = vec3(0, 0, 0);
	vec3 up = vec3(0, 1, 0);
	matrix view;

	void update();
	void updateView(const vec3& eye, const vec3& lookat, const vec3& up);
};