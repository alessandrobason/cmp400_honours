#include <stdio.h>
#include <iostream>

#include "system.h"
#include "options.h"
#include "tracelog.h"
#include "widgets.h"
#include "shader.h"
#include "mesh.h"
#include "macros.h"

#include <imgui.h>

#include <array>

int main() {
	win::create("hello world", 800, 600);

	// push stack so stuff auto-cleans itself
	{
		Vertex verts[] = {
			{ { -1, -1, 0 }, col::red },
			{ {  3, -1, 0 }, col::green },
			{ { -1,  3, 0 }, col::blue },
		};

		Index indices[] = {
			0, 1, 2,
		};

		Mesh m;
		if (m.create(verts, indices)) {
			info("texture created");
		}

		Shader sh;
		if (sh.create("base_vs", "base_ps")) {
			info("shader loaded");
		}

		while (win::isOpen()) {
			win::poll();

			gfx::begin(col::dark_grey);

			gfx::main_rtv.bind();
			sh.update(matrix::identity, matrix::identity, matrix::identity);
			sh.bind();
			m.render();

			gfx::imgui_rtv.bind();

			fpsWidget();
			mainTargetWidget();

			ImGui::ShowDemoWindow();

			gfx::end();
		}
	}

	win::cleanup();
}