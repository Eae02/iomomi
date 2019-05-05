#include "Editor.hpp"
#include "../Levels.hpp"
#include "../MainGameState.hpp"
#include "../Graphics/Materials/MeshDrawArgs.hpp"
#include "../World/Entities/ECEditorVisible.hpp"
#include "../World/Entities/ECWallMounted.hpp"
#include "../World/EntityTypes.hpp"
#include "../World/Entities/ECActivatable.hpp"
#include "../World/Entities/ECActivator.hpp"
#include "../World/Entities/ECActivationLightStrip.hpp"

#include <filesystem>
#include <fstream>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

static eg::EntitySignature editorVisibleSignature = eg::EntitySignature::Create<ECEditorVisible>();

Editor* editor;

Editor::Editor(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx)
{
	m_prepareDrawArgs.isEditor = true;
	m_prepareDrawArgs.meshBatch = &renderCtx.meshBatch;
	
	m_projection.SetFieldOfViewDeg(75.0f);
}

const char* TextureNames[] =
{
	"No Draw",
	"Tactile Gray",
	"Metal 1",
	"Metal Grid"
};

void Editor::InitWorld()
{
	m_selectedEntities.clear();
	InitializeActConnections();
	ECActivationLightStrip::GenerateAll(*m_world);
}

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
								m_world->SetTextureSafe({x, y, z}, (Dir)s, 1);
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
	
	WorldUpdateArgs entityUpdateArgs;
	entityUpdateArgs.dt = dt;
	entityUpdateArgs.player = nullptr;
	entityUpdateArgs.world = m_world.get();
	m_world->Update(entityUpdateArgs);
	
	ImGui::SetNextWindowPos(ImVec2(0, eg::CurrentResolutionY() * 0.5f), ImGuiCond_Always, ImVec2(0.0f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(200, eg::CurrentResolutionY() * 0.5f), ImGuiCond_Always);
	ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
	             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	if (ImGui::Button("Test Level (F5)", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
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
								m_world->SetTextureSafe(pos, m_selectionNormal, i);
							}
						}
					}
				}
			}
			
			ImGui::EndChildFrame();
		}
	}
	else if (m_tool == Tool::Entities)
	{
		for (const eg::EntityHandle& entityHandle : m_selectedEntities)
		{
			eg::Entity* entity = entityHandle.Get();
			if (entity == nullptr)
				continue;
			
			ECEditorVisible* edVisible = entity->FindComponent<ECEditorVisible>();
			if (edVisible == nullptr)
				continue;
			
			ImGui::PushID(entityHandle.Id());
			if (ImGui::CollapsingHeader(edVisible->displayName, ImGuiTreeNodeFlags_DefaultOpen))
			{
				EditorRenderImGuiMessage message;
				entity->HandleMessage(message);
				
				if (entity->FindComponent<ECActivator>())
				{
					if (ImGui::Button("Connect..."))
					{
						m_connectingActivator = *entity;
						RemoveActConnections(m_connectingActivator, -1);
					}
				}
			}
			ImGui::PopID();
		}
	}
	ImGui::End();
	
	m_world->EntityManager().EndFrame();
	
	glm::mat4 viewMatrix, inverseViewMatrix;
	m_camera.GetViewMatrix(viewMatrix, inverseViewMatrix);
	
	RenderSettings::instance->viewProjection = m_projection.Matrix() * viewMatrix;
	RenderSettings::instance->invViewProjection = inverseViewMatrix * m_projection.InverseMatrix();
	RenderSettings::instance->cameraPosition = glm::vec3(inverseViewMatrix[3]);
	RenderSettings::instance->gameTime += dt;
	RenderSettings::instance->UpdateBuffer();
	
	m_viewRay = eg::Ray::UnprojectScreen(RenderSettings::instance->invViewProjection, eg::CursorPos());
	
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
	
	WallRayIntersectResult pickResult = m_world->RayIntersectWall(m_viewRay);
	
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

void Editor::InitializeActConnections()
{
	m_actConnections.clear();
	for (const eg::Entity& activatorEntity : m_world->EntityManager().GetEntitySet(ECActivator::Signature))
	{
		uint32_t activatableName = activatorEntity.GetComponent<ECActivator>().activatableName;
		for (const eg::Entity& activatableEntity : m_world->EntityManager().GetEntitySet(ECActivatable::Signature))
		{
			if (activatableEntity.GetComponent<ECActivatable>().Name() == activatableName)
			{
				m_actConnections.push_back({ activatorEntity, activatableEntity });
				break;
			}
		}
	}
}

