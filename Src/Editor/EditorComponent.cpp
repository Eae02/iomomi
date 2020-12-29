#include "EditorComponent.hpp"
#include "../Graphics/RenderSettings.hpp"
#include "../World/Entities/Components/LiquidPlaneComp.hpp"
#include "../World/Entities/Components/ActivatableComp.hpp"
#include "../World/Entities/Components/ActivatorComp.hpp"

#include <magic_enum.hpp>

static constexpr float ICON_SIZE = 32;

EditorIcon::EditorIcon(const glm::vec3& worldPos, std::function<void()> callback)
	: m_callback(move(callback))
{
	glm::vec4 sp4 = RenderSettings::instance->viewProjection * glm::vec4(worldPos, 1.0f);
	glm::vec3 sp3 = glm::vec3(sp4) / sp4.w;
	
	glm::vec2 screenPos = (glm::vec2(sp3) * 0.5f + 0.5f) *
	                      glm::vec2(eg::CurrentResolutionX(), eg::CurrentResolutionY());
	
	m_rectangle = eg::Rectangle::CreateCentered(screenPos, ICON_SIZE, ICON_SIZE);
	m_depth = sp3.z;
	m_behindScreen = sp4.w < 0;
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
		world->entManager.ForEach([&](Ent& activatorEntity)
		{
			ActivatorComp* activator = activatorEntity.GetComponentMut<ActivatorComp>();
			if (activator && activator->activatableName == activatable->m_name)
			{
				ActivationLightStripEnt::GenerateForActivator(*world, activatorEntity);
			}
		});
	}
	
	ActivationLightStripEnt::GenerateForActivator(*world, entity);
}

void EditorState::InvalidateWater() const
{
	world->entManager.ForEachWithComponent<LiquidPlaneComp>([&] (Ent& entity)
	{
		entity.GetComponentMut<LiquidPlaneComp>()->MarkOutOfDate();
	});
}

static_assert(EDITOR_NUM_TOOLS == magic_enum::enum_count<EditorTool>());
