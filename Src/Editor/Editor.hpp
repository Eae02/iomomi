#pragma once

#include <EGame/FlyCamera.hpp>

#include "../GameState.hpp"
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
	Editor();

	void RunFrame(float dt) override;

private:
	bool m_levelSelectWindowOpen = true;
	bool m_levelSettingsWindowOpen = true;

	std::vector<std::unique_ptr<EditorWorld>> m_worlds;
	EditorWorld* m_currentWorld = nullptr;

	std::unique_ptr<eg::MeshBatch> m_meshBatch;
	std::unique_ptr<eg::MeshBatchOrdered> m_transparentMeshBatch;

	PrepareDrawArgs m_prepareDrawArgs;

	std::string m_newLevelName;

	EditorTool m_tool = EditorTool::Walls;
};

extern Editor* editor;
