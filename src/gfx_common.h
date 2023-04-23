#pragma once

#include "common.h"
#include "d3d11_fwd.h"

enum class ShaderType : uint8_t {
	None,
	Vertex,
	Fragment,
	Compute,
};
