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
	ImGui::Text("FPS: %.1f", win::fps);
	ImGui::End();
}

void mainTargetWidget() {
	mainTargetWidget(gfx::main_rtv.size, gfx::main_rtv.resource);
}

void mainTargetWidget(vec2 size, ID3D11ShaderResourceView *srv) {
	ImGui::Begin("Game");

	//vec2 img_size = gfx::main_rtv.size;
	//vec2 img_size = { 512, 512 };
	vec2 win_size = ImGui::GetWindowContentRegionMax();
	vec2 win_padding = ImGui::GetWindowContentRegionMin();

	win_size -= win_padding;

#if RESIZE_RENDER_TARGET
	vec2 size_diff = size - win_size;
	if (fabsf(size_diff.x) > 0.1f || fabsf(size_diff.y) > 0.1f) {
		info("resize");
		main_rtv.resize((int)win_size.x, (int)win_size.y);
	}
#endif

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

	ImGui::SetCursorPos((win_size - size) * 0.5f + win_padding);
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
	messages.emplace_back(severity, str::dup(message));
}