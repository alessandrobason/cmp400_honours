#include "input.h"

#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "system.h"

#define VK_NUMPAD_ENTER (VK_RETURN + KF_EXTENDED)

static bool keys_state[KEY__COUNT] = { 0 };
static bool prev_keys_state[KEY__COUNT] = { 0 };
static uint32_t mouse_state = 0;
static uint32_t prev_mouse_state = 0;
static vec2i mouse_position;
static vec2i mouse_relative;
static float mouse_wheel = 0.f;

bool isKeyDown(Keys key) {
	return keys_state[key];
}

bool isKeyUp(Keys key) {
	return !keys_state[key];
}

bool isKeyPressed(Keys key) {
	bool pressed = !prev_keys_state[key] && keys_state[key];
	prev_keys_state[key] = keys_state[key];
	return pressed;
}

bool isMouseDown(Mouse mouse) {
	return mouse_state & mouse;
}

bool isMouseUp(Mouse mouse) {
	return !isMouseDown(mouse);
}

bool isMousePressed(Mouse mouse) {
	bool was_not_down = !(prev_mouse_state & mouse);
	bool is_down = mouse_state & mouse;
	prev_mouse_state = mouse_state;
	return is_down && was_not_down;
}

vec2i getMousePos() {
	return mouse_position;
}

vec2 getMousePosNorm() {
	return (vec2)mouse_position / win::getSize();
}

vec2i getMousePosRel() {
	return mouse_relative;
}

float getMouseWheel() {
	return mouse_wheel;
}

Mouse win32ToMouse(uintptr_t virtual_key) {
	switch (virtual_key) {
	case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: case WM_LBUTTONUP: return MOUSE_LEFT;
	case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:	case WM_RBUTTONUP: return MOUSE_RIGHT;
	case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:	case WM_MBUTTONUP: return MOUSE_MIDDLE;
	}
	return MOUSE_NONE;
}

