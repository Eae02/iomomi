#include "ECEditorVisible.hpp"

#include <imgui.h>

void ECEditorVisible::RenderDefaultSettings(eg::Entity& entity)
{
	if (eg::ECPosition3D* positionComp = entity.FindComponent<eg::ECPosition3D>())
	{
		ImGui::DragFloat3("Position", &positionComp->position.x, 0.1f);
	}
}
