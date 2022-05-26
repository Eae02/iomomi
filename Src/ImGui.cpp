#include "ImGui.hpp"

#ifdef EG_HAS_IMGUI
#include <imgui_internal.h>

void ImPushDisabled(bool disabled)
{
	if (disabled)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
	}
}

void ImPopDisabled(bool disabled)
{
	if (disabled)
	{
		ImGui::PopStyleVar();
		ImGui::PopItemFlag();
	}
}
#endif
