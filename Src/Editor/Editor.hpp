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
	
private:
	void DrawWorld();
	
	RenderContext* m_renderCtx;
	std::string m_newLevelName;
	std::unique_ptr<World> m_world;
	
	EditorCamera m_camera;
	
	PrimitiveRenderer m_primRenderer;
	
	enum SelState
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
	
	glm::vec3 m_dragStartPos;
	int m_dragDistance;
};

extern std::unique_ptr<Editor> editor;
