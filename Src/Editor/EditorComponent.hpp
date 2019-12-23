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
	
	const eg::Rectangle& Rectangle() const { return m_rectangle; }
	
private:
	eg::Rectangle m_rectangle;
	float m_depth;
	std::function<void()> m_callback;
};

class EditorComponent
{
public:
	EditorComponent() = default;
	
	virtual void Update(float dt, const EditorState& editorState) = 0;
	virtual bool UpdateInput(float dt, const EditorState& editorState) = 0;
	virtual void EarlyDraw(class PrimitiveRenderer& primitiveRenderer) const { }
	virtual void LateDraw() const { }
	virtual void RenderSettings(const EditorState& editorState) { }
	virtual bool CollectIcons(const EditorState& editorState, std::vector<EditorIcon>& icons) { return false; }
	
	static glm::vec3 SnapToGrid(const glm::vec3& pos);
};
