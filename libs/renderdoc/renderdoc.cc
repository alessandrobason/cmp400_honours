#include "renderdoc.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "renderdoc_app.h"

static RENDERDOC_API_1_5_0 *rdoc_api = nullptr;
static HMODULE rdco_mod = nullptr;

bool renderdocInit() {
	rdco_mod = LoadLibrary(TEXT("renderdoc.dll"));
	if (!rdco_mod) return false;
	pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(rdco_mod, "RENDERDOC_GetAPI");
	int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_5_0, (void **)&rdoc_api);
	return ret == 1;
}

bool renderdocCleanup() {
	return rdco_mod ? FreeLibrary(rdco_mod) : false;
}

bool renderdocCaptureStart() {
	if (rdoc_api) {
		rdoc_api->StartFrameCapture(nullptr, nullptr);
		return true;
	}
	return false;
}

void renderdocCaptureEnd() {
	if (rdoc_api) {
		rdoc_api->EndFrameCapture(nullptr, nullptr);
	}
}