void Editor::RemoveActConnections(eg::EntityHandle entity, int targetConnectionIndex)
{
	for (int64_t i = (int64_t)m_actConnections.size() - 1; i >= 0; i--)
	{
		if (m_actConnections[i].activator == entity || (m_actConnections[i].activatable == entity))
		{
			ECActivatable* activatable = m_actConnections[i].activatable.FindComponent<ECActivatable>();
			ECActivator* activator = m_actConnections[i].activator.FindComponent<ECActivator>();
			if (activatable && activator)
			{
				if (m_actConnections[i].activatable == entity && targetConnectionIndex != -1 &&
				    activator->targetConnectionIndex != targetConnectionIndex)
				{
					continue;
				}
				
				if (auto* lightStrip = m_actConnections[i].activator.FindComponent<ECActivationLightStrip>())
					lightStrip->ClearGeneratedMesh();
				
				activatable->SetDisconnected(activator->targetConnectionIndex);
			}
			
			m_actConnections[i] = m_actConnections.back();
			m_actConnections.pop_back();
		}
	}
}

void Editor::UpdateToolEntities(float dt)
{
	const float ICON_SIZE = 32;
	
	auto AllowMouseInteract = [&]
	{
		return !ImGui::GetIO().WantCaptureMouse && !m_translationGizmo.HasInputFocus();
	};
	
	if (eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
	{
		m_mouseDownPos = eg::CursorPos();
	}
	
	auto SnapToGrid = [&] (const glm::vec3& pos) -> glm::vec3
	{
		if (!eg::IsButtonDown(eg::Button::LeftAlt))
		{
			const float STEP = 0.1f;
			return glm::round(pos / STEP) * STEP;
		}
		return pos;
	};
	
	auto MaybeClone = [&] ()
	{
		if (!eg::IsButtonDown(eg::Button::LeftShift) || m_entitiesCloned)
			return false;
		
		for (size_t i = 0; i < m_selectedEntities.size(); i++)
		{
			//m_selectedEntities[i] = m_selectedEntities[i]->Clone();
			//m_world->EntityManager().AddEntity(m_selectedEntities[i]);
		}
		m_entitiesCloned = true;
		return true;
	};
	
	if (!eg::IsButtonDown(eg::Button::LeftShift))
		m_entitiesCloned = false;
	
	ECWallMounted* wallMountedEntity = nullptr;
	auto UpdateWallDragEntity = [&]
	{
		if (eg::Entity* entity = m_selectedEntities[0].Get())
		{
			wallMountedEntity = entity->FindComponent<ECWallMounted>();
		}
	};
	if (m_selectedEntities.size() == 1)
		UpdateWallDragEntity();
	
	const bool mouseClicked = eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft);
	const bool mouseHeld = eg::IsButtonDown(eg::Button::MouseLeft) && eg::WasButtonDown(eg::Button::MouseLeft);
	const bool isConnectingActivator = m_connectingActivator.Get();
	
	auto EntityMoved = [&] (eg::Entity& entity)
	{
		for (const ActConnection& connection : m_actConnections)
		{
			eg::Entity* activatorEntity = connection.activator.Get();
			if (activatorEntity != nullptr && (&entity == activatorEntity || &entity == connection.activatable.Get()))
			{
				ECActivationLightStrip::GenerateForActivator(*m_world, *activatorEntity);
			}
		}
		
		EditorMovedMessage message;
		message.world = m_world.get();
		m_selectedEntities[0].Get()->HandleMessage(message);
	};
	
	//Moves the wall drag entity
	if (wallMountedEntity && !isConnectingActivator && mouseHeld)
	{
		constexpr int DRAG_BEGIN_DELTA = 10;
		if (m_isDraggingWallEntity)
		{
			WallRayIntersectResult pickResult = m_world->RayIntersectWall(m_viewRay);
			if (pickResult.intersected)
			{
				if (MaybeClone())
					UpdateWallDragEntity();
				
				wallMountedEntity->wallUp = pickResult.normalDir;
				
				if (eg::ECPosition3D* positionComp = m_selectedEntities[0].Get()->FindComponent<eg::ECPosition3D>())
				{
					positionComp->position = SnapToGrid(pickResult.intersectPosition);
					EntityMoved(*m_selectedEntities[0].Get());
				}
			}
		}
		else if (std::abs(m_mouseDownPos.x - eg::CursorX()) > DRAG_BEGIN_DELTA ||
		         std::abs(m_mouseDownPos.y - eg::CursorY()) > DRAG_BEGIN_DELTA)
		{
			m_isDraggingWallEntity = true;
		}
	}
	else
	{
		m_isDraggingWallEntity = false;
	}
	
	//Updates the translation gizmo
	if (!m_selectedEntities.empty() && !ImGui::GetIO().WantCaptureMouse && !wallMountedEntity && !isConnectingActivator)
	{
		if (!m_translationGizmo.HasInputFocus())
		{
			m_gizmoPosUnaligned = glm::vec3(0.0f);
			for (int64_t i = m_selectedEntities.size() - 1; i >= 0; i--)
			{
				if (m_selectedEntities[i].FindComponent<ECWallMounted>())
				{
					m_selectedEntities[i] = m_selectedEntities.back();
					m_selectedEntities.pop_back();
				}
				else
				{
					m_gizmoPosUnaligned += m_selectedEntities[i].Get()->GetComponent<eg::ECPosition3D>().position;
				}
			}
			m_gizmoPosUnaligned /= (float)m_selectedEntities.size();
			m_prevGizmoPos = m_gizmoPosUnaligned;
		}
		
		m_translationGizmo.Update(m_gizmoPosUnaligned, RenderSettings::instance->cameraPosition,
		                          RenderSettings::instance->viewProjection, m_viewRay);
		
		const glm::vec3 gizmoPosAligned = SnapToGrid(m_gizmoPosUnaligned);
		const glm::vec3 dragDelta = gizmoPosAligned - m_prevGizmoPos;
		if (glm::length2(dragDelta) > 1E-6f)
		{
			MaybeClone();
			for (const eg::EntityHandle& selectedEntity : m_selectedEntities)
			{
				selectedEntity.Get()->GetComponent<eg::ECPosition3D>().position += dragDelta;
				EntityMoved(*selectedEntity.Get());
			}
		}
		m_prevGizmoPos = gizmoPosAligned;
	}
	
	//Updates entity icons
	m_entityIcons.clear();
	for (const eg::Entity& entity : m_world->EntityManager().GetEntitySet(editorVisibleSignature))
	{
		auto AddIcon = [&] (const glm::vec3& worldPos, int actConnectionIndex)
		{
			glm::vec4 sp4 = RenderSettings::instance->viewProjection * glm::vec4(worldPos, 1.0f);
			glm::vec3 sp3 = glm::vec3(sp4) / sp4.w;
			
			glm::vec2 screenPos = (glm::vec2(sp3) * 0.5f + 0.5f) *
				glm::vec2(eg::CurrentResolutionX(), eg::CurrentResolutionY());
			
			EntityIcon& icon = m_entityIcons.emplace_back();
			icon.rectangle = eg::Rectangle::CreateCentered(screenPos, ICON_SIZE, ICON_SIZE);
			icon.depth = sp3.z;
			icon.entity = entity;
			icon.actConnectionIndex = actConnectionIndex;
		};
		
		if (isConnectingActivator)
		{
			if (const ECActivatable* activatableComp = entity.FindComponent<ECActivatable>())
			{
				std::vector<glm::vec3> connectionPoints = activatableComp->GetConnectionPoints(entity);
				for (int i = 0; i < (int)connectionPoints.size(); i++)
				{
					AddIcon(connectionPoints[i], i);
				}
			}
		}
		else if (const eg::ECPosition3D* positionComp = entity.FindComponent<eg::ECPosition3D>())
		{
			AddIcon(positionComp->position, -1);
		}
	}
	
	std::sort(m_entityIcons.begin(), m_entityIcons.end(), [&] (const EntityIcon& a, const EntityIcon& b)
	{
		return a.depth > b.depth;
	});
	
	glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	
	//Updates which entities are selected
	if (mouseClicked && AllowMouseInteract())
	{
		if (!eg::IsButtonDown(eg::Button::LeftControl) && !eg::IsButtonDown(eg::Button::RightControl))
		{
			m_selectedEntities.clear();
		}
		
		for (int64_t i = (int64_t)m_entityIcons.size() - 1; i >= 0; i--)
		{
			if (m_entityIcons[i].rectangle.Contains(flippedCursorPos))
			{
				//Connects the activator to the selected entity
				if (eg::Entity* activatorEntity = m_connectingActivator.Get())
				{
					ECActivator& activator = activatorEntity->GetComponent<ECActivator>();
					activator.activatableName = 0;
					
					if (ECActivatable* activatable = m_entityIcons[i].entity.FindComponent<ECActivatable>())
					{
						int connectionIndex = m_entityIcons[i].actConnectionIndex;
						RemoveActConnections(m_entityIcons[i].entity, connectionIndex);
						
						activatable->SetConnected(connectionIndex);
						
						activator.activatableName = activatable->Name();
						activator.targetConnectionIndex = connectionIndex;
						m_actConnections.push_back({ m_connectingActivator, m_entityIcons[i].entity });
						ECActivationLightStrip::GenerateForActivator(*m_world, *activatorEntity);
					}
					
					m_connectingActivator = { };
					break;
				}
				else
				{
					m_selectedEntities.push_back(m_entityIcons[i].entity);
				}
			}
		}
	}
	
	//Despawns entities if delete is pressed
	if (eg::IsButtonDown(eg::Button::Delete) && !eg::WasButtonDown(eg::Button::Delete))
	{
		for (const eg::EntityHandle& entity : m_selectedEntities)
		{
			entity.Get()->Despawn();
		}
		m_selectedEntities.clear();
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
		for (const SpawnableEntityType& entityType : spawnableEntityTypes)
		{
			if (ImGui::MenuItem(entityType.name.c_str()))
			{
				WallRayIntersectResult pickResult = m_world->RayIntersectWall(m_viewRay);
				
				if (pickResult.intersected)
				{
					if (eg::Entity* entity = entityType.factory(m_world->EntityManager()))
					{
						if (eg::ECPosition3D* positionComp = entity->FindComponent<eg::ECPosition3D>())
							positionComp->position = pickResult.intersectPosition;
						if (ECWallMounted* wallMountedComp = entity->FindComponent<ECWallMounted>())
							wallMountedComp->wallUp = pickResult.normalDir;
						
						EditorSpawnedMessage spawnedMessage;
						spawnedMessage.wallPosition = pickResult.intersectPosition;
						spawnedMessage.wallNormal = pickResult.normalDir;
						spawnedMessage.world = m_world.get();
						entity->HandleMessage(spawnedMessage);
					}
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
	
	//Handles mouse move events
	if (eg::IsButtonDown(eg::Button::MouseLeft) && eg::WasButtonDown(eg::Button::MouseLeft) &&
	    eg::CursorPos() != eg::PrevCursorPos())
	{
		WallRayIntersectResult pickResult = m_world->RayIntersectWall(m_viewRay);
		
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
			float closestPoint = dragLineRay.GetClosestPoint(m_viewRay);
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
					
					//Checks if all the next blocks are air.
					//In this case they should be filled in, otherwise carved out.
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
					
					const int airMode = allAir ? 1 : 2;
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
									m_world->SetIsAir(glm::ivec3(x, y, z), !allAir);
								}
							}
						}
						
						const Dir texSourceSide = m_selectionNormal;
						
						const glm::ivec3 absNormal = glm::abs(DirectionVector(m_selectionNormal));
						glm::ivec3 texSourceOffset, texDestOffset;
						if (allAir)
						{
							texDestOffset = absNormal * newDragDir;
						}
						else
						{
							texSourceOffset = absNormal * (-newDragDir);
						}
						
						for (int x = mn.x; x <= mx.x; x++)
						{
							for (int y = mn.y; y <= mx.y; y++)
							{
								for (int z = mn.z; z <= mx.z; z++)
								{
									const glm::ivec3 pos(x, y, z);
									const uint8_t texture = m_world->GetTexture(pos + texSourceOffset, texSourceSide);
									m_world->SetTextureSafe(pos + texDestOffset, texSourceSide, texture);
									for (int s = 0; s < 4; s++)
									{
										const int stepDim = (selDim + 1 + s / 2) % 3;
										const Dir stepDir = (Dir)(stepDim * 2 + s % 2);
										if (!allAir)
										{
											m_world->SetTextureSafe(pos, stepDir, texture);
										}
										else
										{
											const glm::ivec3 npos = pos + DirectionVector(stepDir);
											m_world->SetTextureSafe(npos, stepDir, texture);
										}
									}
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
	m_primRenderer.Begin(RenderSettings::instance->viewProjection, RenderSettings::instance->cameraPosition);
	m_spriteBatch.Begin();
	m_renderCtx->meshBatch.Begin();
	
	m_prepareDrawArgs.spotLights.clear();
	m_prepareDrawArgs.pointLights.clear();
	m_prepareDrawArgs.reflectionPlanes.clear();
	m_world->PrepareForDraw(m_prepareDrawArgs);
	
	if (m_tool == Tool::Walls)
		DrawToolWalls();
	else if (m_tool == Tool::Corners)
		DrawToolCorners();
	else if (m_tool == Tool::Entities)
		DrawToolEntities();
	
	//Sends the editor draw message to entities
	EditorDrawMessage drawMessage;
	drawMessage.spriteBatch = &m_spriteBatch;
	drawMessage.primitiveRenderer = &m_primRenderer;
	drawMessage.meshBatch = &m_renderCtx->meshBatch;
	drawMessage.getDrawMode = [this] (const eg::Entity* entity)
	{
		if (eg::Contains(m_selectedEntities, eg::EntityHandle(*entity)))
			return EntityEditorDrawMode::Selected;
		return EntityEditorDrawMode::Default;
	};
	m_world->EntityManager().SendMessageToAll(drawMessage);
	
	m_primRenderer.End();
	
	m_renderCtx->meshBatch.End(eg::DC);
	
	m_liquidPlaneRenderer.Prepare(*m_world, RenderSettings::instance->cameraPosition);
	
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
	
	if (m_tool == Tool::Entities)
	{
		for (const eg::EntityHandle& entity : m_selectedEntities)
		{
			if (!entity.FindComponent<ECWallMounted>())
			{
				m_translationGizmo.Draw(RenderSettings::instance->viewProjection);
				break;
			}
		}
	}
	
	m_liquidPlaneRenderer.Render();
	
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

static constexpr float ACT_CONNECTION_LINE_WIDTH = 0.06f;
static const eg::ColorSRGB ACT_CONNECTION_COLOR = eg::ColorSRGB::FromRGBAHex(0x0870c8c8);

void Editor::DrawToolEntities()
{
	//Draws activation connections
	for (const ActConnection& connection : m_actConnections)
	{
		if (eg::Contains(m_selectedEntities, connection.activator) || eg::Contains(m_selectedEntities, connection.activatable))
		{
			const ECActivator* activatorComp = connection.activator.FindComponent<ECActivator>();
			const ECActivatable* activatableComp = connection.activatable.FindComponent<ECActivatable>();
			
			if (activatorComp != nullptr && activatableComp != nullptr)
			{
				glm::vec3 a = eg::GetEntityPosition(eg::Deref(connection.activator.Get()));
				glm::vec3 b = eg::GetEntityPosition(eg::Deref(connection.activatable.Get()));
				m_primRenderer.AddLine(a, b, ACT_CONNECTION_COLOR, ACT_CONNECTION_LINE_WIDTH);
			}
		}
	}
	
	const eg::Texture& iconsTexture = eg::GetAsset<eg::Texture>("Textures/EntityIcons.png");
	
	auto CreateSrcRectangle = [&] (int index)
	{
		return eg::Rectangle(index * 50, 0, 50, 50);
	};
	
	for (const EntityIcon& icon : m_entityIcons)
	{
		const bool selected = eg::Contains(m_selectedEntities, icon.entity);
		
		eg::ColorLin color(eg::Color::White);
		
		//Draws the background sprite
		m_spriteBatch.Draw(iconsTexture, icon.rectangle, color,
			CreateSrcRectangle(selected ? 1 : 0), eg::FlipFlags::Normal);
		
		//Selects which icon to use
		int iconIndex = 5;
		if (icon.actConnectionIndex != -1)
		{
			iconIndex = 6;
		}
		else if (const ECEditorVisible* edVisible = icon.entity.FindComponent<ECEditorVisible>())
		{
			iconIndex = edVisible->iconIndex;
		}
		
		//Draws the icon
		m_spriteBatch.Draw(iconsTexture, icon.rectangle, color, CreateSrcRectangle(iconIndex), eg::FlipFlags::Normal);
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
