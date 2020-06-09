#include "Editor.hpp"
#include "../Levels.hpp"
#include "../MainGameState.hpp"
#include "../Graphics/Materials/MeshDrawArgs.hpp"
#include "../World/Entities/Components/ActivatorComp.hpp"
#include "../World/Entities/Components/ActivatableComp.hpp"
#include "Components/EntityEditorComponent.hpp"
#include "Components/SpawnEntityEditorComponent.hpp"
#include "Components/LightStripEditorComponent.hpp"
#include "Components/GravityCornerEditorComponent.hpp"
#include "Components/WallDragEditorComponent.hpp"
#include "Components/AAQuadDragEditorComponent.hpp"

#include <fstream>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

Editor* editor;

struct EditorComponentsSet
{
	EntityEditorComponent entity;
	SpawnEntityEditorComponent spawnEntity;
	LightStripEditorComponent lightStrip;
	GravityCornerEditorComponent gravityCorner;
	WallDragEditorComponent wallDrag;
	AAQuadDragEditorComponent aaQuadDrag;
};

void EditorComponentDeleter(EditorComponentsSet* s)
{
	delete s;
}

Editor::Editor(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx), m_components(new EditorComponentsSet, &EditorComponentDeleter)
{
	m_prepareDrawArgs.isEditor = true;
	m_prepareDrawArgs.meshBatch = &renderCtx.meshBatch;
	m_prepareDrawArgs.player = nullptr;
	
	m_projection.SetFieldOfViewDeg(75.0f);
	
	m_componentsForTool[(int)EditorTool::Entities].push_back(&m_components->lightStrip);
	m_componentsForTool[(int)EditorTool::Entities].push_back(&m_components->aaQuadDrag);
	m_componentsForTool[(int)EditorTool::Entities].push_back(&m_components->spawnEntity);
	m_componentsForTool[(int)EditorTool::Entities].push_back(&m_components->entity);
	
	m_componentsForTool[(int)EditorTool::Walls].push_back(&m_components->wallDrag);
	
	m_componentsForTool[(int)EditorTool::Corners].push_back(&m_components->gravityCorner);
}

void Editor::InitWorld()
{
	m_world->entManager.isEditor = true;
	m_selectedEntities.clear();
	ActivationLightStripEnt::GenerateAll(*m_world);
}

static constexpr int NEW_LEVEL_WALL_TEXTURE = 6;

