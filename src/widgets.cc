#include "widgets.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <d3d11.h>
#include <nfd.hpp>

#include "system.h"
#include "input.h"
#include "tracelog.h"
#include "shader.h"
#include "texture.h"
#include "maths.h"
#include "utils.h"
#include "brush_editor.h"
#include "material_editor.h"
#include "ray_tracing_editor.h"
#include "sculpture.h"
#include "options.h"

// how long before the messagge starts disappearing
constexpr float falloff_time = 2.f;

static struct GameView {
	bool is_open = true;
	bool was_focused = false;
} game_view;

static struct MessageManager {
	struct Message {
		Message() = default;
		Message(LogLevel l, mem::ptr<char[]> &&m, float t) : severity(l), message(mem::move(m)), timer(t) {}
		LogLevel severity;
		mem::ptr<char[]> message;
		float timer = 0.f;
	};

	void widget();
	void add(LogLevel severity, const char *message, float show_time);

	arr<Message> messages;
	char window_title[64] = "Message##";
	const size_t title_beg = strlen(window_title);
} msg_manager;

static struct KeyRemapper {
	void widget();
	bool is_open = false;
} remapper;

static struct ControlsMenu {
	bool is_open = false;
} controls;

static struct MenuBar {
	void widget();
	void saveWidget();

	BrushEditor *brush_editor       = nullptr;
	MaterialEditor *material_editor = nullptr;
	RayTracingEditor *rt_editor     = nullptr;
	Sculpture *sculpture            = nullptr;

	bool open_save_popup  = false;
	bool open_new_popup   = false;
	bool is_saving        = false;
	bool is_creating      = false;
	bool should_load_file = false;
	bool should_save_file = false;
	vec3u save_quality    = 64;
} menu_bar;

namespace widgets {
	void setupMenuBar(BrushEditor &be, MaterialEditor &me, RayTracingEditor &re, Sculpture &sc) {
		menu_bar.brush_editor    = &be;
		menu_bar.material_editor = &me;
		menu_bar.rt_editor       = &re;
		menu_bar.sculpture       = &sc;
	}

