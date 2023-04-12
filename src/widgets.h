#pragma once

#include "vec.h"
#include "d3d11_fwd.h"

enum class LogLevel;

void fpsWidget();
void mainTargetWidget();
void mainTargetWidget(vec2 size, ID3D11ShaderResourceView *srv);
void messagesWidget();
void addMessageToWidget(LogLevel severity, const char *message);

bool imInputUint3(const char *label, unsigned int v[3], int flags = 0);