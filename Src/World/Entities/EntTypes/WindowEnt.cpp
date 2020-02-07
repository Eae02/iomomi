#include "WindowEnt.hpp"

static const glm::vec3 untransformedPositions[] = 
{
	{ -1, -1, -1 }, { 1, -1, -1 }, { -1, 1, 1 },
	{ 1, -1, -1 }, { 1, 1, 1 }, { -1, 1, 1 }
};
static const glm::vec2 uvs[] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };

void WindowEnt::RenderSettings()
{
	Ent::RenderSettings();
}

void WindowEnt::Draw(const EntDrawArgs& args)
{
	Ent::Draw(args);
}

void WindowEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	Ent::EditorDraw(args);
}

const void* WindowEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(AxisAlignedQuadComp))
		return &m_aaQuad;
	return nullptr;
}