	void fps() {
		constexpr float PAD = 10.0f;
		constexpr ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
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

	void imageView(Handle<Texture2D> texture, vec4 *out_bounds) {
		vec2 win_size = ImGui::GetWindowContentRegionMax();
		vec2 win_padding = ImGui::GetWindowContentRegionMin();

		win_size -= win_padding;

		vec2 size = texture->size;
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
		if (out_bounds) *out_bounds = vec4(position + win_pos, size);

		ImGui::SetCursorPos(position);
		ImGui::Image((ImTextureID)texture->srv, size);
	}

	void mainView() {
		if (!game_view.is_open) return;
		if (!ImGui::Begin("Main", &game_view.is_open)) {
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
		if (window_bb.contains(mouse_pos)) {
			// if the user presses the lmb inside the RTV, even if it was not selected it will sculpt anyway,
			// this is to prevent that
			if (is_focused && !game_view.was_focused && isMouseDown(MOUSE_LEFT)) {
				is_focused = false;
			}
			if (isMouseDown(MOUSE_RIGHT)) {
				is_focused = true;
				ImGui::SetWindowFocus();
			}
			gfx::setMainRTVActive(is_focused);
		}

		game_view.was_focused = is_focused;

		ImGui::BeginDisabled(!is_focused);
		vec4 bounds = 0;
		imageView(gfx::main_rtv, &bounds);
		ImGui::EndDisabled();

		gfx::setMainRTVBounds(bounds);


		ImGui::End();
	}

	void messages() {
		msg_manager.widget();
	}

	void addMessage(LogLevel severity, const char *message, float show_time) {
		msg_manager.messages.push(severity, str::dup(message), show_time);
	}

	void keyRemapper() {
		remapper.widget();
	}
	
	void controlsPage() {
		if (!controls.is_open) return;
		if (!ImGui::Begin("Controls", &controls.is_open)) {
			ImGui::End();
			return;
		}
	
		separatorText("Base");

		ImGui::Text(
			"Exit program: %s\n"
			"Take Screenshot: %s\n"
			"Capture Frame (debugging only): %s\n",
			getKeyName(getActionKey(Action::CloseProgram)),
			getKeyName(getActionKey(Action::TakeScreenshot)),
			getKeyName(getActionKey(Action::CaptureFrame))
		);

		separatorText("Camera");

		ImGui::Text(
			"Rotate Camera: %s %s %s %s\n"
			"Reset Zoom: %s\n"
			"Zoom in: %s\n"
			"Zoom out: %s\n",
			getKeyName(getActionKey(Action::RotateCameraHorPos)),
			getKeyName(getActionKey(Action::RotateCameraHorNeg)),
			getKeyName(getActionKey(Action::RotateCameraVerPos)),
			getKeyName(getActionKey(Action::RotateCameraVerNeg)),
			getKeyName(getActionKey(Action::ResetZoom)),
			getKeyName(getActionKey(Action::ZoomIn)),
			getKeyName(getActionKey(Action::ZoomOut))
		);

		separatorText("Brush");

		ImGui::Text(
			"Use brush: %s\n"
			"Use eraser: %s\n"
			"While editing: \n"
			"  - Modify scale: %s\n"
			"  - Modify depth: %s\n"
			"  - Modify blend amount: %s\n",
			getKeyName(getActionKey(Action::ChangeToBrush)),
			getKeyName(getActionKey(Action::ChangeToEraser)),
			getKeyName(KEY_SHIFT),
			getKeyName(KEY_CTRL),
			getKeyName(KEY_ALT)
		);

		separatorText("File");

		ImGui::Text(
			"New: %s-%s\n"
			"Save: %s-%s\n",
			getKeyName(KEY_CTRL), getKeyName(KEY_A),
			getKeyName(KEY_CTRL), getKeyName(KEY_N)
		);

		ImGui::End();
	}

	void menuBar() {
		menu_bar.widget();
	}

	void saveLoadFile() {
		menu_bar.saveWidget();
	}

} // namespace widgets

void MessageManager::widget() {
	constexpr ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

	const float PAD = 10.0f;
	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	const vec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
	const vec2 work_size = viewport->WorkSize;
	vec2 window_pos = work_pos + PAD;
	const vec2 window_pos_pivot = 0.f;
	float y_size = 0.f;

	for (size_t i = 0; i < messages.len; ++i) {
		auto &msg = messages[i];

		const float alpha = msg.timer / falloff_time;
		ImVec4 border_colour = ImGui::GetStyleColorVec4(ImGuiCol_Border);
		border_colour.w = alpha;

		ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, vec2(0.f));
		ImGui::SetNextWindowBgAlpha(alpha);
		ImGui::PushStyleColor(ImGuiCol_Border, border_colour);

		str::formatBuf(window_title + title_beg, sizeof(window_title) - title_beg, "%zu", i);

		ImGui::Begin(window_title, nullptr, window_flags);
		ImColor col = logGetLevelColour(msg.severity);
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

void KeyRemapper::widget() {
	if (!is_open) return;
	if (!ImGui::Begin("Key Remapping", &is_open)) {
		ImGui::End();
		return;
	}

	tooltip("These are various actions that can be performed with their key, here you can change that key", false);

	static Action cur_action = Action::Count;
	static bool is_popup_open = false;

	const auto &remapAction = [](Action action, const char *name, Action &selected, const char *tip = nullptr) {
		ImGui::Text(name);
		if (tip) tooltip(tip);
		ImGui::SameLine();
		if (ImGui::Button(getKeyName(getActionKey(action)), vec2(50, 20))) {
			selected = action;
			setLastKeyPressed(KEY_NONE);
			is_popup_open = true;
			ImGui::OpenPopup("Remap");
		}
	};

	remapAction(Action::ResetZoom, "Reset Zoom", cur_action);
	remapAction(Action::CloseProgram, "Close Program", cur_action);
	remapAction(Action::TakeScreenshot, "Take Screenshot", cur_action);
	remapAction(Action::RotateCameraHorPos, "Rotate Camera Horizontal Positive", cur_action);
	remapAction(Action::RotateCameraHorNeg, "Rotate Camera Horizontal Negative", cur_action);
	remapAction(Action::RotateCameraVerPos, "Rotate Camera Vertical Positive", cur_action);
	remapAction(Action::RotateCameraVerNeg, "Rotate Camera Vertical Negative", cur_action);
	remapAction(Action::ZoomIn, "Zoom In", cur_action);
	remapAction(Action::ZoomOut, "Zoom Out", cur_action);
	remapAction(Action::CaptureFrame, "Capture Frame", cur_action, "(debugging only) Captures a frame in RenderDoc");
	remapAction(Action::ChangeToBrush, "Change to Brush", cur_action);
	remapAction(Action::ChangeToEraser, "Change to Eraser", cur_action);

	if (ImGui::BeginPopupModal("Remap", &is_popup_open)) {
		Keys last_pressed = getLastKeyPressed();
		ImGui::Text("Press any key: ");
		ImGui::SameLine();
		ImGui::Text(getKeyName(last_pressed));

		if (ImGui::Button("Accept")) {
			is_popup_open = false;
			setActionKey(cur_action, last_pressed);
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel")) {
			is_popup_open = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

void MenuBar::widget() {
	assert(brush_editor && material_editor && rt_editor);

	if (isKeyDown(KEY_CTRL)) {
		bool is_popup_open = open_new_popup || open_save_popup;
		if (!is_popup_open) {
			if (isKeyPressed(KEY_N)) {
				open_new_popup = true;
			}
			if (isKeyPressed(KEY_S)) {
				if (sculpture->getPath()) {
					sculpture->save(save_quality);
				}
				else {
					open_save_popup = true;
				}
			}
		}
	}

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New", "Ctrl+N")) {
				open_new_popup = true;
			}

			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				open_save_popup = true;
			}

			if (ImGui::MenuItem("Load")) {
				should_load_file = true;
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			Options &options = Options::get();

			if (ImGui::MenuItem("Main View", nullptr, game_view.is_open)) {
				game_view.is_open = !game_view.is_open;
			}

			if (ImGui::MenuItem("Render View", nullptr, rt_editor->isViewOpen())) {
				rt_editor->setViewOpen(!rt_editor->isViewOpen());
			}

			if (ImGui::MenuItem("Render editor", nullptr, rt_editor->isEditorOpen())) {
				rt_editor->setEditorOpen(!rt_editor->isEditorOpen());
			}

			if (ImGui::MenuItem("Brush Editor", nullptr, brush_editor->isOpen())) {
				brush_editor->setOpen(!brush_editor->isOpen());
			}

			if (ImGui::MenuItem("Material Editor", nullptr, material_editor->isOpen())) {
				material_editor->setOpen(!material_editor->isOpen());
			}

			if (ImGui::MenuItem("Logger", nullptr, logIsOpen())) {
				logSetOpen(!logIsOpen());
			}

			if (ImGui::MenuItem("Options", nullptr, options.isOpen())) {
				options.setOpen(!options.isOpen());
			}

			if (ImGui::MenuItem("Key Remapper", nullptr, remapper.is_open)) {
				remapper.is_open = !remapper.is_open;
			}

			if (ImGui::MenuItem("Controls", nullptr, controls.is_open)) {
				controls.is_open = !controls.is_open;
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	if (open_save_popup) {
		is_saving = true;
		ImGui::OpenPopup("Save To File");
	}

	if (open_new_popup) {
		is_creating = true;
		ImGui::OpenPopup("New File");
	}

	const auto &qualityDecision = [](Handle<Texture3D> tex, vec3u &quality, int &cur_level) {
		static const char *quality_levels[] = { "Low", "Medium", "High", "Very High", "Maximum", "Original", "Custom" };
		vec3u quality_sizes[] = { 32, 64, 128, 256, 512, tex->size, 0 };

		if (ImGui::Combo("Quality Level", &cur_level, quality_levels, ARRLEN(quality_levels))) {
			quality = quality_sizes[cur_level];
		}

		if (sliderUInt3("Size", quality.data, 0, 1024)) {
			quality = vec3u(round(vec3(quality) / 8.f)) * 8;
			cur_level = ARRLEN(quality_levels) - 1;
		}
	};

	if (ImGui::BeginPopupModal("Save To File", &is_saving, ImGuiWindowFlags_AlwaysAutoResize)) {
		static int cur_level = 5;
		qualityDecision(sculpture->texture, save_quality, cur_level);

		// only called once, so we can setup the variables
		if (open_save_popup) {
			open_save_popup = false;
			save_quality = sculpture->texture->size;
		}

		if (ImGui::Button("Save")) {
			should_save_file = true;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("New File", &is_creating, ImGuiWindowFlags_AlwaysAutoResize)) {
		static vec3u quality = 64;
		static Shapes cur_shape = Shapes::Sphere;
		static ShapeData shader_data;
		static int cur_level = 5;

		// only called once, so we can setup the variables
		if (open_new_popup) {
			open_new_popup = false;
			quality = sculpture->texture->size;
		}

		qualityDecision(sculpture->texture, quality, cur_level);

		ImGui::Combo("Initial Shape", (int *)&cur_shape, "Sphere\0Box\0Cylinder");

		ImGui::DragFloat3("Position##shape", shader_data.position.data);

		switch (cur_shape) {
		case Shapes::Sphere:
			ImGui::DragFloat("Radius##shape", &shader_data.sphere.radius);
			break;
		case Shapes::Box:
			ImGui::DragFloat3("Size##shape", shader_data.box.size.data);
			break;
		case Shapes::Cylinder:
			ImGui::DragFloat("Radius##shape", &shader_data.cylinder.radius);
			ImGui::DragFloat("Height##shape", &shader_data.cylinder.height);
			break;
		}

		if (ImGui::Button("New")) {
			if (all(quality != sculpture->texture->size)) {
				sculpture->texture->init(quality, sculpture->texture->getType());
			}
			brush_editor->runFillShader(cur_shape, shader_data, sculpture->texture);
			
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void MenuBar::saveWidget() {
	if (should_save_file) {
		should_save_file = false;

		NFD::UniquePathU8 path;
		nfdu8filteritem_t filter[] = { { "Tex3D", "bin" } };
		nfdresult_t result = NFD::SaveDialog(path, filter, ARRLEN(filter));
		if (result == NFD_OKAY) {
			sculpture->save(save_quality, path.release());
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
			sculpture->texture->loadFromFile(path.get());
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

bool filledSlider(const char *str_id, float *p_data, float vmin, float vmax, const char *fmt, ImGuiSliderFlags flags) {
	const ImGuiDataType data_type = ImGuiDataType_Float;
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

	const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
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
	const float value_norm = math::clamp(((*p_data) - vmin) / value_width, 0.f, 1.f);
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

bool inputUint(const char *label, uint &value, uint step, int flags) {
	return ImGui::InputScalar(label, ImGuiDataType_U32, &value, &step);
}

bool inputUint2(const char *label, unsigned int v[2], int flags) {
	static unsigned int step = 1;
	return ImGui::InputScalarN(label, ImGuiDataType_U32, v, 2, &step, nullptr, "%u", flags);
}

bool inputUint3(const char *label, unsigned int v[3], int flags) {
	static unsigned int step = 1;
	return ImGui::InputScalarN(label, ImGuiDataType_U32, v, 3, &step, nullptr, "%u", flags);
}

bool sliderUInt(const char *label, unsigned int *v, unsigned int vmin, unsigned int vmax, int flags) {
	return ImGui::SliderScalar(label, ImGuiDataType_U32, v, &vmin, &vmax, "%u", flags);
}

bool sliderUInt2(const char *label, unsigned int v[2], unsigned int vmin, unsigned int vmax, int flags) {
	return ImGui::SliderScalarN(label, ImGuiDataType_U32, v, 2, &vmin, &vmax, "%u", flags);
}

bool sliderUInt3(const char *label, unsigned int v[3], unsigned int vmin, unsigned int vmax, int flags) {
	return ImGui::SliderScalarN(label, ImGuiDataType_U32, v, 3, &vmin, &vmax, "%u", flags);
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

void tooltip(const char *msg, bool same_line) {
	if (same_line) ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(msg);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}