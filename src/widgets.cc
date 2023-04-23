#include "widgets.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <d3d11.h>

#include "system.h"
#include "tracelog.h"
#include "maths.h"
#include "utils.h"

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
	if (!ImGui::Begin("Game")) {
		ImGui::End();
		gfx::setMainRTVActive(false);
		return;
	}

	gfx::setMainRTVActive(true);

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