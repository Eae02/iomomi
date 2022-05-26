#pragma once

#ifdef EG_HAS_IMGUI
#include <EGameImGui.hpp>

void ImPushDisabled(bool disabled);
void ImPopDisabled(bool disabled);
#else
namespace ImGui
{
	inline void Separator() { }
}
#endif
