#pragma once

#include "../World/World.hpp"
#include "../World/PrepareDrawArgs.hpp"
#include "../Graphics/RenderContext.hpp"
#include "../GameState.hpp"
#include "EditorCamera.hpp"
#include "PrimitiveRenderer.hpp"
#include "LiquidPlaneRenderer.hpp"
#include "../World/Entities/EntTypes/ActivationLightStripEnt.hpp"

class Editor : public GameState
{
public:
	explicit Editor(RenderContext& renderCtx);
	
	void RunFrame(float dt) override;
	
	void SetResolution(int width, int height);
	
private:
	template <typename CallbackTp>
	void IterateSelection(CallbackTp callback);
	
	void InitWorld();
	
	void DrawWorld();
	
	void DrawMenuBar();
	
	void Save();
	
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
	
	LiquidPlaneRenderer m_liquidPlaneRenderer;
	
	PrimitiveRenderer m_primRenderer;
	eg::SpriteBatch m_spriteBatch;
	
	std::vector<const EntType*> m_spawnEntityList;
	
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
	
	eg::Ray m_viewRay;
	
	WallRayIntersectResult m_spawnEntityPickResult;
	
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
	
	enum class IconType
	{
		Entity,
		ActTarget,
		ActPathNewPoint,
		ActPathExistingPoint
	};
	
	struct EntityIcon
	{
		eg::Rectangle rectangle;
		float depth;
		std::weak_ptr<Ent> entity;
		IconType type;
		int actConnectionIndex;
		int wayPointIndex;
	};
	std::vector<EntityIcon> m_entityIcons;
	std::vector<std::weak_ptr<Ent>> m_selectedEntities;
	
	std::weak_ptr<Ent> m_connectingActivator;
	
	glm::vec3 m_lightStripInsertPos;
	Dir m_lightStripInsertNormal;
	
	std::weak_ptr<ActivationLightStripEnt> m_editingLightStripEntity;
	int m_editingWayPointIndex = -1;
	
	glm::vec3 m_gizmoPosUnaligned;
	glm::vec3 m_prevGizmoPos;
	eg::TranslationGizmo m_translationGizmo;
	bool m_entitiesCloned = false;
	
	glm::ivec2 m_mouseDownPos;
	bool m_isDraggingWallEntity = false;
	
	bool m_levelSettingsOpen = false;
};

extern Editor* editor;
