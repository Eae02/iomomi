#pragma once

#include "../World/World.hpp"
#include "../World/PrepareDrawArgs.hpp"
#include "../Graphics/RenderContext.hpp"
#include "../GameState.hpp"
#include "../World/Entities/EntTypes/ActivationLightStripEnt.hpp"
#include "EditorCamera.hpp"
#include "PrimitiveRenderer.hpp"
#include "LiquidPlaneRenderer.hpp"
#include "EditorComponent.hpp"

#include <EGame/FlyCamera.hpp>

class Editor : public GameState
{
public:
	explicit Editor(RenderContext& renderCtx);
	
	void RunFrame(float dt) override;
	
	void SetResolution(int width, int height);
	
	eg::ColorSRGB SSRFallbackColor() const { return m_world->ssrFallbackColor; }
	void SetSSRFallbackColor(const eg::ColorSRGB& color) { m_world->ssrFallbackColor = color; }
	
private:
	void InitWorld();
	
	void DrawWorld();
	
	void DrawMenuBar();
	
	void LoadLevel(const struct Level& level);
	void Save();
	void Close();
	
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
	
	int m_previousSumOfWaterBlockedVersion = -1;
	
	bool m_isUpdatingThumbnailView = false;
	eg::FlyCamera m_thumbnailCamera;
	
	float m_savedTextTimer = 0;
};

extern Editor* editor;
