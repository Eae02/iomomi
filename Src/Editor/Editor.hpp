#pragma once

#include "../World/World.hpp"
#include "../World/PrepareDrawArgs.hpp"
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
	void UpdateToolEntities(float dt);
	
	void DrawToolWalls();
	void DrawToolCorners();
	void DrawToolEntities();
	
	RenderContext* m_renderCtx;
	PrepareDrawArgs m_prepareDrawArgs;
	
	std::string m_newLevelName;
	
	std::string m_levelName;
	std::unique_ptr<World> m_world;
	
	eg::PerspectiveProjection m_projection;
	EditorCamera m_camera;
	
	PrimitiveRenderer m_primRenderer;
	eg::SpriteBatch m_spriteBatch;
	
	enum class Tool
	{
		Walls,
		Corners,
		Entities
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
	
	struct EntityIcon
	{
		eg::Rectangle rectangle;
		float depth;
		std::shared_ptr<Entity> entity;
	};
	std::vector<EntityIcon> m_entityIcons;
	std::vector<std::shared_ptr<Entity>> m_selectedEntities;
	std::vector<std::weak_ptr<Entity>> m_settingsWindowEntities;
	
	glm::vec3 m_gizmoPosUnaligned;
	glm::vec3 m_prevGizmoPos;
	eg::TranslationGizmo m_translationGizmo;
	bool m_entitiesCloned = false;
};

extern Editor* editor;
