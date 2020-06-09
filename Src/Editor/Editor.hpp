#pragma once

#include "../World/World.hpp"
#include "../World/PrepareDrawArgs.hpp"
#include "../Graphics/RenderContext.hpp"
#include "../GameState.hpp"
#include "EditorCamera.hpp"
#include "PrimitiveRenderer.hpp"
#include "LiquidPlaneRenderer.hpp"
#include "../World/Entities/EntTypes/ActivationLightStripEnt.hpp"
#include "EditorComponent.hpp"

class Editor : public GameState
{
public:
	explicit Editor(RenderContext& renderCtx);
	
	void RunFrame(float dt) override;
	
	void SetResolution(int width, int height);
	
private:
	template <typename CallbackTp>
	void IterateSelection(CallbackTp callback);
	
	bool IsSelected(const Ent& entity) const;
	
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
	
	EditorTool m_tool = EditorTool::Walls;
	
	std::unique_ptr<struct EditorComponentsSet, void(*)(struct EditorComponentsSet*)> m_components;
	std::vector<EditorComponent*> m_componentsForTool[EDITOR_NUM_TOOLS]; 
	
	std::vector<EditorIcon> m_icons;
	int m_hoveredIcon = -1;
	std::vector<std::weak_ptr<Ent>> m_selectedEntities;
	
	bool m_levelSettingsOpen = false;
};

extern Editor* editor;
