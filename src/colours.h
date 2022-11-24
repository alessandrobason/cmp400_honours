#pragma once

#include "vec.h"

using Colour = vec4;

namespace col {
	constexpr Colour black       = { 0, 0, 0, 1 };
	constexpr Colour white       = { 1, 1, 1, 1 };

	constexpr Colour red         = { 1, 0, 0, 1 };
	constexpr Colour green       = { 0, 1, 0, 1 };
	constexpr Colour blue        = { 0, 0, 1, 1 };

	constexpr Colour cyan        = { 0, 1, 1, 1 };
	constexpr Colour magenta     = { 1, 0, 1, 1 };
	constexpr Colour yellow      = { 1, 1, 0, 1 };
	
	constexpr Colour crimson     = { 0.86f, 0.08f, 0.24f, 1 };
	constexpr Colour pink        = { 1.00f, 0.75f, 0.80f, 1 };
	constexpr Colour coral       = { 1.00f, 0.50f, 0.31f, 1 };
	constexpr Colour peach       = { 1.00f, 0.85f, 0.73f, 1 };
	constexpr Colour violet      = { 0.93f, 0.51f, 0.93f, 1 };
	constexpr Colour indigo      = { 0.29f, 0.00f, 0.51f, 1 };
	constexpr Colour lime        = { 0.20f, 0.80f, 0.20f, 1 };
	constexpr Colour olive       = { 0.50f, 0.50f, 0.00f, 1 };
	constexpr Colour teal        = { 0.03f, 0.00f, 0.50f, 1 };
	constexpr Colour sky         = { 0.53f, 0.81f, 0.98f, 1 };
	constexpr Colour navy        = { 0.00f, 0.00f, 0.50f, 1 };
	constexpr Colour brown       = { 0.74f, 0.56f, 0.56f, 1 };
	constexpr Colour maroon      = { 0.50f, 0.00f, 0.00f, 1 };
	constexpr Colour beige       = { 0.96f, 0.96f, 0.86f, 1 };
	constexpr Colour rose        = { 1.00f, 0.89f, 0.88f, 1 };
	constexpr Colour orange      = { 1.00f, 0.65f, 0.00f, 1 };
	constexpr Colour purple      = { 0.50f, 0.00f, 0.50f, 1 };
	constexpr Colour silver      = { 0.75f, 0.75f, 0.75f, 1 };
	constexpr Colour dark_grey   = { 0.10f, 0.10f, 0.10f, 1 };
	constexpr Colour turquoise   = { 0.25f, 0.88f, 0.82f, 1 };
} // namespace col
