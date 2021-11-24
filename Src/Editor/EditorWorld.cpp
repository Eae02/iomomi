#ifndef IOMOMI_NO_EDITOR
#include "EditorWorld.hpp"
#include "Editor.hpp"
#include "Components/EntityEditorComponent.hpp"
#include "Components/SpawnEntityEditorComponent.hpp"
#include "Components/LightStripEditorComponent.hpp"
#include "Components/GravityCornerEditorComponent.hpp"
#include "Components/WallDragEditorComponent.hpp"
#include "Components/AAQuadDragEditorComponent.hpp"
#include "../World/Entities/Components/WaterBlockComp.hpp"
#include "../World/Entities/Components/ActivatorComp.hpp"
#include "../World/Entities/EntTypes/EntranceExitEnt.hpp"
#include "../Graphics/Materials/MeshDrawArgs.hpp"
#include "../MainGameState.hpp"
#include "../Gui/GuiCommon.hpp"
#include "../Levels.hpp"

#include <fstream>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <imgui_internal.h>
#include <magic_enum.hpp>

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

static uint32_t nextUID = 0;

EditorWorld::EditorWorld(std::string levelName, std::unique_ptr<World> world)
	: m_levelName(std::move(levelName)), m_world(std::move(world)),
	  m_components(new EditorComponentsSet, &EditorComponentDeleter)
{
	m_componentsForTool[(int)EditorTool::Entities].push_back(&m_components->lightStrip);
	m_componentsForTool[(int)EditorTool::Entities].push_back(&m_components->aaQuadDrag);
	m_componentsForTool[(int)EditorTool::Entities].push_back(&m_components->spawnEntity);
	m_componentsForTool[(int)EditorTool::Entities].push_back(&m_components->entity);
	
	m_componentsForTool[(int)EditorTool::Walls].push_back(&m_components->wallDrag);
	
	m_componentsForTool[(int)EditorTool::Corners].push_back(&m_components->gravityCorner);
	
	m_world->entManager.isEditor = true;
	
	m_editorState.world = m_world.get();
	m_editorState.camera = &m_camera;
	m_editorState.selectedEntities = &m_selectedEntities;
	m_editorState.projection.SetFieldOfViewDeg(75.0f);
	m_editorState.primitiveRenderer = &m_primRenderer;
	m_editorState.selectionRenderer = &m_selectionRenderer;
	
	m_thumbnailCamera.SetView(m_world->thumbnailCameraPos, m_world->thumbnailCameraPos + m_world->thumbnailCameraDir);
	
	ResetCamera();
	
	uid = nextUID++;
}

void EditorWorld::SetWindowRect(const eg::Rectangle& windowRect)
{
	m_editorState.windowRect = windowRect;
	m_editorState.projection.SetResolution(windowRect.w, windowRect.h);
	
	uint32_t resX = windowRect.w;
	uint32_t resY = windowRect.h;
	if (renderTexture.handle == nullptr || resX != renderTexture.Width() || resY != renderTexture.Height())
	{
		eg::SamplerDescription samplerDescription;
		samplerDescription.wrapU = eg::WrapMode::ClampToEdge;
		samplerDescription.wrapV = eg::WrapMode::ClampToEdge;
		samplerDescription.wrapW = eg::WrapMode::ClampToEdge;
		
		eg::TextureCreateInfo textureCI;
		textureCI.width = resX;
		textureCI.height = resY;
		textureCI.mipLevels = 1;
		
		textureCI.defaultSamplerDescription = &samplerDescription;
		textureCI.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
		textureCI.format = eg::Format::R8G8B8A8_sRGB;
		renderTexture = eg::Texture::Create2D(textureCI);
		
		textureCI.defaultSamplerDescription = nullptr;
		textureCI.flags = eg::TextureFlags::FramebufferAttachment;
		textureCI.format = eg::Format::Depth32;
		m_renderTextureDepth = eg::Texture::Create2D(textureCI);
		
		eg::FramebufferAttachment colorAttachment(renderTexture.handle);
		eg::FramebufferAttachment depthAttachment(m_renderTextureDepth.handle);
		
		m_framebuffer = eg::Framebuffer({ &colorAttachment, 1 }, depthAttachment);
	}
	
	m_selectionRenderer.BeginFrame(resX, resY);
}

