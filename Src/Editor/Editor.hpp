#pragma once

#include <EGame/FlyCamera.hpp>

#include "../GameState.hpp"
#include "../Graphics/RenderContext.hpp"
#include "../World/Entities/EntTypes/Activation/ActivationLightStripEnt.hpp"
#include "../World/PrepareDrawArgs.hpp"
#include "../World/World.hpp"
#include "EditorCamera.hpp"
#include "EditorComponent.hpp"
#include "EditorWorld.hpp"
#include "LiquidPlaneRenderer.hpp"
#include "PrimitiveRenderer.hpp"

class Editor : public GameState
{
public:
	explicit Editor(RenderContext& renderCtx);

	void RunFrame(float dt) override;

private:
	bool m_levelSelectWindowOpen = true;
	bool m_levelSettingsWindowOpen = true;

	std::vector<std::unique_ptr<EditorWorld>> m_worlds;
	EditorWorld* m_currentWorld = nullptr;

	RenderContext* m_renderCtx;
	PrepareDrawArgs m_prepareDrawArgs;

	std::string m_newLevelName;

	EditorTool m_tool = EditorTool::Walls;
};

extern Editor* editor;
