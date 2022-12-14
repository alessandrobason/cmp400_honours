#include <stdio.h>
#include <iostream>

#include "system.h"
#include "options.h"
#include "tracelog.h"
#include "widgets.h"
#include "shader.h"
#include "mesh.h"
#include "macros.h"
#include "timer.h"

#include <imgui.h>

#include <array>

int main() {
	win::create("hello world", 800, 600);

	// push stack so stuff auto-cleans itself
	{
		Vertex verts[] = {
			{ { -1, -1, 0 }, Colour::red },
			{ {  3, -1, 0 }, Colour::green },
			{ { -1,  3, 0 }, Colour::blue },
		};

		Index indices[] = {
			0, 2, 1,
		};

		Mesh m;
		if (m.create(verts, indices)) {
			info("texture created");
		}

		Shader sh;
		if (!sh.create("base_vs", "base_ps")) {
			exit(1);
		}

		//sh.update(matrix::identity, matrix::identity, matrix::identity);

		while (win::isOpen()) {
			win::poll();

			gfx::begin(Colour::dark_grey);
				gfx::main_rtv.bind();
					sh.bind();
						m.render();
					sh.unbind();
				gfx::imgui_rtv.bind();
					fpsWidget();
					mainTargetWidget();
					ImGui::ShowDemoWindow();
			gfx::end();

#ifndef NDEBUG
			// take a one screenshot every time we open the application, this way we can have
			// a history of the development
			static OnceClock screenshot_clock;
			if (screenshot_clock.after(0.1f)) {
				gfx::imgui_rtv.takeScreenshot("automatic_screen");
			}
#endif

			if (ImGui::IsKeyPressed(ImGuiKey_P)) {
				gfx::imgui_rtv.takeScreenshot();
			}

			gfx::logD3D11messages();
		}
	}

	win::cleanup();
}