void EditorWorld::RenderImGui()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save", "Ctrl + S"))
			{
				Save();
			}
			
			if (ImGui::MenuItem("Close", "Ctrl + W"))
			{
				shouldClose = true;
			}
			
			if (ImGui::MenuItem("Save and Close"))
			{
				Save();
				shouldClose = true;
			}
			
			ImGui::EndMenu();
		}
		
		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Update Thumbnail View"))
			{
				m_isUpdatingThumbnailView = true;
			}
			
			if (ImGui::MenuItem("Reset Camera"))
			{
				ResetCamera();
			}
			
			ImGui::MenuItem("Draw Voxel Grid", "Ctrl + G", &m_drawVoxelGrid);
			
			ImGui::EndMenu();
		}
		
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !m_levelHasEntrance);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_levelHasEntrance ? 1.0f : 0.4f);
		if (ImGui::Button("Test Level (F5)"))
		{
			TestLevel();
		}
		ImGui::PopStyleVar();
		ImGui::PopItemFlag();
		
		ImGui::EndMenuBar();
	}
}

void EditorWorld::RenderLevelSettings()
{
	ImGui::Checkbox("Has Gravity Gun", &m_world->playerHasGravityGun);
	
	int extraWaterParticles = m_world->extraWaterParticles;
	if (ImGui::DragInt("Extra Water Particles", &extraWaterParticles))
		m_world->extraWaterParticles = std::max(extraWaterParticles, 0);
	
	int presimIterations = m_world->waterPresimIterations;
	if (ImGui::DragInt("Water Presim Iterations", &presimIterations))
		m_world->waterPresimIterations = std::max(presimIterations, 1);
	
	ImGui::InputText("Title", &m_world->title);
	
	if (ImGui::CollapsingHeader("Control Hints", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (int i = 0; i < NUM_OPTIONAL_CONTROL_HINTS; i++)
		{
			std::string label = std::string(magic_enum::enum_name((OptionalControlHintType)i));
			ImGui::Checkbox(label.c_str(), &m_world->showControlHint[i]);
		}
	}
}

std::weak_ptr<World> runningEditorWorld;

void EditorWorld::TestLevel()
{
	if (!m_levelHasEntrance)
		return;
	runningEditorWorld = m_world;
	std::stringstream stream;
	m_world->Save(stream);
	stream.seekg(0, std::ios::beg);
	mainGameState->SetWorld(World::Load(stream, false), -1, nullptr, true);
	SetCurrentGS(mainGameState);
}

void EditorWorld::Update(float dt, EditorTool currentTool)
{
	if (m_isUpdatingThumbnailView)
	{
		if (eg::IsButtonDown(eg::Button::Escape) || eg::IsButtonDown(eg::Button::Space))
		{
			if (eg::IsButtonDown(eg::Button::Space))
			{
				m_world->thumbnailCameraPos = m_thumbnailCamera.Position();
				m_world->thumbnailCameraDir = m_thumbnailCamera.Forward();
			}
			m_isUpdatingThumbnailView = false;
		}
		
		if (!eg::console::IsShown())
		{
			eg::SetRelativeMouseMode(true);
			m_thumbnailCamera.Update(dt);
		}
		
		RenderSettings::instance->viewProjection =
			m_editorState.projection.Matrix() * m_thumbnailCamera.ViewMatrix();
		RenderSettings::instance->invViewProjection =
			m_thumbnailCamera.InverseViewMatrix() * m_editorState.projection.InverseMatrix();
		RenderSettings::instance->cameraPosition = m_thumbnailCamera.Position();
		RenderSettings::instance->UpdateBuffer();
		
		m_icons.clear();
		return;
	}
	
	if (isWindowFocused)
	{
		if (eg::InputState::Current().IsCtrlDown())
		{
			if (eg::IsButtonDown(eg::Button::S) && !eg::WasButtonDown(eg::Button::S))
				Save();
			if (eg::IsButtonDown(eg::Button::W) && !eg::WasButtonDown(eg::Button::W))
				shouldClose = true;
			if (eg::IsButtonDown(eg::Button::G) && !eg::WasButtonDown(eg::Button::G))
				m_drawVoxelGrid = !m_drawVoxelGrid;
		}
		if (eg::IsButtonDown(eg::Button::F5) && !eg::WasButtonDown(eg::Button::F5))
			TestLevel();
	}
	
	m_levelHasEntrance = false;
	m_world->entManager.ForEachOfType<EntranceExitEnt>([&] (const EntranceExitEnt& ent)
	{
		if (ent.m_type == EntranceExitEnt::Type::Entrance)
			m_levelHasEntrance = true;
	});
	
	WorldUpdateArgs entityUpdateArgs = { };
	entityUpdateArgs.mode = WorldMode::Editor;
	entityUpdateArgs.dt = dt;
	entityUpdateArgs.world = m_world.get();
	m_world->Update(entityUpdateArgs);
	
	bool canUpdateInput = isWindowFocused && isWindowHovered && !eg::console::IsShown();
	
	if (canUpdateInput)
	{
		m_camera.Update(dt, canUpdateInput);
	}
	
	glm::mat4 viewMatrix, inverseViewMatrix;
	m_camera.GetViewMatrix(viewMatrix, inverseViewMatrix);
	RenderSettings::instance->viewProjection = m_editorState.projection.Matrix() * viewMatrix;
	RenderSettings::instance->invViewProjection = inverseViewMatrix * m_editorState.projection.InverseMatrix();
	RenderSettings::instance->cameraPosition = glm::vec3(inverseViewMatrix[3]);
	RenderSettings::instance->UpdateBuffer();
	
	m_selectionRenderer.viewProjection = RenderSettings::instance->viewProjection;
	
	glm::vec2 windowCursorPosNF = glm::vec2(eg::CursorPos()) - m_editorState.windowRect.Min();
	glm::vec2 windowCursorPosNDC = ((windowCursorPosNF / m_editorState.windowRect.Size()) * 2.0f - 1.0f) * glm::vec2(1, -1);
	
	m_editorState.windowCursorPos = glm::vec2(windowCursorPosNF.x, m_editorState.windowRect.h - windowCursorPosNF.y);
	m_editorState.inverseViewProjection = RenderSettings::instance->invViewProjection;
	m_editorState.viewProjection = RenderSettings::instance->viewProjection;
	m_editorState.cameraPosition = glm::vec3(inverseViewMatrix[3]);
	m_editorState.viewRay = eg::Ray::UnprojectNDC(m_editorState.inverseViewProjection, windowCursorPosNDC);
	m_editorState.tool = currentTool;
	
	for (EditorComponent* comp : m_componentsForTool[(int)currentTool])
	{
		comp->Update(dt, m_editorState);
	}
	
	m_icons.clear();
	for (EditorComponent* comp : m_componentsForTool[(int)currentTool])
	{
		if (comp->CollectIcons(m_editorState, m_icons))
			break;
	}
	m_icons.push_back(m_editorState.CreateIcon(m_world->thumbnailCameraPos, nullptr));
	m_icons.back().iconIndex = 11;
	
	m_icons.erase(std::remove_if(m_icons.begin(), m_icons.end(), [&] (const EditorIcon& icon)
	{
		return icon.m_behindScreen;
	}), m_icons.end());
	std::sort(m_icons.begin(), m_icons.end(), [&] (const EditorIcon& a, const EditorIcon& b)
	{
		return a.m_depth > b.m_depth;
	});
	
	m_hoveredIcon = -1;
	m_editorState.anyIconHovered = false;
	if (canUpdateInput)
	{
		for (size_t i = 0; i < m_icons.size(); i++)
		{
			if (m_icons[i].m_rectangle.Contains(m_editorState.windowCursorPos))
			{
				m_hoveredIcon = i;
				m_editorState.anyIconHovered = true;
			}
		}
	}
	
	if (canUpdateInput)
	{
		for (EditorComponent* comp : m_componentsForTool[(int)currentTool])
		{
			if (comp->UpdateInput(dt, m_editorState))
			{
				canUpdateInput = false;
				break;
			}
		}
	}
	
	if (canUpdateInput && eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
	{
		bool shouldClearSelected = !eg::InputState::Current().IsCtrlDown();
		
		if (m_hoveredIcon != -1)
		{
			if (m_icons[m_hoveredIcon].shouldClearSelection && shouldClearSelected)
				m_selectedEntities.clear();
			if (m_icons[m_hoveredIcon].m_callback)
				m_icons[m_hoveredIcon].m_callback();
		}
		else if (shouldClearSelected)
			m_selectedEntities.clear();
	}
	
	//Invalidates water if the sum of all WaterBlockComp::editorVersion has changed
	int sumOfWaterBlockedVersion = 0;
	m_world->entManager.ForEachWithComponent<WaterBlockComp>([&] (const Ent& entity)
	{
		sumOfWaterBlockedVersion += entity.GetComponent<WaterBlockComp>()->editorVersion;
	});
	if (m_previousSumOfWaterBlockedVersion != -1 && m_previousSumOfWaterBlockedVersion != sumOfWaterBlockedVersion)
	{
		m_editorState.InvalidateWater();
	}
	m_previousSumOfWaterBlockedVersion = sumOfWaterBlockedVersion;
	
	m_savedTextTimer = std::max(m_savedTextTimer - dt, 0.0f);
}

void EditorWorld::Draw(EditorTool currentTool, RenderContext& renderCtx, PrepareDrawArgs prepareDrawArgs)
{
	m_primRenderer.Begin(RenderSettings::instance->viewProjection, RenderSettings::instance->cameraPosition);
	m_spriteBatch.Begin();
	renderCtx.meshBatch.Begin();
	renderCtx.transparentMeshBatch.Begin();
	
	eg::Frustum frustum(RenderSettings::instance->invViewProjection);
	prepareDrawArgs.frustum = &frustum;
	
	m_world->PrepareForDraw(prepareDrawArgs);
	
	for (EditorComponent* comp : m_componentsForTool[(int)currentTool])
	{
		comp->EarlyDraw(m_editorState);
	}
	
	//Draws icons
	const eg::Texture& iconsTexture = eg::GetAsset<eg::Texture>("Textures/UI/EntityIcons.png");
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
		                   CreateSrcRectangle(m_icons[i].selected ? 1 : ((int)i == m_hoveredIcon ? 10 : 0)),
		                   eg::SpriteFlags::None);
		
		//Draws the icon
		m_spriteBatch.Draw(iconsTexture, m_icons[i].m_rectangle, color,
		                   CreateSrcRectangle(m_icons[i].iconIndex), eg::SpriteFlags::None);
	}
	
	//Sends the editor draw message to entities
	EntEditorDrawArgs drawArgs = {};
	drawArgs.spriteBatch = &m_spriteBatch;
	drawArgs.primitiveRenderer = &m_primRenderer;
	drawArgs.meshBatch = &renderCtx.meshBatch;
	drawArgs.transparentMeshBatch = &renderCtx.transparentMeshBatch;
	drawArgs.world = m_world.get();
	drawArgs.frustum = &frustum;
	drawArgs.getDrawMode = [this] (const Ent* entity) -> EntEditorDrawMode
	{
		return IsEntitySelected(entity) ? EntEditorDrawMode::Selected : EntEditorDrawMode::Default;
	};
	m_world->entManager.ForEachWithFlag(EntTypeFlags::EditorDrawable, [&] (Ent& entity)
	{
		m_components->entity.DrawEntityBox(m_primRenderer, entity,
			currentTool == EditorTool::Entities && IsEntitySelected(&entity));
		entity.EditorDraw(drawArgs);
	});
	
	m_primRenderer.End();
	
	renderCtx.meshBatch.End(eg::DC);
	renderCtx.transparentMeshBatch.End(eg::DC);
	
	m_liquidPlaneRenderer.Prepare(*m_world, renderCtx.transparentMeshBatch, RenderSettings::instance->cameraPosition);
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_framebuffer.handle;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(0, 0, 0, 0);
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	m_world->DrawEditor(m_drawVoxelGrid);
	
	MeshDrawArgs mDrawArgs;
	mDrawArgs.waterDepthTexture = WaterRenderer::GetDummyDepthTexture();
	mDrawArgs.drawMode = MeshDrawMode::Editor;
	mDrawArgs.rtManager = nullptr;
	renderCtx.meshBatch.Draw(eg::DC, &mDrawArgs);
	
	m_primRenderer.Draw();
	
	renderCtx.transparentMeshBatch.Draw(eg::DC, &mDrawArgs);
	
	m_liquidPlaneRenderer.Render();
	
	m_selectionRenderer.EndFrame();
	
	for (EditorComponent* comp : m_componentsForTool[(int)currentTool])
	{
		comp->LateDraw(m_editorState);
	}
	
	eg::DC.EndRenderPass();
	
	if (m_savedTextTimer > 0)
	{
		constexpr float SAVED_TEXT_FADE_TIME = 0.5f;
		const float a = std::min(m_savedTextTimer / SAVED_TEXT_FADE_TIME, 1.0f);
		m_spriteBatch.DrawText(*style::UIFontSmall, "Saved", glm::vec2(10), eg::ColorLin(1, 1, 1, a),
		                       1, nullptr, eg::TextFlags::DropShadow);
	}
	
	if (m_isUpdatingThumbnailView)
	{
		const std::string_view label = "Press space to set view\nPress escape to cancel";
		const eg::Rectangle labelRect(10, m_editorState.windowRect.h - 60, 210, 50);
		const float labelY = labelRect.MaxY() - 5 - eg::SpriteFont::DevFont().LineHeight();
		
		m_spriteBatch.DrawRect(labelRect, eg::ColorLin(0, 0, 0, 0.8f));
		m_spriteBatch.DrawTextMultiline(
			eg::SpriteFont::DevFont(), label, glm::vec2(labelRect.x + 5, labelY),
			eg::ColorLin(eg::ColorLin::White));
	}
	
	eg::RenderPassBeginInfo spriteRPBeginInfo;
	spriteRPBeginInfo.framebuffer = m_framebuffer.handle;
	spriteRPBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	m_spriteBatch.End(m_editorState.windowRect.w, m_editorState.windowRect.h, spriteRPBeginInfo);
	
	renderTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

bool EditorWorld::IsEntitySelected(const Ent* entity) const
{
	for (const std::weak_ptr<Ent>& selEnt : m_selectedEntities)
	{
		if (selEnt.lock().get() == entity)
			return true;
	}
	return false;
}

void EditorWorld::Save()
{
	std::string savePath = GetLevelPath(m_levelName);
	std::ofstream stream(savePath, std::ios::binary);
	m_world->Save(stream);
	m_savedTextTimer = 3;
	InitLevels();
}

void EditorWorld::RenderToolSettings(EditorTool currentTool)
{
	for (EditorComponent* comp : m_componentsForTool[(int)currentTool])
	{
		comp->RenderSettings(m_editorState);
	}
}

void EditorWorld::ResetCamera()
{
	auto [voxelMin, voxelMax] = m_world->voxels.CalculateBounds();
	m_camera.Reset(eg::Sphere(
		glm::vec3(voxelMin + voxelMax) / 2.0f,
		glm::distance(glm::vec3(voxelMin), glm::vec3(voxelMax)) / 2.0f
	));
}

#endif
