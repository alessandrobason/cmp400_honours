#include "widgets.h"

#include <imgui.h>

#include "system.h"
#include "tracelog.h"

void fpsWidget() {
	constexpr float PAD = 10.0f;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
	ImGuiViewport *viewport = ImGui::GetMainViewport();
	vec2 work_pos = viewport->WorkPos;
	vec2 work_size = viewport->WorkSize;
	vec2 window_pos = {
		work_pos.x + PAD,
		work_pos.y + work_size.y - PAD
	};
	vec2 window_pos_pivot = { 0.f, 1.f };
	ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::SetNextWindowBgAlpha(0.35f);

	ImGui::Begin("FPS overlay", nullptr, window_flags);
	ImGui::Text("FPS: %.0f", roundf(win::fps));
	ImGui::End();
}

void mainTargetWidget() {
	ImGui::Begin("Game");

	vec2 img_size = gfx::main_rtv.size;
	vec2 win_size = ImGui::GetWindowContentRegionMax();
	vec2 win_padding = ImGui::GetWindowContentRegionMin();

	win_size -= win_padding;

#if RESIZE_RENDER_TARGET
	vec2 size_diff = img_size - win_size;
	if (fabsf(size_diff.x) > 0.1f || fabsf(size_diff.y) > 0.1f) {
		info("resize");
		main_rtv.resize((int)win_size.x, (int)win_size.y);
	}
#endif

	float dx = win_size.x / img_size.x;
	float dy = win_size.y / img_size.y;

	if (dx > dy) {
		img_size.x *= dy;
		img_size.y = win_size.y;
	}
	else {
		img_size.x = win_size.x;
		img_size.y *= dx;
	}

	ImGui::SetCursorPos((win_size - img_size) * 0.5f + win_padding);
	ImGui::Image((ImTextureID)gfx::main_rtv.resource, img_size);
	ImGui::End();
}