void Editor::RunFrame(float dt)
{
	if (m_world == nullptr)
	{
		ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_Once);
		if (ImGui::Begin("Select Level", nullptr, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::Text("New Level");
			ImGui::InputText("Name", &m_newLevelName);
			if (ImGui::Button("Create") && !m_newLevelName.empty())
			{
				m_levelName = std::move(m_newLevelName);
				m_newLevelName = { };
				m_world = std::make_unique<World>();
				InitWorld();
				
				for (int x = 0; x < 3; x++)
				{
					for (int y = 0; y < 3; y++)
					{
						for (int z = 0; z < 3; z++)
						{
							m_world->SetIsAir({x, y, z}, true);
							for (int s = 0; s < 6; s++)
							{
								m_world->SetMaterialSafe({x, y, z}, (Dir)s, NEW_LEVEL_WALL_TEXTURE);
							}
						}
					}
				}
			}
			
			ImGui::Separator();
			ImGui::Text("Open Level");
			
			for (const Level& level : levels)
			{
				ImGui::PushID(&level);
				std::string label = eg::Concat({ level.name, "###L" });
				if (ImGui::MenuItem(label.c_str()))
				{
					std::string path = GetLevelPath(level.name);
					std::ifstream stream(path, std::ios::binary);
					
					if (std::unique_ptr<World> world = World::Load(stream, true))
					{
						m_world = std::move(world);
						m_levelName = level.name;
						InitWorld();
					}
				}
				ImGui::PopID();
			}
		}
		
		ImGui::End();
		return;
	}
	
	DrawMenuBar();
	
	if (m_world == nullptr)
		return;
	
	if (m_levelSettingsOpen)
	{
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Level Settings", &m_levelSettingsOpen))
		{
			ImGui::Checkbox("Has Gravity Gun", &m_world->playerHasGravityGun);
			ImGui::InputText("Title", &m_world->title);
		}
		ImGui::End();
	}
	
	WorldUpdateArgs entityUpdateArgs = { };
	entityUpdateArgs.dt = dt;
	entityUpdateArgs.world = m_world.get();
	m_world->Update(entityUpdateArgs);
	
	
	EditorState editorState;
	editorState.world = m_world.get();
	editorState.camera = &m_camera;
	editorState.projection = &m_projection;
	editorState.selectedEntities = &m_selectedEntities;
	editorState.tool = m_tool;
	
	
	glm::mat4 viewMatrix, inverseViewMatrix;
	m_camera.GetViewMatrix(viewMatrix, inverseViewMatrix);
	
	RenderSettings::instance->viewProjection = m_projection.Matrix() * viewMatrix;
	RenderSettings::instance->invViewProjection = inverseViewMatrix * m_projection.InverseMatrix();
	RenderSettings::instance->cameraPosition = glm::vec3(inverseViewMatrix[3]);
	RenderSettings::instance->gameTime += dt;
	RenderSettings::instance->UpdateBuffer();
	
	editorState.viewRay = eg::Ray::UnprojectScreen(RenderSettings::instance->invViewProjection, eg::CursorPos());
	
	
	ImGui::SetNextWindowPos(ImVec2(0, eg::CurrentResolutionY() / 2.0f), ImGuiCond_Always, ImVec2(0, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(250, eg::CurrentResolutionY() * 0.5f), ImGuiCond_Once);
	ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
	             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
	if (ImGui::Button("Test Level (F5)", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
	{
		std::stringstream stream;
		m_world->Save(stream);
		stream.seekg(0, std::ios::beg);
		mainGameState->LoadWorld(stream);
		SetCurrentGS(mainGameState);
		ImGui::End();
		return;
	}
	ImGui::Spacing();
	if (ImGui::CollapsingHeader("Tools", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::RadioButton("Walls", reinterpret_cast<int*>(&m_tool), (int)EditorTool::Walls);
		ImGui::RadioButton("Corners", reinterpret_cast<int*>(&m_tool), (int)EditorTool::Corners);
		ImGui::RadioButton("Entities", reinterpret_cast<int*>(&m_tool), (int)EditorTool::Entities);
	}
	for (EditorComponent* comp : m_componentsForTool[(int)m_tool])
	{
		comp->RenderSettings(editorState);
	}
	ImGui::End();
	
	for (EditorComponent* comp : m_componentsForTool[(int)m_tool])
	{
		comp->Update(dt, editorState);
	}
	
	bool canUpdateInput = !ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard && !eg::console::IsShown();
	if (canUpdateInput)
	{
		m_camera.Update(dt);
		for (EditorComponent* comp : m_componentsForTool[(int)m_tool])
		{
			if (comp->UpdateInput(dt, editorState))
			{
				canUpdateInput = false;
				break;
			}
		}
	}
	
	m_icons.clear();
	for (EditorComponent* comp : m_componentsForTool[(int)m_tool])
	{
		if (comp->CollectIcons(editorState, m_icons))
			break;
	}
	std::sort(m_icons.begin(), m_icons.end(), [&] (const EditorIcon& a, const EditorIcon& b)
	{
		return a.m_depth > b.m_depth;
	});
	
	glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	m_hoveredIcon = -1;
	for (size_t i = 0; i < m_icons.size(); i++)
	{
		if (m_icons[i].m_rectangle.Contains(flippedCursorPos))
			m_hoveredIcon = i;
	}
	
	if (canUpdateInput && eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
	{
		bool shouldClearSelected = !eg::IsButtonDown(eg::Button::LeftControl) && !eg::IsButtonDown(eg::Button::RightControl);
		
		if (m_hoveredIcon != -1)
		{
			if (m_icons[m_hoveredIcon].shouldClearSelection && shouldClearSelected)
				m_selectedEntities.clear();
			m_icons[m_hoveredIcon].m_callback();
		}
		else if (shouldClearSelected)
			m_selectedEntities.clear();
	}
	
	DrawWorld();
}

void Editor::DrawWorld()
{
	m_primRenderer.Begin(RenderSettings::instance->viewProjection, RenderSettings::instance->cameraPosition);
	m_spriteBatch.Begin();
	m_renderCtx->meshBatch.Begin();
	m_renderCtx->transparentMeshBatch.Begin();
	
	m_prepareDrawArgs.spotLights.clear();
	m_prepareDrawArgs.pointLights.clear();
	m_prepareDrawArgs.reflectionPlanes.clear();
	m_world->PrepareForDraw(m_prepareDrawArgs);
	
	for (EditorComponent* comp : m_componentsForTool[(int)m_tool])
	{
		comp->EarlyDraw(m_primRenderer);
	}
	
	//Draws icons
	const eg::Texture& iconsTexture = eg::GetAsset<eg::Texture>("Textures/EntityIcons.png");
	auto CreateSrcRectangle = [&] (int index)
	{
		return eg::Rectangle(index * 50, 0, 50, 50);
	};
	for (size_t i = 0; i < m_icons.size(); i++)
	{
		if (m_icons[i].hideIfNotHovered && (int)i != m_hoveredIcon)
			continue;
		
		eg::ColorLin color(eg::Color::White);
		
		//Draws the background sprite
		m_spriteBatch.Draw(iconsTexture, m_icons[i].m_rectangle, color,
			CreateSrcRectangle(m_icons[i].selected ? 1 : ((int)i == m_hoveredIcon ? 10 : 0)), eg::SpriteFlags::None);
		
		//Draws the icon
		m_spriteBatch.Draw(iconsTexture, m_icons[i].m_rectangle, color,
			CreateSrcRectangle(m_icons[i].iconIndex), eg::SpriteFlags::None);
	}
	
	//Sends the editor draw message to entities
	EntEditorDrawArgs drawArgs;
	drawArgs.spriteBatch = &m_spriteBatch;
	drawArgs.primitiveRenderer = &m_primRenderer;
	drawArgs.meshBatch = &m_renderCtx->meshBatch;
	drawArgs.transparentMeshBatch = &m_renderCtx->transparentMeshBatch;
	drawArgs.world = m_world.get();
	drawArgs.getDrawMode = [this] (const Ent* entity) -> EntEditorDrawMode
	{
		for (const std::weak_ptr<Ent>& selEnt : m_selectedEntities)
		{
			if (selEnt.lock().get() == entity)
				return EntEditorDrawMode::Selected;
		}
		return EntEditorDrawMode::Default;
	};
	m_world->entManager.ForEachWithFlag(EntTypeFlags::EditorDrawable, [&] (Ent& entity)
	{
		entity.EditorDraw(drawArgs);
	});
	
	m_primRenderer.End();
	
	m_renderCtx->meshBatch.End(eg::DC);
	m_renderCtx->transparentMeshBatch.End(eg::DC);
	
	m_liquidPlaneRenderer.Prepare(*m_world, m_renderCtx->transparentMeshBatch, RenderSettings::instance->cameraPosition);
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = nullptr;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(eg::ColorSRGB::FromHex(0x1C1E26));
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	m_world->DrawEditor();
	
	MeshDrawArgs mDrawArgs;
	mDrawArgs.drawMode = MeshDrawMode::Editor;
	m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
	
	m_primRenderer.Draw();
	
	m_renderCtx->transparentMeshBatch.Draw(eg::DC, &mDrawArgs);
	
	m_liquidPlaneRenderer.Render();
	
	for (EditorComponent* comp : m_componentsForTool[(int)m_tool])
	{
		comp->LateDraw();
	}
	
	eg::DC.EndRenderPass();
	
	eg::RenderPassBeginInfo spriteRPBeginInfo;
	spriteRPBeginInfo.framebuffer = nullptr;
	spriteRPBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	m_spriteBatch.End(eg::CurrentResolutionX(), eg::CurrentResolutionY(), spriteRPBeginInfo);
}

void Editor::Save()
{
	std::string savePath = eg::Concat({ eg::ExeDirPath(), "/levels/", m_levelName, ".gwd" });
	std::ofstream stream(savePath, std::ios::binary);
	m_world->Save(stream);
	InitLevels();
}

void Editor::DrawMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save"))
			{
				Save();
			}
			
			if (ImGui::MenuItem("Close"))
			{
				m_levelName.clear();
				m_world = nullptr;
			}
			
			if (ImGui::MenuItem("Save and Close"))
			{
				Save();
				m_levelName.clear();
				m_world = nullptr;
			}
			
			ImGui::EndMenu();
		}
		
		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Level Settings"))
			{
				m_levelSettingsOpen = true;
			}
			
			ImGui::EndMenu();
		}
		
		ImGui::EndMainMenuBar();
	}
}

void Editor::SetResolution(int width, int height)
{
	m_projection.SetResolution(width, height);
}