Keys win32ToKeys(uintptr_t virtual_key) {
	switch (virtual_key) {
	case '0': return KEY_0;
	case '1': return KEY_1;
	case '2': return KEY_2;
	case '3': return KEY_3;
	case '4': return KEY_4;
	case '5': return KEY_5;
	case '6': return KEY_6;
	case '7': return KEY_7;
	case '8': return KEY_8;
	case '9': return KEY_9;
	case 'A': return KEY_A;
	case 'B': return KEY_B;
	case 'C': return KEY_C;
	case 'D': return KEY_D;
	case 'E': return KEY_E;
	case 'F': return KEY_F;
	case 'G': return KEY_G;
	case 'H': return KEY_H;
	case 'I': return KEY_I;
	case 'J': return KEY_J;
	case 'K': return KEY_K;
	case 'L': return KEY_L;
	case 'M': return KEY_M;
	case 'N': return KEY_N;
	case 'O': return KEY_O;
	case 'P': return KEY_P;
	case 'Q': return KEY_Q;
	case 'R': return KEY_R;
	case 'S': return KEY_S;
	case 'T': return KEY_T;
	case 'U': return KEY_U;
	case 'V': return KEY_V;
	case 'W': return KEY_W;
	case 'X': return KEY_X;
	case 'Y': return KEY_Y;
	case 'Z': return KEY_Z;
	case VK_F1:  return KEY_F1;
	case VK_F2:  return KEY_F2;
	case VK_F3:  return KEY_F3;
	case VK_F4:  return KEY_F4;
	case VK_F5:  return KEY_F5;
	case VK_F6:  return KEY_F6;
	case VK_F7:  return KEY_F7;
	case VK_F8:  return KEY_F8;
	case VK_F9:  return KEY_F9;
	case VK_F10: return KEY_F10;
	case VK_F11: return KEY_F11;
	case VK_F12: return KEY_F12;
	case VK_F13: return KEY_F13;
	case VK_F14: return KEY_F14;
	case VK_F15: return KEY_F15;
	case VK_F16: return KEY_F16;
	case VK_F17: return KEY_F17;
	case VK_F18: return KEY_F18;
	case VK_F19: return KEY_F19;
	case VK_F20: return KEY_F20;
	case VK_F21: return KEY_F21;
	case VK_F22: return KEY_F22;
	case VK_F23: return KEY_F23;
	case VK_F24: return KEY_F24;
	case VK_TAB:          return KEY_TAB;
	case VK_LEFT:         return KEY_LEFT;
	case VK_RIGHT:        return KEY_RIGHT;
	case VK_UP:           return KEY_UP;
	case VK_DOWN:         return KEY_DOWN;
	case VK_PRIOR:        return KEY_PRIOR;
	case VK_NEXT:         return KEY_NEXT;
	case VK_HOME:         return KEY_HOME;
	case VK_END:          return KEY_END;
	case VK_INSERT:       return KEY_INSERT;
	case VK_DELETE:       return KEY_DELETE;
	case VK_BACK:         return KEY_BACK;
	case VK_SPACE:        return KEY_SPACE;
	case VK_RETURN:       return KEY_RETURN;
	case VK_ESCAPE:       return KEY_ESCAPE;
	case VK_OEM_7:        return KEY_APOSTROPHE;
	case VK_OEM_COMMA:    return KEY_COMMA;
	case VK_OEM_MINUS:    return KEY_MINUS;
	case VK_OEM_PERIOD:   return KEY_PERIOD;
	case VK_OEM_2:        return KEY_SLASH;
	case VK_OEM_1:        return KEY_SEMICOLON;
	case VK_OEM_PLUS:     return KEY_EQUAL;
	case VK_OEM_4:        return KEY_LBRACKET;
	case VK_OEM_6:        return KEY_RBRACKER;
	case VK_OEM_5:        return KEY_BACKSLASH;
	case VK_CAPITAL:      return KEY_CAPSLOCK;
	case VK_SCROLL:       return KEY_SCROLLLOCK;
	case VK_NUMLOCK:      return KEY_NUMLOCK;
	case VK_PRINT:        return KEY_PRINT;
	case VK_PAUSE:        return KEY_PAUSE;
	case VK_NUMPAD0:      return KEY_KEYPAD0;
	case VK_NUMPAD1:      return KEY_KEYPAD1;
	case VK_NUMPAD2:      return KEY_KEYPAD2;
	case VK_NUMPAD3:      return KEY_KEYPAD3;
	case VK_NUMPAD4:      return KEY_KEYPAD4;
	case VK_NUMPAD5:      return KEY_KEYPAD5;
	case VK_NUMPAD6:      return KEY_KEYPAD6;
	case VK_NUMPAD7:      return KEY_KEYPAD7;
	case VK_NUMPAD8:      return KEY_KEYPAD8;
	case VK_NUMPAD9:      return KEY_KEYPAD9;
	case VK_NUMPAD_ENTER: return KEY_KEYPADENTER;
	case VK_DECIMAL:      return KEY_DECIMAL;
	case VK_MULTIPLY:     return KEY_MULTIPLY;
	case VK_SUBTRACT:     return KEY_SUBTRACT;
	case VK_ADD:          return KEY_ADD;
	case VK_SHIFT:		  return KEY_SHIFT;
	case VK_CONTROL:	  return KEY_CTRL;
	case VK_MENU:		  return KEY_ALT;
	case VK_LWIN:         return KEY_LWIN;
	case VK_RWIN:         return KEY_RWIN;
	case VK_APPS:         return KEY_APPS;
	}

	return KEY_NONE;
}

void setKeyState(Keys key, bool is_down) {
	prev_keys_state[key] = keys_state[key];
	keys_state[key] = is_down;
}

void setMousePosition(vec2i pos) {
	mouse_relative = pos - mouse_position;
	mouse_position = pos;
}

void setMouseRelative(vec2i rel) {
	mouse_relative = rel;
}

void setMouseButtonState(Mouse button, bool is_down) {
	prev_mouse_state = mouse_state;
	if (is_down) mouse_state |= button;
	else         mouse_state ^= button;
}

void setMouseWheel(float value) {
	mouse_wheel = value;
}
