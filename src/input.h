#pragma once

#include <stddef.h>
#include <stdint.h>

#include "vec.h"

using Mouse = uint32_t;

enum : uint32_t {
	MOUSE_NONE   = 0,
	MOUSE_LEFT   = 1 << 0,
	MOUSE_RIGHT  = 1 << 1,
	MOUSE_MIDDLE = 1 << 2,
};

enum Keys {
	KEY_NONE,
	KEY_0,
	KEY_1,
	KEY_2,
	KEY_3,
	KEY_4,
	KEY_5,
	KEY_6,
	KEY_7,
	KEY_8,
	KEY_9,
	KEY_A,
	KEY_B,
	KEY_C,
	KEY_D,
	KEY_E,
	KEY_F,
	KEY_G,
	KEY_H,
	KEY_I,
	KEY_J,
	KEY_K,
	KEY_L,
	KEY_M,
	KEY_N,
	KEY_O,
	KEY_P,
	KEY_Q,
	KEY_R,
	KEY_S,
	KEY_T,
	KEY_U,
	KEY_V,
	KEY_W,
	KEY_X,
	KEY_Y,
	KEY_Z,
	KEY_F1,
	KEY_F2,
	KEY_F3,
	KEY_F4,
	KEY_F5,
	KEY_F6,
	KEY_F7,
	KEY_F8,
	KEY_F9,
	KEY_F10,
	KEY_F11,
	KEY_F12,
	KEY_F13,
	KEY_F14,
	KEY_F15,
	KEY_F16,
	KEY_F17,
	KEY_F18,
	KEY_F19,
	KEY_F20,
	KEY_F21,
	KEY_F22,
	KEY_F23,
	KEY_F24,
	KEY_TAB,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_UP,
	KEY_DOWN,
	KEY_PRIOR,
	KEY_NEXT,
	KEY_HOME,
	KEY_END,
	KEY_INSERT,
	KEY_DELETE,
	KEY_BACK,
	KEY_SPACE,
	KEY_RETURN,
	KEY_ESCAPE,
	KEY_APOSTROPHE,
	KEY_COMMA,
	KEY_MINUS,
	KEY_PERIOD,
	KEY_SLASH,
	KEY_SEMICOLON,
	KEY_EQUAL,
	KEY_LBRACKET,
	KEY_RBRACKER,
	KEY_BACKSLASH,
	KEY_CAPSLOCK,
	KEY_SCROLLLOCK,
	KEY_NUMLOCK,
	KEY_PRINT,
	KEY_PAUSE,
	KEY_KEYPAD0,
	KEY_KEYPAD1,
	KEY_KEYPAD2,
	KEY_KEYPAD3,
	KEY_KEYPAD4,
	KEY_KEYPAD5,
	KEY_KEYPAD6,
	KEY_KEYPAD7,
	KEY_KEYPAD8,
	KEY_KEYPAD9,
	KEY_KEYPADENTER,
	KEY_DECIMAL,
	KEY_MULTIPLY,
	KEY_SUBTRACT,
	KEY_ADD,
	KEY_SHIFT,
	KEY_CTRL,
	KEY_ALT,
	//KEY_LSHIFT,
	//KEY_LCTRL,
	//KEY_LALT,
	KEY_LWIN,
	//KEY_RSHIFT,
	//KEY_RCTRL,
	//KEY_RALT,
	KEY_RWIN,
	KEY_APPS,
	KEY__COUNT,
};

bool isKeyDown(Keys key);
bool isKeyUp(Keys key);
bool isKeyPressed(Keys key);
bool isMouseDown(Mouse mouse);
bool isMouseUp(Mouse mouse);
vec2i getMousePos();
vec2 getMousePosNorm();
vec2i getMousePosRel();
float getMouseWheel();

Mouse win32ToMouse(uintptr_t virtual_key);
Keys win32ToKeys(uintptr_t virtual_key);

void setKeyState(Keys key, bool is_down);
void setMousePosition(vec2i pos);
void setMouseRelative(vec2i rel);
void setMouseButtonState(Mouse button, bool is_down);
void setMouseWheel(float value);