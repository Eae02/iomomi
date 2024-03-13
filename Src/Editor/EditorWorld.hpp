#pragma once

#include <EGame/FlyCamera.hpp>

#include "../GameState.hpp"
#include "../Graphics/RenderContext.hpp"
#include "../World/PrepareDrawArgs.hpp"
#include "../World/World.hpp"
#include "EditorCamera.hpp"
#include "EditorComponent.hpp"
#include "LiquidPlaneRenderer.hpp"
#include "PrimitiveRenderer.hpp"
#include "SelectionRenderer.hpp"

class EditorWorld
{
public:
	EditorWorld(std::string levelName, std::unique_ptr<World> world);

	void Update(float dt, EditorTool currentTool);

	void Draw(EditorTool currentTool, RenderContext& renderCtx, PrepareDrawArgs prepareDrawArgs);

	void RenderToolSettings(EditorTool currentTool);

	void RenderLevelSettings();

	void Save();

	void SetWindowRect(const eg::Rectangle& windowRect);

	void RenderImGui();

	const std::string& Name() const { return m_levelName; }

	World& GetWorld() const { return *m_world; }

	void TestLevel();

	bool isWindowHovered = false;
	bool isWindowVisisble = false;
	bool isWindowFocused = false;

	bool shouldClose = false;

	eg::Texture renderTexture;

	uint32_t uid;

private:
	void ResetCamera();

	void UpdateRenderSettingsAndViewMatrices(const glm::mat4& viewMatrix, const glm::mat4& invViewMatrix);

	bool IsEntitySelected(const Ent* entity) const;

	eg::Texture m_renderTextureDepth;
	eg::Framebuffer m_framebuffer;

	SelectionRenderer m_selectionRenderer;

	EditorState m_editorState;

	std::string m_levelName;
	std::shared_ptr<World> m_world;

	LiquidPlaneRenderer m_liquidPlaneRenderer;
	PrimitiveRenderer m_primRenderer;
	eg::SpriteBatch m_spriteBatch;

	EditorCamera m_camera;

	std::unique_ptr<struct EditorComponentsSet, void (*)(struct EditorComponentsSet*)> m_components;
	std::vector<EditorComponent*> m_componentsForTool[EDITOR_NUM_TOOLS];

	std::vector<EditorIcon> m_icons;
	int m_hoveredIcon = -1;
	std::vector<std::weak_ptr<Ent>> m_selectedEntities;

	int m_previousSumOfWaterBlockedVersion = -1;

	bool m_levelHasEntrance = false;

	bool m_drawVoxelGrid = true;

	bool m_isUpdatingThumbnailView = false;
	eg::FlyCamera m_thumbnailCamera;

	float m_savedTextTimer = 0;
};
