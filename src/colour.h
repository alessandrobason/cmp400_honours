#pragma once

#include "vec.h"

struct Colour : public vec4 {
	using vec4::vec4;
	Colour(const vec4 &vec) : vec4(vec) {}

	static const Colour black;
	static const Colour white;

	static const Colour red;
	static const Colour green;
	static const Colour blue;

	static const Colour cyan;
	static const Colour magenta;
	static const Colour yellow;

	static const Colour crimson;
	static const Colour pink;
	static const Colour coral;
	static const Colour peach;
	static const Colour violet;
	static const Colour indigo;
	static const Colour lime;
	static const Colour olive;
	static const Colour teal;
	static const Colour sky;
	static const Colour navy;
	static const Colour brown;
	static const Colour maroon;
	static const Colour beige;
	static const Colour rose;
	static const Colour orange;
	static const Colour purple;
	static const Colour silver;
	static const Colour dark_grey;
	static const Colour turquoise;
};
