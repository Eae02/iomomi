#ifndef IOMOMI_NO_EDITOR
#include "Editor.hpp"
#include "../Levels.hpp"
#include "../MainGameState.hpp"
#include "../World/Entities/Components/ActivatorComp.hpp"
#include "../ImGuiInterface.hpp"
#include "../ThumbnailRenderer.hpp"
#include "../MainMenuGameState.hpp"

#include <fstream>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

Editor* editor;

Editor::Editor(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx)
{
	m_prepareDrawArgs.isEditor = true;
	m_prepareDrawArgs.meshBatch = &renderCtx.meshBatch;
	m_prepareDrawArgs.transparentMeshBatch = &renderCtx.transparentMeshBatch;
	m_prepareDrawArgs.player = nullptr;
}

static inline std::unique_ptr<World> CreateEmptyWorld()
{
	constexpr int NEW_LEVEL_WALL_TEXTURE = 6;
	
	std::unique_ptr<World> world = std::make_unique<World>();
	
	for (int x = 0; x < 3; x++)
	{
		for (int y = 0; y < 3; y++)
		{
			for (int z = 0; z < 3; z++)
			{
				world->voxels.SetIsAir({x, y, z}, true);
				for (int s = 0; s < 6; s++)
				{
					world->voxels.SetMaterialSafe({x, y, z}, (Dir)s, NEW_LEVEL_WALL_TEXTURE);
				}
			}
		}
	}
	
	return world;
}

