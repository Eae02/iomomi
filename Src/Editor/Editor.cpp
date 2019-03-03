#include "Editor.hpp"
#include "../Levels.hpp"
#include "../MainGameState.hpp"

#include <filesystem>
#include <fstream>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

Editor* editor;

Editor::Editor(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx)
{
	m_prepareDrawArgs.isEditor = true;
	m_prepareDrawArgs.objectRenderer = &renderCtx.objectRenderer;
	
	m_projection.SetFieldOfViewDeg(75.0f);
}

const char* TextureNames[] =
{
	"No Draw",
	"Tactile Gray",
	"Metal 1"
};

void Editor::RunFrame(float dt)
{
	if (m_world == nullptr)
	{
		ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_Always);
		if (ImGui::Begin("Select Level", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
		{
			ImGui::Text("New Level");
			ImGui::InputText("Name", &m_newLevelName);
			if (ImGui::Button("Create") && !m_newLevelName.empty())
			{
				m_levelName = std::move(m_newLevelName);
				m_newLevelName = { };
				m_world = std::make_unique<World>();
				
				for (int x = 0; x < 3; x++)
				{
					for (int y = 0; y < 3; y++)
					{
						for (int z = 0; z < 3; z++)
						{
							m_world->SetIsAir({x, y, z}, true);
						}
					}
				}
			}
			
			ImGui::Separator();
			ImGui::Text("Open Level");
			
			for (Level* level = firstLevel; level != nullptr; level = level->next)
			{
				ImGui::PushID(level);
				std::string label = eg::Concat({ level->name, "###L" });
				if (ImGui::MenuItem(label.c_str()))
				{
					m_world = std::make_unique<World>();
					
					std::string path = eg::Concat({ eg::ExeDirPath(), "/levels/", level->name, ".gwd" });
					std::ifstream stream(path, std::ios::binary);
					m_world->Load(stream);
					
					m_levelName = level->name;
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
	
	ImGui::SetNextWindowPos(ImVec2(0, eg::CurrentResolutionY() * 0.5f), ImGuiCond_Always, ImVec2(0.0f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(200, eg::CurrentResolutionY() * 0.5f), ImGuiCond_Always);
	ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
	             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	if (ImGui::Button("Test Level (F5)", ImVec2(ImGui::GetContentRegionAvailWidth(), 0)))
	{
		std::stringstream stream;
		m_world->Save(stream);
		stream.seekg(0, std::ios::beg);
		mainGameState->LoadWorld(stream);
		currentGS = mainGameState;
		ImGui::End();
		return;
	}
	ImGui::Spacing();
	if (ImGui::CollapsingHeader("Tools", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::RadioButton("Walls", reinterpret_cast<int*>(&m_tool), (int)Tool::Walls);
		ImGui::RadioButton("Corners", reinterpret_cast<int*>(&m_tool), (int)Tool::Corners);
		ImGui::RadioButton("Entities", reinterpret_cast<int*>(&m_tool), (int)Tool::Entities);
	}
	if (m_tool == Tool::Walls)
	{
		if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::BeginChildFrame(ImGui::GetID("TexturesList"), ImVec2(0, 300));
			
			for (size_t i = 0; i < eg::ArrayLen(TextureNames); i++)
			{
				const float ICON_HEIGHT = 64;
				if (ImGui::Selectable(TextureNames[i], false) &&  m_selState == SelState::SelectDone)
				{
					glm::ivec3 mn = glm::min(m_selection1, m_selection2);
					glm::ivec3 mx = glm::max(m_selection1, m_selection2);
					
					for (int x = mn.x; x <= mx.x; x++)
					{
						for (int y = mn.y; y <= mx.y; y++)
						{
							for (int z = mn.z; z <= mx.z; z++)
							{
								glm::ivec3 pos = glm::ivec3(x, y, z) + DirectionVector(m_selectionNormal);
								if (m_world->IsAir(pos))
								{
									m_world->SetTexture(pos, m_selectionNormal, i);
								}
							}
						}
					}
				}
			}
			
			ImGui::EndChildFrame();
		}
	}
	ImGui::End();
	
	glm::mat4 viewMatrix, inverseViewMatrix;
	m_camera.GetViewMatrix(viewMatrix, inverseViewMatrix);
	
	RenderSettings::instance->viewProjection = m_projection.Matrix() * viewMatrix;
	RenderSettings::instance->invViewProjection = inverseViewMatrix * m_projection.InverseMatrix();
	RenderSettings::instance->cameraPosition = glm::vec3(inverseViewMatrix[3]);
	RenderSettings::instance->UpdateBuffer();
	
	if (!eg::console::IsShown())
	{
		m_camera.Update(dt);
		
		if (m_tool == Tool::Walls)
			UpdateToolWalls(dt);
		else if (m_tool == Tool::Corners)
			UpdateToolCorners(dt);
		else if (m_tool == Tool::Entities)
			UpdateToolEntities(dt);
	}
	
	m_selection2Anim += std::min(dt * 20, 1.0f) * (glm::vec3(m_selection2) - m_selection2Anim);
	
	DrawWorld();
}

void Editor::UpdateToolCorners(float dt)
{
	if (ImGui::GetIO().WantCaptureMouse)
		return;
	
	eg::Ray viewRay = eg::Ray::UnprojectScreen(RenderSettings::instance->invViewProjection, eg::CursorPos());
	PickWallResult pickResult = m_world->PickWall(viewRay);
	
	m_hoveredCornerDim = -1;
	
	if (pickResult.intersected)
	{
		const int nDim = (int)pickResult.normalDir / 2;
		const int uDim = (nDim + 1) % 3;
		const int vDim = (nDim + 2) % 3;
		float u = pickResult.intersectPosition[uDim];
		float v = pickResult.intersectPosition[vDim];
		
		int ni = (int)std::round(pickResult.intersectPosition[nDim]);
		int ui = (int)std::round(u);
		int vi = (int)std::round(v);
		float dui = std::abs(ui - u);
		float dvi = std::abs(vi - v);
		
		const float MAX_DIST = 0.4f;
		auto TryV = [&]
		{
			if (dvi > MAX_DIST)
				return;
			m_hoveredCornerPos[nDim] = ni;
			m_hoveredCornerPos[uDim] = std::floor(u);
			m_hoveredCornerPos[vDim] = vi;
			if (m_world->IsCorner(m_hoveredCornerPos, (Dir)(uDim * 2)))
				m_hoveredCornerDim = uDim;
		};
		auto TryU = [&]
		{
			if (dui > MAX_DIST)
				return;
			m_hoveredCornerPos[nDim] = ni;
			m_hoveredCornerPos[uDim] = ui;
			m_hoveredCornerPos[vDim] = std::floor(v);
			if (m_world->IsCorner(m_hoveredCornerPos, (Dir)(vDim * 2)))
				m_hoveredCornerDim = vDim;
		};
		
		if (dui < dvi)
		{
			TryU();
			if (m_hoveredCornerDim == -1)
				TryV();
		}
		else
		{
			TryV();
			if (m_hoveredCornerDim == -1)
				TryU();
		}
	}
	
	if (eg::IsButtonDown(eg::Button::MouseLeft))
	{
		if (m_modCornerDim != m_hoveredCornerDim || m_modCornerPos != m_hoveredCornerPos)
		{
			const Dir cornerDir = (Dir)(m_hoveredCornerDim * 2);
			const bool isGravityCorner = m_world->IsGravityCorner(m_hoveredCornerPos, cornerDir);
			m_world->SetIsGravityCorner(m_hoveredCornerPos, cornerDir, !isGravityCorner);
			
			m_modCornerDim = m_hoveredCornerDim;
			m_modCornerPos = m_hoveredCornerPos;
		}
	}
	else
	{
		m_modCornerDim = -1;
	}
}

void Editor::UpdateToolEntities(float dt)
{
	const float ICON_SIZE = 32;
	
	auto AllowMouseInteract = [&] { return !ImGui::GetIO().WantCaptureMouse && !m_translationGizmo.HasInputFocus(); };
	
	auto viewRay = eg::MakeLazy([&]
	{
		return eg::Ray::UnprojectScreen(RenderSettings::instance->invViewProjection, eg::CursorPos());
	});
	
	//Updates the translation gizmo
	if (!m_selectedEntities.empty() && !ImGui::GetIO().WantCaptureMouse)
	{
		glm::vec3 prevPos;
		for (const std::shared_ptr<Entity>& selectedEntity : m_selectedEntities)
		{
			prevPos += selectedEntity->Position();
		}
		prevPos /= (float)m_selectedEntities.size();
		
		glm::vec3 newPos = prevPos;
		m_translationGizmo.Update(newPos, RenderSettings::instance->cameraPosition,
		                          RenderSettings::instance->viewProjection, *viewRay);
		
		for (const std::shared_ptr<Entity>& selectedEntity : m_selectedEntities)
		{
			selectedEntity->SetPosition(selectedEntity->Position() + newPos - prevPos);
		}
	}
	
	//Updates entity icons
	m_entityIcons.clear();
	for (const std::shared_ptr<Entity>& entity : m_world->Entities())
	{
		glm::vec4 sp4 = RenderSettings::instance->viewProjection * glm::vec4(entity->Position(), 1.0f);
		glm::vec3 sp3 = glm::vec3(sp4) / sp4.w;
		glm::vec2 screenPos = (glm::vec2(sp3) * 0.5f + 0.5f) * glm::vec2(eg::CurrentResolutionX(), eg::CurrentResolutionY());
		
		m_entityIcons.push_back({ eg::Rectangle::CreateCentered(screenPos, ICON_SIZE, ICON_SIZE), sp3.z, entity });
	}
	
	std::sort(m_entityIcons.begin(), m_entityIcons.end(), [&] (const EntityIcon& a, const EntityIcon& b)
	{
		return a.depth > b.depth;
	});
	
	glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	
	//Updates the selected entity
	if (eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft) && AllowMouseInteract())
	{
		if (!eg::IsButtonDown(eg::Button::LeftControl) && !eg::IsButtonDown(eg::Button::RightControl))
		{
			m_selectedEntities.clear();
		}
		
		for (int64_t i = (int64_t)m_entityIcons.size() - 1; i >= 0; i--)
		{
			if (m_entityIcons[i].rectangle.Contains(flippedCursorPos))
			{
				m_selectedEntities.push_back(m_entityIcons[i].entity);
				break;
			}
		}
	}
	
	//Opens settings windows
	if (ImGui::IsMouseDoubleClicked(0) && AllowMouseInteract())
	{
		for (int64_t i = (int64_t)m_entityIcons.size() - 1; i >= 0; i--)
		{
			if (m_entityIcons[i].rectangle.Contains(flippedCursorPos))
			{
				std::weak_ptr<Entity> entityWeak = m_entityIcons[i].entity;
				if (!std::any_of(m_settingsWindowEntities.begin(), m_settingsWindowEntities.end(),
					[&] (const std::weak_ptr<Entity>& e) { return e.lock() == m_entityIcons[i].entity; }))
				{
					m_settingsWindowEntities.emplace_back(m_entityIcons[i].entity);
				}
				break;
			}
		}
	}
	
	//Updates settings windows
	for (int64_t i = (int64_t)m_settingsWindowEntities.size() - 1; i >= 0; i--)
	{
		bool open = true;
		if (std::shared_ptr<Entity> entity = m_settingsWindowEntities[i].lock())
		{
			std::ostringstream labelStream;
			labelStream << entity->TypeName() << " - Settings##" << entity.get();
			std::string labelString = labelStream.str();
			ImGui::SetNextWindowSize(ImVec2(200, 0), ImGuiCond_Appearing);
			if (ImGui::Begin(labelString.c_str(), &open, ImGuiWindowFlags_NoSavedSettings))
			{
				entity->EditorRenderSettings();
			}
			ImGui::End();
		}
		else
		{
			open = false;
		}
		if (!open)
		{
			m_settingsWindowEntities[i].swap(m_settingsWindowEntities.back());
			m_settingsWindowEntities.pop_back();
		}
	}
	
	//Opens the spawn entity menu if the right mouse button is clicked
	if (eg::IsButtonDown(eg::Button::MouseRight) && !eg::WasButtonDown(eg::Button::MouseRight) && AllowMouseInteract())
	{
		ImGui::OpenPopup("SpawnEntity");
	}
	
	if (ImGui::BeginPopup("SpawnEntity"))
	{
		ImGui::Text("Spawn Entity");
		ImGui::Separator();
		
		ImGui::BeginChild("EntityTypes", ImVec2(180, 250));
		for (const EntityType& entityType : entityTypes)
		{
			if (ImGui::MenuItem(entityType.DisplayName().c_str()))
			{
				PickWallResult pickResult = m_world->PickWall(*viewRay);
				
				if (pickResult.intersected)
				{
					std::shared_ptr<Entity> entity = entityType.CreateInstance();
					entity->EditorSpawned(pickResult.intersectPosition, pickResult.normalDir);
					m_world->AddEntity(std::move(entity));
				}
				
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndChild();
		
		ImGui::EndPopup();
	}
}

void Editor::UpdateToolWalls(float dt)
{
	if (ImGui::GetIO().WantCaptureMouse)
		return;
	
	auto viewRay = eg::MakeLazy([&]
	{
		return eg::Ray::UnprojectScreen(RenderSettings::instance->invViewProjection, eg::CursorPos());
	});
	
	//Handles mouse move events
	if (eg::IsButtonDown(eg::Button::MouseLeft) && eg::WasButtonDown(eg::Button::MouseLeft) &&
	    eg::CursorPos() != eg::PrevCursorPos())
	{
		PickWallResult pickResult = m_world->PickWall(*viewRay);
		
		int selDim = (int)m_selectionNormal / 2;
		if (m_selState == SelState::Selecting)
		{
			//Updates the current selection if the new hovered voxel is
			// in the same plane as the voxel where the selection started.
			if (pickResult.intersected && pickResult.voxelPosition[selDim] == m_selection1[selDim])
			{
				m_selection2 = pickResult.voxelPosition;
			}
		}
		else if (m_selState == SelState::Dragging)
		{
			eg::Ray dragLineRay(m_dragStartPos, glm::abs(DirectionVector(m_selectionNormal)));
			float closestPoint = dragLineRay.GetClosestPoint(*viewRay);
			if (!std::isnan(closestPoint))
			{
				int newDragDist = (int)glm::round(closestPoint);
				if (newDragDist != m_dragDistance)
				{
					int newDragDir = newDragDist > m_dragDistance ? 1 : -1;
					if (newDragDir != m_dragDir)
						m_dragAirMode = 0;
					
					int dragDelta = newDragDist - m_dragDistance;
					glm::ivec3 mn = glm::min(m_selection1, m_selection2);
					glm::ivec3 mx = glm::max(m_selection1, m_selection2);
					if (dragDelta > 0)
					{
						mx[selDim] += dragDelta - 1;
					}
					else
					{
						mn[selDim] += dragDelta;
						mx[selDim]--;
					}
					
					if ((int)m_selectionNormal % 2 == 0)
					{
						mn[selDim]++;
						mx[selDim]++;
					}
					
					bool allAir = true;
					for (int x = mn.x; x <= mx.x; x++)
					{
						for (int y = mn.y; y <= mx.y; y++)
						{
							for (int z = mn.z; z <= mx.z; z++)
							{
								if (!m_world->IsAir({x, y, z}))
									allAir = false;
							}
						}
					}
					
					int airMode = allAir ? 1 : 2;
					if (m_dragAirMode == 0)
						m_dragAirMode = airMode;
					
					if (m_dragAirMode == airMode)
					{
						for (int x = mn.x; x <= mx.x; x++)
						{
							for (int y = mn.y; y <= mx.y; y++)
							{
								for (int z = mn.z; z <= mx.z; z++)
								{
									m_world->SetIsAir({x, y, z}, !allAir);
								}
							}
						}
						
						m_selection1[selDim] += newDragDist - m_dragDistance;
						m_selection2[selDim] += newDragDist - m_dragDistance;
						m_dragDir = newDragDist > m_dragDistance ? 1 : -1;
						m_dragDistance = newDragDist;
					}
				}
			}
		}
		else if (m_selState == SelState::SelectDone && pickResult.intersected &&
		    pickResult.voxelPosition.x >= std::min(m_selection1.x, m_selection2.x) && 
		    pickResult.voxelPosition.x <= std::max(m_selection1.x, m_selection2.x) && 
		    pickResult.voxelPosition.y >= std::min(m_selection1.y, m_selection2.y) && 
		    pickResult.voxelPosition.y <= std::max(m_selection1.y, m_selection2.y) && 
		    pickResult.voxelPosition.z >= std::min(m_selection1.z, m_selection2.z) && 
		    pickResult.voxelPosition.z <= std::max(m_selection1.z, m_selection2.z))
		{
			//Starts a drag operation
			m_selState = SelState::Dragging;
			m_dragStartPos = pickResult.intersectPosition;
			m_dragDistance = 0;
			m_dragAirMode = 0;
			m_dragDir = 0;
		}
		else if (pickResult.intersected)
		{
			//Starts a new selection
			m_selState = SelState::Selecting;
			m_selection1 = pickResult.voxelPosition;
			m_selection2Anim = m_selection1;
			m_selectionNormal = pickResult.normalDir;
		}
	}
	
	//Handles mouse up events. These should complete the selection if the user was selecting,
	// go back to this mode if the user was dragging, or clear the selection otherwise.
	if (eg::WasButtonDown(eg::Button::MouseLeft) && !eg::IsButtonDown(eg::Button::MouseLeft))
	{
		if (m_selState == SelState::Selecting || m_selState == SelState::Dragging)
			m_selState = SelState::SelectDone;
		else
			m_selState = SelState::NoSelection;
	}
}

void Editor::DrawWorld()
{
	m_primRenderer.Begin(RenderSettings::instance->viewProjection);
	m_spriteBatch.Begin();
	m_renderCtx->objectRenderer.Begin(ObjectMaterial::PipelineType::Editor);
	
	m_world->PrepareForDraw(m_prepareDrawArgs);
	
	if (m_tool == Tool::Walls)
		DrawToolWalls();
	else if (m_tool == Tool::Corners)
		DrawToolCorners();
	else if (m_tool == Tool::Entities)
		DrawToolEntities();
	
	//Calls EditorDraw on entities
	Entity::EditorDrawArgs entityDrawArgs;
	entityDrawArgs.spriteBatch = &m_spriteBatch;
	entityDrawArgs.primitiveRenderer = &m_primRenderer;
	entityDrawArgs.objectRenderer = &m_renderCtx->objectRenderer;
	for (const std::shared_ptr<Entity>& entity : m_world->Entities())
	{
		entity->EditorDraw(m_tool == Tool::Entities && eg::Contains(m_selectedEntities, entity), entityDrawArgs);
	}
	
	m_primRenderer.End();
	
	m_renderCtx->objectRenderer.End();
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = nullptr;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(eg::ColorSRGB::FromHex(0x1C1E26));
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	m_world->DrawEditor();
	
	m_renderCtx->objectRenderer.Draw();
	
	m_primRenderer.Draw();
	
	if (m_tool == Tool::Entities && !m_selectedEntities.empty())
	{
		m_translationGizmo.Draw(RenderSettings::instance->viewProjection);
	}
	
	eg::DC.EndRenderPass();
	
	eg::RenderPassBeginInfo spriteRPBeginInfo;
	spriteRPBeginInfo.framebuffer = nullptr;
	spriteRPBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	m_spriteBatch.End(eg::CurrentResolutionX(), eg::CurrentResolutionY(), spriteRPBeginInfo);
}

void Editor::DrawToolWalls()
{
	if (m_selState != SelState::NoSelection)
	{
		int nd = (int)m_selectionNormal / 2;
		int sd1 = (nd + 1) % 3;
		int sd2 = (nd + 2) % 3;
		
		glm::vec3 quadCorners[4];
		for (glm::vec3& corner : quadCorners)
		{
			corner[nd] = m_selection1[nd] + (((int)m_selectionNormal % 2) ? 0 : 1);
		}
		quadCorners[1][sd1] = quadCorners[0][sd1] = std::min<float>(m_selection1[sd1], m_selection2Anim[sd1]);
		quadCorners[3][sd1] = quadCorners[2][sd1] = std::max<float>(m_selection1[sd1], m_selection2Anim[sd1]) + 1;
		quadCorners[2][sd2] = quadCorners[0][sd2] = std::min<float>(m_selection1[sd2], m_selection2Anim[sd2]);
		quadCorners[1][sd2] = quadCorners[3][sd2] = std::max<float>(m_selection1[sd2], m_selection2Anim[sd2]) + 1;
		
		eg::ColorSRGB color;
		if (m_selState == SelState::Selecting)
		{
			color = eg::ColorSRGB(eg::ColorSRGB::FromHex(0x91CAED).ScaleAlpha(0.5f));
		}
		else if (m_selState == SelState::Dragging)
		{
			color = eg::ColorSRGB(eg::ColorSRGB::FromHex(0xDB9951).ScaleAlpha(0.4f));
		}
		else
		{
			color = eg::ColorSRGB(eg::ColorSRGB::FromHex(0x91CAED).ScaleAlpha(0.4f));
		}
		
		m_primRenderer.AddQuad(quadCorners, color);
	}
}

void Editor::DrawToolCorners()
{
	if (m_hoveredCornerDim != -1)
	{
		const float LINE_WIDTH = 0.1f;
		
		glm::vec3 lineCorners[8];
		
		glm::vec3 uDir, vDir, sDir;
		uDir[m_hoveredCornerDim] = 1;
		vDir[(m_hoveredCornerDim + 1) % 3] = LINE_WIDTH;
		sDir[(m_hoveredCornerDim + 2) % 3] = LINE_WIDTH;
		
		for (int s = 0; s < 2; s++)
		{
			for (int u = 0; u < 2; u++)
			{
				for (int v = 0; v < 2; v++)
				{
					lineCorners[s * 4 + u * 2 + v] = 
						glm::vec3(m_hoveredCornerPos) +
						(s ? sDir : vDir) * (float)(v * 2 - 1) +
						uDir * (float)u;
				}
			}
		}
		
		eg::ColorSRGB color = eg::ColorSRGB(eg::ColorSRGB::FromHex(0xDB9951).ScaleAlpha(0.4f));
		m_primRenderer.AddQuad(lineCorners, color);
		m_primRenderer.AddQuad(lineCorners + 4, color);
	}
}

void Editor::DrawToolEntities()
{
	const eg::Texture& iconsTexture = eg::GetAsset<eg::Texture>("Textures/EntityIcons.png");
	
	auto CreateSrcRectangle = [&] (int index)
	{
		return eg::Rectangle(index * 50, 0, 50, 50);
	};
	
	for (const EntityIcon& icon : m_entityIcons)
	{
		bool selected = eg::Contains(m_selectedEntities, icon.entity);
		
		m_spriteBatch.Draw(iconsTexture, icon.rectangle, eg::ColorLin(eg::Color::White),
			CreateSrcRectangle(selected ? 1 : 0), eg::FlipFlags::Normal);
		
		m_spriteBatch.Draw(iconsTexture, icon.rectangle, eg::ColorLin(eg::Color::White),
			CreateSrcRectangle(icon.entity->GetEditorIconIndex()), eg::FlipFlags::Normal);
	}
}

void Editor::DrawMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save"))
			{
				std::string savePath = eg::Concat({ eg::ExeDirPath(), "/levels/", m_levelName, ".gwd" });
				std::ofstream stream(savePath, std::ios::binary);
				m_world->Save(stream);
			}
			
			if (ImGui::MenuItem("Close"))
			{
				m_levelName.clear();
				m_world = nullptr;
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
