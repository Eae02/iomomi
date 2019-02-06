#pragma once

#include "../World/World.hpp"
#include "../Graphics/RenderContext.hpp"
#include "../GameState.hpp"
#include "EditorCamera.hpp"
#include "PrimitiveRenderer.hpp"

class Editor : public GameState
{
public:
	explicit Editor(RenderContext& renderCtx);
	
	void RunFrame(float dt) override;
	
	void SetResolution(int width, int height);
	
private:
	void DrawWorld();
	
	void DrawMenuBar();
	
	void UpdateToolWalls(float dt);
	void UpdateToolCorners(float dt);
	
	void DrawToolWalls();
	void DrawToolCorners();
	
	RenderContext* m_renderCtx;
	std::string m_newLevelName;
	
	std::string m_levelName;
	std::unique_ptr<World> m_world;
	
	eg::PerspectiveProjection m_projection;
	EditorCamera m_camera;
	
	PrimitiveRenderer m_primRenderer;
	
	enum class Tool
	{
		Walls,
		Corners,
		Objects
	};
	Tool m_tool = Tool::Walls;
	
	enum class SelState
	{
		NoSelection,
		Selecting,
		SelectDone,
		Dragging
	};
	
	SelState m_selState = SelState::NoSelection;
	glm::ivec3 m_selection1;
	glm::ivec3 m_selection2;
	glm::vec3 m_selection2Anim;
	Dir m_selectionNormal;
	
	int m_hoveredCornerDim = -1;
	glm::ivec3 m_hoveredCornerPos;
	int m_modCornerDim = -1;
	glm::ivec3 m_modCornerPos;
	
	glm::vec3 m_dragStartPos;
	int m_dragDistance;
	int m_dragDir;
	int m_dragAirMode;
};

extern Editor* editor;
