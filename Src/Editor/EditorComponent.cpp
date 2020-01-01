#include "EditorComponent.hpp"

glm::vec3 EditorComponent::SnapToGrid(const glm::vec3& pos)
{
	const float STEP = eg::IsButtonDown(eg::Button::LeftAlt) ? 0.5f : 0.1f;
	return glm::round(pos / STEP) * STEP;
}

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