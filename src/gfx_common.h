#pragma once

#include "common.h"
#include "d3d11_fwd.h"

#define GFX_CLASS_CHECK(T) \
	static_assert((sizeof(T) % (sizeof(float) * 4)) == 0)

enum class ShaderType : uint8_t {
	None,
	Vertex,
	Fragment,
	Compute,
};
