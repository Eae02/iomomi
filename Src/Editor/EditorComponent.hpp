#pragma once

#include "EditorCamera.hpp"
#include "../World/World.hpp"

enum class EditorTool
{
	Walls,
	Corners,
	Entities
};

constexpr size_t EDITOR_NUM_TOOLS = 3;

struct EditorState
{
	World* world;
	const EditorCamera* camera;
	const eg::PerspectiveProjection* projection;
	eg::Ray viewRay;
	std::vector<std::weak_ptr<Ent>>* selectedEntities;
	EditorTool tool;
	
	bool IsEntitySelected(const Ent& entity) const;
};

class EditorIcon
{
public:
	friend class Editor;
	
	EditorIcon(const glm::vec3& worldPos, std::function<void()> callback);
	
	int iconIndex = 5;
	bool selected = false;
	bool shouldClearSelection = true;
	bool hideIfNotHovered = false;
	
	const eg::Rectangle& Rectangle() const { return m_rectangle; }
	
	void ApplyDepthBias(float bias) { m_depth -= bias; }
	
private:
	eg::Rectangle m_rectangle;
	float m_depth;
	std::function<void()> m_callback;
};

class EditorComponent
{
public:
	EditorComponent() = default;
	
	virtual void Update(float dt, const EditorState& editorState) { };
	virtual bool UpdateInput(float dt, const EditorState& editorState) { return false; };
	virtual void EarlyDraw(class PrimitiveRenderer& primitiveRenderer) const { }
	virtual void LateDraw() const { }
	virtual void RenderSettings(const EditorState& editorState) { }
	virtual bool CollectIcons(const EditorState& editorState, std::vector<EditorIcon>& icons) { return false; }
	
	template <typename T>
	static T SnapToGrid(const T& pos)
	{
		const float STEP = eg::IsButtonDown(eg::Button::LeftAlt) ? 0.5f : 0.1f;
		return glm::round(pos / STEP) * STEP;
	}
};
