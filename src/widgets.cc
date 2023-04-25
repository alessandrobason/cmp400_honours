#include "widgets.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <d3d11.h>
#include <nfd.hpp>

#include "system.h"
#include "tracelog.h"
#include "shader.h"
#include "texture.h"
#include "maths.h"
#include "utils.h"
#include "brush_editor.h"
#include "material_editor.h"
#include "options.h"

static bool is_game_view_open = true;

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
	ImGui::Text("FPS: %.1f", win::fps);
	ImGui::End();
}

void mainTargetWidget() {
	mainTargetWidget(gfx::main_rtv.size, gfx::main_rtv.resource);
}

void mainTargetWidget(vec2 size, ID3D11ShaderResourceView *srv) {
	if (!is_game_view_open) return;
	if (!ImGui::Begin("Main", &is_game_view_open)) {
		ImGui::End();
		gfx::setMainRTVActive(false);
		return;
	}

	// set the main RTV to active if:
	// - the curent window is focused
	// - the mouse is inside the window
	bool is_focused = ImGui::IsWindowFocused();
	vec2 mouse_pos = ImGui::GetMousePos();
	vec4 window_bb = vec4(ImGui::GetWindowPos(), ImGui::GetWindowSize());
	gfx::setMainRTVActive(is_focused && window_bb.contains(mouse_pos));

	vec2 win_size = ImGui::GetWindowContentRegionMax();
	vec2 win_padding = ImGui::GetWindowContentRegionMin();

	win_size -= win_padding;

	float dx = win_size.x / size.x;
	float dy = win_size.y / size.y;

	if (dx > dy) {
		size.x *= dy;
		size.y = win_size.y;
	}
	else {
		size.x = win_size.x;
		size.y *= dx;
	}

	vec2 position = (win_size - size) * 0.5f + win_padding;
	vec2 win_pos = ImGui::GetWindowPos();
	gfx::setMainRTVBounds(vec4(position + win_pos, size));

	ImGui::SetCursorPos(position);
	ImGui::Image((ImTextureID)srv, size);
	ImGui::End();
}

bool imInputUint2(const char *label, unsigned int v[2], int flags) {
	static unsigned int step = 1;
	return ImGui::InputScalarN(label, ImGuiDataType_U32, v, 2, &step, nullptr, "%u", flags);
}

bool imInputUint3(const char *label, unsigned int v[3], int flags) {
	static unsigned int step = 1;
	return ImGui::InputScalarN(label, ImGuiDataType_U32, v, 3, &step, nullptr, "%u", flags);
}

// how long will the message be shown for
constexpr float show_time = 3.f;
// how long before the messagge starts disappearing
constexpr float falloff_time = 2.f;

static const ImColor level_to_colour[(int)LogLevel::Count] = {
    ImColor(  0,   0,   0), // None
    ImColor( 26, 102, 191), // Debug
    ImColor( 30, 149,  96), // Info
    ImColor(230, 179,   0), // Warning
    ImColor(251,  35,  78), // Error
    ImColor(255,   0,   0), // Fatal
};

struct Message {
	Message() = default;
	Message(LogLevel l, mem::ptr<char[]>&& m) : severity(l), message(mem::move(m)) {}
	LogLevel severity;
	mem::ptr<char[]> message;
	float timer = show_time;
};

static arr<Message> messages;
static char window_title[64] = "Message##";
static const size_t title_beg = strlen(window_title);

void messagesWidget() {
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

	const float PAD = 10.0f;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const vec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
	const vec2 work_size = viewport->WorkSize;
	vec2 window_pos = work_pos + PAD;
	const vec2 window_pos_pivot = 0.f;
	float y_size = 0.f;

	for (size_t i = 0; i < messages.size(); ++i) {
		Message &msg = messages[i];
		
		const float alpha = msg.timer / (show_time - falloff_time);
		ImVec4 border_colour = ImGui::GetStyleColorVec4(ImGuiCol_Border);
		border_colour.w = alpha;

		ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, vec2(0.f));
		//ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::SetNextWindowBgAlpha(alpha);
		ImGui::PushStyleColor(ImGuiCol_Border, border_colour);

		str::formatBuf(window_title + title_beg, sizeof(window_title) - title_beg, "%zu", i);
		
		ImGui::Begin(window_title, nullptr, window_flags);
			ImColor col = level_to_colour[(int)msg.severity];
			col.Value.w = alpha;
			ImGui::TextColored(col, msg.message.get());
			y_size = ImGui::GetWindowSize().y;
		ImGui::End();

		ImGui::PopStyleColor();

		msg.timer -= win::dt;
		if (msg.timer <= 0.f) {
			messages.removeSlow(i--);
		}

		window_pos.y += y_size + PAD;
	}
}