void Editor::RunFrame(float dt)
{
	eg::SetRelativeMouseMode(false);
	
	RenderSettings::instance->gameTime += dt;
	
	//Sets up the root docking window
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGuiWindowFlags rootWindowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove
		 | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
		 | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	ImGui::Begin("rootDockWindow", nullptr, rootWindowFlags);
	ImGui::PopStyleVar(2);
	const ImGuiID dockSpaceID = ImGui::GetID("rootDockSpace");
	ImGui::DockSpace(dockSpaceID, ImVec2(0.0f, 0.0f));
	ImGui::End();
	
	for (size_t i = 0; i < m_worlds.size();)
	{
		std::string title = m_worlds[i]->Name() + "###Level" + std::to_string(m_worlds[i]->uid);
		bool open = true;
		
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		
		ImGui::SetNextWindowDockID(dockSpaceID, ImGuiCond_Appearing);
		m_worlds[i]->isWindowVisisble = ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_MenuBar);
		m_worlds[i]->isWindowFocused = ImGui::IsWindowFocused();
		m_worlds[i]->isWindowHovered = ImGui::IsWindowHovered();
		
		ImGui::PopStyleVar();
		
		if (m_worlds[i]->isWindowFocused)
		{
			m_currentWorld = m_worlds[i].get();
		}
		if (m_worlds[i]->isWindowVisisble)
		{
			m_worlds[i]->RenderImGui();
			
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const glm::vec2 imguiCursorPos = ImGui::GetCursorScreenPos();
			
			const eg::Rectangle windowRect(
				imguiCursorPos.x,
				imguiCursorPos.y, 
				ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x,
				ImGui::GetWindowHeight());
			m_worlds[i]->SetWindowRect(windowRect);
			
			const bool flipY = eg::CurrentGraphicsAPI() == eg::GraphicsAPI::OpenGL;
			drawList->AddImage(MakeImTextureID(m_worlds[i]->renderTexture),
			                   imguiCursorPos, windowRect.Max(),
			                   ImVec2(0, flipY ? 1 : 0), ImVec2(1, flipY ? 0 : 1));
		}
		
		ImGui::End();
		
		if (!open || m_worlds[i]->shouldClose)
		{
			if (m_currentWorld == m_worlds[i].get())
				m_currentWorld = nullptr;
			m_worlds.erase(m_worlds.begin() + i);
		}
		else
		{
			i++;
		}
	}
	
	// ** Level select window **
	if (m_levelSelectWindowOpen)
	{
		ImGui::SetNextWindowSize(ImVec2(400, eg::CurrentResolutionY() * 0.8f), ImGuiCond_Once);
		if (ImGui::Begin("Select Level", &m_levelSelectWindowOpen, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::Text("New Level");
			ImGui::InputText("Name", &m_newLevelName);
			if (ImGui::Button("Create") && !m_newLevelName.empty())
			{
				m_worlds.push_back(std::make_unique<EditorWorld>(std::move(m_newLevelName), CreateEmptyWorld()));
				m_newLevelName = {};
			}
			
			ImGui::Separator();
			
			auto DrawLevelButton = [&] (const Level& level)
			{
				ImGui::PushID(&level);
				std::string label = eg::Concat({level.name, "###L"});
				if (ImGui::MenuItem(label.c_str()))
				{
					std::string path = GetLevelPath(level.name);
					std::ifstream stream(path, std::ios::binary);
					
					if (std::unique_ptr<World> world = World::Load(stream, true))
					{
						m_worlds.push_back(std::make_unique<EditorWorld>(level.name, std::move(world)));
					}
				}
				
				if (level.thumbnail.handle != nullptr && ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Image(MakeImTextureID(level.thumbnail), ImVec2(LEVEL_THUMBNAIL_RES_X / 2.0f, LEVEL_THUMBNAIL_RES_Y / 2.0f));
					ImGui::EndTooltip();
				}
				
				ImGui::PopID();
			};
			
			if (ImGui::CollapsingHeader("Game Levels", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (std::string_view levelName : levelsOrder)
				{
					int64_t index = FindLevel(levelName);
					if (index != -1)
					{
						DrawLevelButton(levels[index]);
					}
				}
			}
			
			if (ImGui::CollapsingHeader("Extra Levels", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const Level& level : levels)
				{
					if (level.isExtra)
					{
						DrawLevelButton(level);
					}
				}
			}
		}
		ImGui::End();
	}
	
	// ** Level settings window **
	if (m_levelSettingsWindowOpen)
	{
		ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_FirstUseEver);
		std::string levelSettingsTitle = "Level Settings";
		if (m_currentWorld != nullptr)
			levelSettingsTitle += " - " + m_currentWorld->Name();
		levelSettingsTitle += "###LevelSettingsWindow";
		if (ImGui::Begin(levelSettingsTitle.c_str(), &m_levelSettingsWindowOpen) && m_currentWorld != nullptr)
		{
			m_currentWorld->RenderLevelSettings();
		}
		ImGui::End();
	}
	
	for (int i = 0; i < (int)EDITOR_NUM_TOOLS; i++)
	{
		if (eg::IsButtonDown((eg::Button)((int)eg::Button::F1 + i)))
			m_tool = (EditorTool)i;
	}
	
	ImGui::Begin("Tools");
	ImGui::Spacing();
	if (ImGui::CollapsingHeader("Tools", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::RadioButton("Walls (F1)", reinterpret_cast<int*>(&m_tool), (int)EditorTool::Walls);
		ImGui::RadioButton("Corners (F2)", reinterpret_cast<int*>(&m_tool), (int)EditorTool::Corners);
		ImGui::RadioButton("Entities (F3)", reinterpret_cast<int*>(&m_tool), (int)EditorTool::Entities);
	}
	if (m_currentWorld != nullptr)
	{
		m_currentWorld->RenderToolSettings(m_tool);
	}
	ImGui::End();
	
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			const std::string saveCurrentText = m_currentWorld ?
				"Save " + m_currentWorld->Name() + "###SaveCur" :
				"Save Current###SaveCur";
			if (ImGui::MenuItem(saveCurrentText.c_str(), nullptr, false, m_currentWorld != nullptr))
			{
				m_currentWorld->Save();
			}
			
			if (ImGui::MenuItem("Save All", nullptr, false, !m_worlds.empty()))
			{
				for (const std::unique_ptr<EditorWorld>& world : m_worlds)
				{
					world->Save();
				}
			}
			
			ImGui::Separator();
			
			if (ImGui::MenuItem("Close All", nullptr, false, !m_worlds.empty()))
			{
				m_worlds.clear();
				m_currentWorld = nullptr;
			}
			
			ImGui::Separator();
			
			if (ImGui::MenuItem("Exit to Main Menu"))
			{
				SetCurrentGS(mainMenuGameState);
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Level Settings"))
				m_levelSettingsWindowOpen = true;
			if (ImGui::MenuItem("Level Select"))
				m_levelSelectWindowOpen = true;
			
			ImGui::Separator();
			ImGui::TextDisabled("Icons");
			
			for (int entityType = 0; entityType < (int)EntTypeID::MAX; entityType++)
			{
				const EntType* type = GetEntityType((EntTypeID)entityType);
				if (type != nullptr && eg::HasFlag(type->flags, EntTypeFlags::OptionalEditorIcon))
				{
					ImGui::MenuItem(type->prettyName.c_str(), nullptr, &settings.edEntityIconEnabled[entityType]);
				}
			}
			
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	
	for (const std::unique_ptr<EditorWorld>& world : m_worlds)
	{
		if (world->isWindowVisisble)
		{
			world->Update(dt, m_tool);
			world->Draw(m_tool, *m_renderCtx, m_prepareDrawArgs);
		}
	}
}

#endif
