#include "EditorComponent.hpp"

#include <magic_enum/magic_enum.hpp>

#include "../Graphics/RenderSettings.hpp"
#include "../World/Entities/Components/ActivatableComp.hpp"
#include "../World/Entities/Components/ActivatorComp.hpp"
#include "../World/Entities/Components/LiquidPlaneComp.hpp"

static constexpr float ICON_SIZE = 32;

EditorIcon EditorState::CreateIcon(const glm::vec3& worldPos, std::function<void()> callback) const
{
	EditorIcon icon;
	icon.m_callback = std::move(callback);

	glm::vec4 sp4 = viewProjection * glm::vec4(worldPos, 1.0f);
	glm::vec3 sp3 = glm::vec3(sp4) / sp4.w;

	glm::vec2 screenPos = (glm::vec2(sp3) * 0.5f + 0.5f) * windowRect.Size();

	icon.m_rectangle = eg::Rectangle::CreateCentered(screenPos, ICON_SIZE, ICON_SIZE);
	icon.m_depth = sp3.z;
	icon.m_behindScreen = sp4.w < 0;

	return icon;
}

bool EditorState::IsEntitySelected(const Ent& entity) const
{
	for (const std::weak_ptr<Ent>& selEnt : *selectedEntities)
	{
		std::shared_ptr<Ent> selEntSP = selEnt.lock();
		if (selEntSP.get() == &entity)
			return true;
	}
	return false;
}

void EditorState::EntityMoved(Ent& entity) const
{
	if (ActivatableComp* activatable = entity.GetComponentMut<ActivatableComp>())
	{
		world->entManager.ForEach(
			[&](Ent& activatorEntity)
			{
				ActivatorComp* activator = activatorEntity.GetComponentMut<ActivatorComp>();
				if (activator && activator->activatableName == activatable->m_name)
				{
					ActivationLightStripEnt::GenerateForActivator(*world, activatorEntity);
				}
			}
		);
	}

	ActivationLightStripEnt::GenerateForActivator(*world, entity);
}

void EditorState::InvalidateWater() const
{
	world->entManager.ForEachWithComponent<LiquidPlaneComp>(
		[&](Ent& entity) { entity.GetComponentMut<LiquidPlaneComp>()->MarkOutOfDate(); }
	);
}

static_assert(EDITOR_NUM_TOOLS == magic_enum::enum_count<EditorTool>());
