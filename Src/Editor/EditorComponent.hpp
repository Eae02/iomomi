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

class EditorIcon
{
public:
	friend class Editor;
	friend class EditorWorld;
	friend class EditorState;
	
	int iconIndex = 5;
	bool selected = false;
	bool shouldClearSelection = true;
	bool hideIfNotHovered = false;
	
	const eg::Rectangle& Rectangle() const { return m_rectangle; }
	
	void ApplyDepthBias(float bias) { m_depth -= bias; }
	
	void InvokeCallback() const { m_callback(); }
	
private:
	EditorIcon() = default;
	
	eg::Rectangle m_rectangle;
	float m_depth;
	bool m_behindScreen;
	std::function<void()> m_callback;
};

struct EditorState
{
	World* world;
	const EditorCamera* camera;
	glm::vec3 cameraPosition;
	eg::PerspectiveProjection projection;
	eg::Ray viewRay;
	std::vector<std::weak_ptr<Ent>>* selectedEntities;
	EditorTool tool;
	
	class SelectionRenderer* selectionRenderer;
	class PrimitiveRenderer* primitiveRenderer;
	
	glm::mat4 viewProjection;
	glm::mat4 inverseViewProjection;
	
	glm::vec2 windowCursorPos;
	eg::Rectangle windowRect;
	
	void InvalidateWater() const;
	
	bool IsEntitySelected(const Ent& entity) const;
	
	void EntityMoved(Ent& entity) const;
	
	EditorIcon CreateIcon(const glm::vec3& worldPos, std::function<void()> callback) const;
};

constexpr float SMALL_GRID_SNAP = 0.1f;
constexpr float BIG_GRID_SNAP = 0.5f;

template <typename T>
static T SnapToGrid(const T& pos, float step)
{
	return glm::round(pos / step) * step;
}

template <typename T>
static T SnapToGrid(const T& pos)
{
	if (eg::InputState::Current().IsAltDown())
		return SnapToGrid(pos, BIG_GRID_SNAP);
	else if (!eg::InputState::Current().IsCtrlDown())
		return SnapToGrid(pos, SMALL_GRID_SNAP);
	return pos;
}

class EditorComponent
{
public:
	EditorComponent() = default;
	
	virtual void Update(float dt, const EditorState& editorState) { };
	virtual bool UpdateInput(float dt, const EditorState& editorState) { return false; };
	virtual void EarlyDraw(const EditorState& editorState) const { }
	virtual void LateDraw(const EditorState& editorState) const { }
	virtual void RenderSettings(const EditorState& editorState) { }
	virtual bool CollectIcons(const EditorState& editorState, std::vector<EditorIcon>& icons) { return false; }
};