void addMessageToWidget(LogLevel severity, const char* message) {
	messages.push(severity, str::dup(message));
}

static bool should_load_file = false;
static bool should_save_file = false;
static unsigned int save_quality = 64;

void mainMenuBar(BrushEditor &be, MaterialEditor &me) {
	static bool open_save_popup = false;
	static bool is_saving = false;

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {

			if (ImGui::MenuItem("Save")) {
				open_save_popup = true;
			}

			if (ImGui::MenuItem("Load")) {
				should_load_file = true;
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			Options &options = Options::get();

			if (ImGui::MenuItem("Main View", nullptr, is_game_view_open)) {
				is_game_view_open = !is_game_view_open;
			}
			
			if (ImGui::MenuItem("Brush Editor", nullptr, be.isOpen())) {
				be.setOpen(!be.isOpen());
			}

			if (ImGui::MenuItem("Material Editor", nullptr, me.isOpen())) {
				me.setOpen(!me.isOpen());
			}

			if (ImGui::MenuItem("Logger", nullptr, logIsOpen())) {
				logSetOpen(!logIsOpen());
			}

			if (ImGui::MenuItem("Options", nullptr, options.isOpen())) {
				options.setOpen(!options.isOpen());
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	if (open_save_popup) {
		open_save_popup = false;
		is_saving = true;
		ImGui::OpenPopup("SaveToFile");
	}

	if (ImGui::BeginPopupModal("SaveToFile", &is_saving, ImGuiWindowFlags_AlwaysAutoResize)) {
		static const char *quality_levels[] = { "Low", "Medium", "High", "Very High", "Maximum", "Custom"};
		static int quality_sizes[] = { 32, 64, 128, 256, 512, 0 };
		static int cur_level_ind = 1;

		if (ImGui::Combo("Quality Level", &cur_level_ind, quality_levels, ARRLEN(quality_levels))) {
			save_quality = quality_sizes[cur_level_ind];
		}
		
		if (sliderUInt("Size", &save_quality, 0, 1024)) {
			unsigned int remainder = save_quality % 8;
			if (remainder < 4) save_quality -= remainder;
			else			   save_quality += remainder;
			cur_level_ind = ARRLEN(quality_levels) - 1;
		}

		if (ImGui::Button("Save")) {
			should_save_file = true;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void saveLoadFileDialog(Texture3D &main_texture, Shader *scale_cs) {
	if (should_save_file) {
		should_save_file = false;

		NFD::UniquePathU8 path;
		nfdu8filteritem_t filter[] = { { "Tex3D", "bin" } };
		nfdresult_t result = NFD::SaveDialog(path, filter, ARRLEN(filter));
		if (result == NFD_OKAY) {
			Texture3D out_text;
			out_text.create(save_quality, main_texture.getType());
			scale_cs->dispatch(save_quality / 8, {}, { main_texture.srv }, { out_text.uav });
			out_text.save(path.get(), true);
			return;
		}
		else if (result == NFD_CANCEL) {
			// user has pressed back
			return;
		}
		else {
			err("Error in the saving file dialog: %s", NFD::GetError());
			return;
		}
	}

	if (should_load_file) {
		should_load_file = false;
		NFD::UniquePathU8 path;
		nfdu8filteritem_t filter[] = { { "Tex3D", "bin" } };
		nfdresult_t result = NFD::OpenDialog(path, filter, ARRLEN(filter));
		if (result == NFD_OKAY) {
			main_texture.load(path.get());
			return;
		}
		else if (result == NFD_CANCEL) {
			// user has pressed back
			return;
		}
		else {
			err("Error in the loading file dialog: %s", NFD::GetError());
			return;
		}
	}
}

bool filledSlider(const char *str_id, float *p_data, float vmin, float vmax, const char *fmt) {
	const ImGuiDataType data_type = ImGuiDataType_Float;
	const ImGuiSliderFlags flags = 0;
	float *p_min = &vmin;
	float *p_max = &vmax;

	ImGuiWindow *window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext &g = *GImGui;
	const ImGuiStyle &style = g.Style;
	const ImGuiID id = window->GetID(str_id);
	const float w = ImGui::GetContentRegionAvail().x;

	const ImVec2 label_size = ImGui::CalcTextSize(str_id, NULL, true);
	const ImRect frame_bb(window->DC.CursorPos, vec2(w, label_size.y + style.FramePadding.y * 2.0f) + window->DC.CursorPos);
	const ImRect total_bb(frame_bb.Min, frame_bb.Max);

	const bool temp_input_allowed = true;
	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
		return false;

	// Default format string when passing NULL
	if (fmt == NULL)
		fmt = ImGui::DataTypeGetInfo(data_type)->PrintFmt;

	const bool hovered = ImGui::ItemHoverable(frame_bb, id);
	bool temp_input_is_active = temp_input_allowed && ImGui::TempInputIsActive(id);
	if (!temp_input_is_active)
	{
		// Tabbing or CTRL-clicking on Slider turns it into an input box
		const bool input_requested_by_tabbing = temp_input_allowed && (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_FocusedByTabbing) != 0;
		const bool clicked = (hovered && g.IO.MouseClicked[0]);
		const bool make_active = (input_requested_by_tabbing || clicked || g.NavActivateId == id || g.NavActivateInputId == id);
		if (make_active && temp_input_allowed)
			if (input_requested_by_tabbing || (clicked && g.IO.KeyCtrl) || g.NavActivateInputId == id)
				temp_input_is_active = true;

		if (make_active && !temp_input_is_active)
		{
			ImGui::SetActiveID(id, window);
			ImGui::SetFocusID(id, window);
			ImGui::FocusWindow(window);
			g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
		}
	}

	if (temp_input_is_active)
	{
		// Only clamp CTRL+Click input when ImGuiSliderFlags_AlwaysClamp is set
		const bool is_clamp_input = (flags & ImGuiSliderFlags_AlwaysClamp) != 0;
		return ImGui::TempInputScalar(frame_bb, id, str_id, data_type, p_data, fmt, is_clamp_input ? p_min : NULL, is_clamp_input ? p_max : NULL);
	}

	// Draw frame
	const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
	ImGui::RenderNavHighlight(frame_bb, id);
	ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

	// Slider behavior
	ImRect grab_bb;
	const bool value_changed = ImGui::SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, fmt, flags, &grab_bb);
	if (value_changed)
		ImGui::MarkItemEdited(id);

	constexpr float grab_padding = 2.f;
	const float value_width = vmax - vmin;
	const float value_norm = ((*p_data) - vmin) / value_width;
	const float frame_width = frame_bb.GetWidth() - grab_padding * 2.f;
	const float grab_width = math::max(value_norm * frame_width, 1.f);

	grab_bb.Min.x = window->DC.CursorPos.x + grab_padding;
	grab_bb.Max.x = grab_bb.Min.x + grab_width;

	// Render grab
	if (grab_bb.Max.x > grab_bb.Min.x)
		window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);

	// Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
	char value_buf[64];
	const char *value_buf_end = value_buf + ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, fmt);
	ImGui::RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.5f));

	IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
	return value_changed;
}

bool inputU8(const char *label, uint8_t *data, uint8_t step, uint8_t step_fast) {
	return ImGui::InputScalar(label, ImGuiDataType_U8, data, &step, &step_fast, "%u");
}

bool sliderUInt(const char *label, unsigned int *v, unsigned int vmin, unsigned int vmax, int flags) {
	return ImGui::SliderScalar(label, ImGuiDataType_U32, v, &vmin, &vmax, "%u", flags);
}

bool sliderUInt2(const char *label, unsigned int *v, unsigned int vmin, unsigned int vmax, int flags) {
	return ImGui::SliderScalarN(label, ImGuiDataType_U32, v, 2, &vmin, &vmax, "%u", flags);
}

void separatorTextEx(ImGuiID id, const char *label, const char *label_end, float extra_w) {
	ImGuiContext &g = *GImGui;
	ImGuiWindow *window = g.CurrentWindow;
	ImGuiStyle &style = g.Style;

	const ImVec2 label_size = ImGui::CalcTextSize(label, label_end, false);
	const ImVec2 pos = window->DC.CursorPos;
	const ImVec2 padding = ImVec2(20.0f, 3.f); // style.SeparatorTextPadding;

	const ImVec2 separator_text_align = ImVec2(0.0f, 0.5f); // style.SeparatorTextAlign

	const float separator_thickness = 3.0f; // style.SeparatorTextBorderSize;
	const ImVec2 min_size(label_size.x + extra_w + padding.x * 2.0f, ImMax(label_size.y + padding.y * 2.0f, separator_thickness));
	const ImRect bb(pos, ImVec2(window->WorkRect.Max.x, pos.y + min_size.y));
	const float text_baseline_y = ImFloor((bb.GetHeight() - label_size.y) * separator_text_align.y + 0.99999f); //ImMax(padding.y, ImFloor((style.SeparatorTextSize - label_size.y) * 0.5f));
	ImGui::ItemSize(min_size, text_baseline_y);
	if (!ImGui::ItemAdd(bb, id))
		return;

	const float sep1_x1 = pos.x;
	const float sep2_x2 = bb.Max.x;
	const float seps_y = ImFloor((bb.Min.y + bb.Max.y) * 0.5f + 0.99999f);

	const float label_avail_w = ImMax(0.0f, sep2_x2 - sep1_x1 - padding.x * 2.0f);
	const ImVec2 label_pos(pos.x + padding.x + ImMax(0.0f, (label_avail_w - label_size.x - extra_w) * separator_text_align.x), pos.y + text_baseline_y); // FIXME-ALIGN

	// This allows using SameLine() to position something in the 'extra_w'
	window->DC.CursorPosPrevLine.x = label_pos.x + label_size.x;

	const ImU32 separator_col = ImGui::GetColorU32(ImGuiCol_Separator);
	if (label_size.x > 0.0f)
	{
		const float sep1_x2 = label_pos.x - style.ItemSpacing.x;
		const float sep2_x1 = label_pos.x + label_size.x + extra_w + style.ItemSpacing.x;
		if (sep1_x2 > sep1_x1 && separator_thickness > 0.0f)
			window->DrawList->AddLine(ImVec2(sep1_x1, seps_y), ImVec2(sep1_x2, seps_y), separator_col, separator_thickness);
		if (sep2_x2 > sep2_x1 && separator_thickness > 0.0f)
			window->DrawList->AddLine(ImVec2(sep2_x1, seps_y), ImVec2(sep2_x2, seps_y), separator_col, separator_thickness);
		ImGui::RenderTextEllipsis(window->DrawList, label_pos, ImVec2(bb.Max.x, bb.Max.y + style.ItemSpacing.y), bb.Max.x, bb.Max.x, label, label_end, &label_size);
	}
	else
	{
		if (separator_thickness > 0.0f)
			window->DrawList->AddLine(ImVec2(sep1_x1, seps_y), ImVec2(sep2_x2, seps_y), separator_col, separator_thickness);
	}
}

void separatorText(const char *label) {
	ImGuiWindow *window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return;

	// The SeparatorText() vs SeparatorTextEx() distinction is designed to be considerate that we may want:
	// - allow headers to be draggable items (would require a stable ID + a noticeable highlight)
	// - this high-level entry point to allow formatting? (may require ID separate from formatted string)
	// - because of this we probably can't turn 'const char* label' into 'const char* fmt, ...'
	// Otherwise, we can decide that users wanting to drag this would layout a dedicated drag-item,
	// and then we can turn this into a format function.
	separatorTextEx(0, label, ImGui::FindRenderedTextEnd(label), 0.0f);
}