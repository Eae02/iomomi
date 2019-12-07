#include "Editor.hpp"
#include "../Levels.hpp"
#include "../MainGameState.hpp"
#include "../Graphics/Materials/MeshDrawArgs.hpp"
#include "../World/Entities/Components/ActivatorComp.hpp"
#include "../World/Entities/Components/ActivatableComp.hpp"

#include <fstream>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

Editor* editor;

Editor::Editor(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx)
{
	m_prepareDrawArgs.isEditor = true;
	m_prepareDrawArgs.meshBatch = &renderCtx.meshBatch;
	m_prepareDrawArgs.player = nullptr;
	
	m_projection.SetFieldOfViewDeg(75.0f);
	
	for (const auto& entType : entTypeMap)
	{
		if (!eg::HasFlag(entType.second.flags, EntTypeFlags::EditorInvisible))
		{
			m_spawnEntityList.push_back(&entType.second);
		}
	}
	sort(m_spawnEntityList.begin(), m_spawnEntityList.end(), [&] (const EntType* a, const EntType* b)
	{
		return a->prettyName < b->prettyName;
	});
}

const char* TextureNames[] =
{
	"No Draw",
	"Tactile Gray",
	"Clear Metal",
	"Metal Grid",
	"Cement",
	"Panels 1",
	"Panels 2"
};

void Editor::InitWorld()
{
	m_world->entManager.isEditor = true;
	m_selectedEntities.clear();
	ActivationLightStripEnt::GenerateAll(*m_world);
}

template <typename CallbackTp>
void Editor::IterateSelection(CallbackTp callback)
{
	if (m_selState != SelState::SelectDone)
		return;
	glm::ivec3 selMin = glm::min(m_selection1, m_selection2);
	glm::ivec3 selMax = glm::max(m_selection1, m_selection2);
	for (int x = selMin.x; x <= selMax.x; x++)
	{
		for (int y = selMin.y; y <= selMax.y; y++)
		{
			for (int z = selMin.z; z <= selMax.z; z++)
			{
				callback(glm::ivec3(x, y, z) + DirectionVector(m_selectionNormal));
			}
		}
	}
}

static constexpr int NEW_LEVEL_WALL_TEXTURE = 6;

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
								m_world->SetMaterialSafe({x, y, z}, (Dir)s, { NEW_LEVEL_WALL_TEXTURE, false });
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
		if (ImGui::Begin("Level Settings", &m_levelSettingsOpen))
		{
			ImGui::Checkbox("Has Gravity Gun", &m_world->playerHasGravityGun);
		}
		ImGui::End();
	}
	
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
		SetCurrentGS(mainGameState);
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
		bool anyReflective = false;
		bool anyNotReflective = false;
		if (m_selState == SelState::SelectDone)
		{
			IterateSelection([&] (glm::ivec3 pos)
			{
				WallSideMaterial material = m_world->GetMaterial(pos, m_selectionNormal);
				if (material.enableReflections)
					anyReflective = true;
				else
					anyNotReflective = true;
			});
		}
		
		bool checkboxSet = anyReflective && !anyNotReflective;
		if (ImGui::Checkbox("Reflective", &checkboxSet))
		{
			IterateSelection([&] (glm::ivec3 pos)
			{
				WallSideMaterial material = m_world->GetMaterial(pos, m_selectionNormal);
				material.enableReflections = checkboxSet;
				m_world->SetMaterialSafe(pos, m_selectionNormal, material);
			});
		}
		
		if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::BeginChildFrame(ImGui::GetID("TexturesList"), ImVec2(0, 300));
			
			for (size_t i = 0; i < eg::ArrayLen(TextureNames); i++)
			{
				if (ImGui::Selectable(TextureNames[i], false) &&  m_selState == SelState::SelectDone)
				{
					IterateSelection([&] (glm::ivec3 pos)
					{
						WallSideMaterial material = m_world->GetMaterial(pos, m_selectionNormal);
						material.texture = i;
						m_world->SetMaterialSafe(pos, m_selectionNormal, material);
					});
				}
			}
			
			ImGui::EndChildFrame();
		}
	}
	else if (m_tool == Tool::Entities)
	{
		for (const std::weak_ptr<Ent>& entityHandle : m_selectedEntities)
		{
			std::shared_ptr<Ent> entity = entityHandle.lock();
			if (entity == nullptr)
				continue;
			
			const EntType& entType = entTypeMap.at(entity->TypeID());
			if (eg::HasFlag(entType.flags, EntTypeFlags::EditorInvisible))
				continue;
			
			ImGui::PushID(entity->Name());
			if (ImGui::CollapsingHeader(entType.prettyName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				entity->RenderSettings();
				
				if (entity->GetComponent<ActivatorComp>())
				{
					if (ImGui::Button("Connect..."))
					{
						m_connectingActivator = entity;
					}
				}
			}
			ImGui::PopID();
		}
	}
	ImGui::End();
	
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
		const float STEP = eg::IsButtonDown(eg::Button::LeftAlt) ? 0.5f : 0.1f;
		return glm::round(pos / STEP) * STEP;
	};
	
	auto MaybeClone = [&] ()
	{
		if (!eg::IsButtonDown(eg::Button::LeftShift) || m_entitiesCloned)
			return false;
		
		for (std::weak_ptr<Ent>& entityWeak : m_selectedEntities)
		{
			if (std::shared_ptr<Ent> entity = entityWeak.lock())
			{
				if ((entity = entity->Clone()))
				{
					entityWeak = entity;
					m_world->entManager.AddEntity(std::move(entity));
				}
			}
		}
		m_entitiesCloned = true;
		return true;
	};
	
	if (!eg::IsButtonDown(eg::Button::LeftShift))
		m_entitiesCloned = false;
	
	const bool mouseClicked = eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft);
	const bool mouseHeld = eg::IsButtonDown(eg::Button::MouseLeft) && eg::WasButtonDown(eg::Button::MouseLeft);
	std::shared_ptr<Ent> connectingActivatorEnt = m_connectingActivator.lock();
	std::shared_ptr<ActivationLightStripEnt> editingLightStripEnt = m_editingLightStripEntity.lock();
	
	auto EntityMoved = [&] (Ent& entity)
	{
		if (ActivatableComp* activatable = entity.GetComponentMut<ActivatableComp>())
		{
			m_world->entManager.ForEach([&] (Ent& activatorEntity)
			{
				ActivatorComp* activator = activatorEntity.GetComponentMut<ActivatorComp>();
				if (activator && activator->activatableName == activatable->m_name)
				{
					ActivationLightStripEnt::GenerateForActivator(*m_world, activatorEntity);
				}
			});
		}
		
		ActivationLightStripEnt::GenerateForActivator(*m_world, entity);
	};
	
	std::shared_ptr<Ent> wallDragEntity;
	auto UpdateWallDragEntity = [&] ()
	{
		if (m_selectedEntities.size() == 1)
		{
			wallDragEntity = m_selectedEntities[0].lock();
			if (!eg::HasFlag(wallDragEntity->TypeFlags(), EntTypeFlags::EditorWallMove))
				wallDragEntity = nullptr;
		}
	};
	UpdateWallDragEntity();
	
	//Moves the wall drag entity
	if (wallDragEntity != nullptr && !connectingActivatorEnt && !editingLightStripEnt && mouseHeld)
	{
		constexpr int DRAG_BEGIN_DELTA = 10;
		if (m_isDraggingWallEntity)
		{
			WallRayIntersectResult pickResult = m_world->RayIntersectWall(m_viewRay);
			if (pickResult.intersected)
			{
				if (MaybeClone())
					UpdateWallDragEntity();
				
				wallDragEntity->EditorMoved(SnapToGrid(pickResult.intersectPosition), pickResult.normalDir);
				EntityMoved(*wallDragEntity);
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
	if (!m_selectedEntities.empty() && !ImGui::GetIO().WantCaptureMouse &&
		wallDragEntity == nullptr && !connectingActivatorEnt && !editingLightStripEnt)
	{
		if (!m_translationGizmo.HasInputFocus())
		{
			m_gizmoPosUnaligned = glm::vec3(0.0f);
			for (int64_t i = m_selectedEntities.size() - 1; i >= 0; i--)
			{
				std::shared_ptr<Ent> selectedEntitySP = m_selectedEntities[i].lock();
				if (!selectedEntitySP || eg::HasFlag(selectedEntitySP->TypeFlags(), EntTypeFlags::EditorWallMove))
				{
					m_selectedEntities[i].swap(m_selectedEntities.back());
					m_selectedEntities.pop_back();
				}
				else
				{
					m_gizmoPosUnaligned += selectedEntitySP->Pos();
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
			for (const std::weak_ptr<Ent>& selectedEntity : m_selectedEntities)
			{
				if (std::shared_ptr<Ent> selectedEntitySP = selectedEntity.lock())
				{
					selectedEntitySP->EditorMoved(selectedEntitySP->Pos() + dragDelta, {});
					EntityMoved(*selectedEntitySP);
				}
			}
		}
		m_prevGizmoPos = gizmoPosAligned;
	}
	
	if (editingLightStripEnt)
	{
		std::shared_ptr<Ent> activatorEnt = editingLightStripEnt->ActivatorEntity().lock();
		ActivatorComp* activator = activatorEnt ? activatorEnt->GetComponentMut<ActivatorComp>() : nullptr;
		
		if (activator != nullptr)
		{
			WallRayIntersectResult pickResult = m_world->RayIntersectWall(m_viewRay);
			if (pickResult.intersected)
			{
				activator->waypoints[m_editingWayPointIndex] = { pickResult.normalDir, pickResult.intersectPosition };
				ActivationLightStripEnt::GenerateForActivator(*m_world, *activatorEnt);
			}
		}
		
		if (!eg::IsButtonDown(eg::Button::MouseLeft) || activator == nullptr)
		{
			m_editingLightStripEntity = { };
			m_editingWayPointIndex = -1;
		}
	}
	
	//Updates entity icons
	m_entityIcons.clear();
	auto AddIcon = [&] (const glm::vec3& worldPos, IconType type, std::weak_ptr<Ent> entity) -> EntityIcon&
	{
		glm::vec4 sp4 = RenderSettings::instance->viewProjection * glm::vec4(worldPos, 1.0f);
		glm::vec3 sp3 = glm::vec3(sp4) / sp4.w;
		
		glm::vec2 screenPos = (glm::vec2(sp3) * 0.5f + 0.5f) *
			glm::vec2(eg::CurrentResolutionX(), eg::CurrentResolutionY());
		
		EntityIcon& icon = m_entityIcons.emplace_back();
		icon.rectangle = eg::Rectangle::CreateCentered(screenPos, ICON_SIZE, ICON_SIZE);
		icon.depth = sp3.z;
		icon.type = type;
		icon.entity = std::move(entity);
		icon.actConnectionIndex = -1;
		
		return icon;
	};
	
	m_world->entManager.ForEach([&] (Ent& entity)
	{
		if (eg::HasFlag(entity.TypeFlags(), EntTypeFlags::EditorInvisible))
			return;
		if (!connectingActivatorEnt)
		{
			AddIcon(entity.Pos(), IconType::Entity, entity.shared_from_this());
			return;
		}
		if (const auto* activatableComp = entity.GetComponent<ActivatableComp>())
		{
			std::vector<glm::vec3> connectionPoints = activatableComp->GetConnectionPoints(entity);
			for (int i = 0; i < (int)connectionPoints.size(); i++)
			{
				EntityIcon& icon = AddIcon(connectionPoints[i], IconType::ActTarget, entity.shared_from_this());
				icon.actConnectionIndex = i;
			}
		}
	});
	
	glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	
	if (!connectingActivatorEnt)
	{
		glm::vec3 closestPoint;
		float closestDist = INFINITY;
		std::weak_ptr<Ent> closestEntity;
		int nextWayPoint;
		Dir wallNormal;
		bool hoveringExistingPoint = false;
		
		for (const std::weak_ptr<Ent>& entity : m_selectedEntities)
		{
			std::shared_ptr<Ent> entitySP = entity.lock();
			if (entitySP == nullptr)
				continue;
			
			auto* lightStrip = dynamic_cast<ActivationLightStripEnt*>(entitySP.get());
			if (lightStrip == nullptr)
				continue;
			
			std::shared_ptr<Ent> activatorEnt = lightStrip->ActivatorEntity().lock();
			const ActivatorComp* activator = activatorEnt ? activatorEnt->GetComponent<ActivatorComp>() : nullptr;
			if (activator == nullptr)
				continue;
			
			for (size_t i = 1; i < lightStrip->Path().size(); i++)
			{
				const auto& a = lightStrip->Path()[i];
				const auto& b = lightStrip->Path()[i - 1];
				
				if (a.prevWayPoint != b.prevWayPoint)
					continue;
				
				eg::Ray lineRay(a.position, b.position - a.position);
				float closestA = lineRay.GetClosestPoint(m_viewRay);
				if (!std::isnan(closestA))
				{
					glm::vec3 newPoint = lineRay.GetPoint(glm::clamp(closestA, 0.0f, 1.0f));
					float newDist = m_viewRay.GetDistanceToPoint(newPoint);
					if (newDist < closestDist)
					{
						closestDist = newDist;
						closestPoint = newPoint;
						closestEntity = entity;
						nextWayPoint = a.prevWayPoint;
						wallNormal = a.wallNormal;
					}
				}
			}
			
			for (size_t i = 0; i < activator->waypoints.size(); i++)
			{
				EntityIcon& icon = AddIcon(activator->waypoints[i].position, IconType::ActPathExistingPoint, entity);
				icon.wayPointIndex = i;
				if (icon.rectangle.Contains(flippedCursorPos))
					hoveringExistingPoint = true;
			}
		}
		
		if (closestDist < 0.5f && !editingLightStripEnt && !hoveringExistingPoint)
		{
			EntityIcon& icon = AddIcon(closestPoint, IconType::ActPathNewPoint, closestEntity);
			icon.wayPointIndex = nextWayPoint;
			m_lightStripInsertPos = closestPoint;
			m_lightStripInsertNormal = wallNormal;
		}
	}
	
	std::sort(m_entityIcons.begin(), m_entityIcons.end(), [&] (const EntityIcon& a, const EntityIcon& b)
	{
		return a.depth < b.depth;
	});
	
	//Updates which entities are selected
	if (mouseClicked && AllowMouseInteract() && !editingLightStripEnt)
	{
		if (!eg::IsButtonDown(eg::Button::LeftControl) && !eg::IsButtonDown(eg::Button::RightControl))
		{
			m_selectedEntities.clear();
		}
		
		for (const EntityIcon& icon : m_entityIcons)
		{
			std::shared_ptr<Ent> iconEntity = icon.entity.lock();
			if (iconEntity == nullptr)
				continue;
			
			if (icon.rectangle.Contains(flippedCursorPos))
			{
				//Connects the activator to the selected entity
				if (connectingActivatorEnt != nullptr)
				{
					ActivatorComp& activator = *connectingActivatorEnt->GetComponentMut<ActivatorComp>();
					activator.activatableName = 0;
					
					if (ActivatableComp* activatable = iconEntity->GetComponentMut<ActivatableComp>())
					{
						int connectionIndex = icon.actConnectionIndex;
						
						activatable->SetConnected(connectionIndex);
						
						activator.waypoints.clear();
						activator.activatableName = activatable->m_name;
						activator.targetConnectionIndex = connectionIndex;
						ActivationLightStripEnt::GenerateForActivator(*m_world, *connectingActivatorEnt);
					}
					
					m_connectingActivator = { };
				}
				else if (icon.type == IconType::ActPathExistingPoint)
				{
					const ActivatorComp* activator = iconEntity->GetComponent<ActivatorComp>();
					if (activator != nullptr)
					{
						m_editingLightStripEntity = activator->lightStripEntity;
						m_editingWayPointIndex = icon.wayPointIndex;
					}
				}
				else if (icon.type == IconType::ActPathNewPoint)
				{
					ActivatorComp* activator = iconEntity->GetComponentMut<ActivatorComp>();
					if (activator != nullptr)
					{
						ActivationLightStripEnt::WayPoint wp;
						wp.position = m_lightStripInsertPos;
						wp.wallNormal = m_lightStripInsertNormal;
						activator->waypoints.insert(activator->waypoints.begin() + icon.wayPointIndex, wp);
						
						m_editingLightStripEntity = activator->lightStripEntity;
						m_editingWayPointIndex = icon.wayPointIndex;
					}
				}
				else if (icon.type == IconType::Entity)
				{
					m_selectedEntities.push_back(icon.entity);
				}
				break;
			}
		}
		
		if (m_editingLightStripEntity.lock())
		{
			m_selectedEntities.clear();
			m_selectedEntities.push_back(m_editingLightStripEntity);
		}
	}
	
	//Despawns entities or removes light strip way points if delete is pressed
	if (eg::IsButtonDown(eg::Button::Delete) && !eg::WasButtonDown(eg::Button::Delete))
	{
		if (std::shared_ptr<ActivationLightStripEnt> lightStripEntity = m_editingLightStripEntity.lock())
		{
			std::shared_ptr<Ent> activatorEnt = lightStripEntity->ActivatorEntity().lock();
			ActivatorComp* activator = activatorEnt ? activatorEnt->GetComponentMut<ActivatorComp>() : nullptr;
			
			if (activator != nullptr)
			{
				activator->waypoints.erase(activator->waypoints.begin() + m_editingWayPointIndex);
				ActivationLightStripEnt::GenerateForActivator(*m_world, *activatorEnt);
				
				m_editingWayPointIndex = -1;
				m_editingLightStripEntity = { };
			}
		}
		else
		{
			for (const std::weak_ptr<Ent>& entity : m_selectedEntities)
			{
				std::shared_ptr<Ent> entitySP = entity.lock();
				if (entitySP)
				{
					m_world->entManager.RemoveEntity(*entitySP);
				}
			}
		}
		m_selectedEntities.clear();
	}
	
	//Opens the spawn entity menu if the right mouse button is clicked
	if (eg::IsButtonDown(eg::Button::MouseRight) && !eg::WasButtonDown(eg::Button::MouseRight) && AllowMouseInteract())
	{
		m_spawnEntityPickResult = m_world->RayIntersectWall(m_viewRay);
		if (m_spawnEntityPickResult.intersected)
		{
			ImGui::OpenPopup("SpawnEntity");
		}
	}
	
	if (ImGui::BeginPopup("SpawnEntity"))
	{
		ImGui::Text("Spawn Entity");
		ImGui::Separator();
		
		ImGui::BeginChild("EntityTypes", ImVec2(180, 250));
		for (const EntType* entityType : m_spawnEntityList)
		{
			if (ImGui::MenuItem(entityType->prettyName.c_str()))
			{
				std::shared_ptr<Ent> entity = entityType->create();
				
				entity->EditorMoved(m_spawnEntityPickResult.intersectPosition, m_spawnEntityPickResult.normalDir);
				m_world->entManager.AddEntity(std::move(entity));
				
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
									const WallSideMaterial material = m_world->GetMaterial(pos + texSourceOffset, texSourceSide);
									m_world->SetMaterialSafe(pos + texDestOffset, texSourceSide, material);
									for (int s = 0; s < 4; s++)
									{
										const int stepDim = (selDim + 1 + s / 2) % 3;
										const Dir stepDir = (Dir)(stepDim * 2 + s % 2);
										if (!allAir)
										{
											m_world->SetMaterialSafe(pos, stepDir, material);
										}
										else
										{
											const glm::ivec3 npos = pos + DirectionVector(stepDir);
											m_world->SetMaterialSafe(npos, stepDir, material);
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
	m_renderCtx->transparentMeshBatch.Begin();
	
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
	EntEditorDrawArgs drawArgs;
	drawArgs.spriteBatch = &m_spriteBatch;
	drawArgs.primitiveRenderer = &m_primRenderer;
	drawArgs.meshBatch = &m_renderCtx->meshBatch;
	drawArgs.transparentMeshBatch = &m_renderCtx->transparentMeshBatch;
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
	
	if (m_tool == Tool::Entities)
	{
		for (const std::weak_ptr<Ent>& entity : m_selectedEntities)
		{
			std::shared_ptr<Ent> entitySP = entity.lock();
			if (entitySP && !eg::HasFlag(entitySP->TypeFlags(), EntTypeFlags::EditorWallMove))
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

void Editor::DrawToolEntities()
{
	const eg::Texture& iconsTexture = eg::GetAsset<eg::Texture>("Textures/EntityIcons.png");
	
	auto CreateSrcRectangle = [&] (int index)
	{
		return eg::Rectangle(index * 50, 0, 50, 50);
	};
	
	for (const EntityIcon& icon : m_entityIcons)
	{
		std::shared_ptr<Ent> entitySP = icon.entity.lock();
		if (!entitySP)
			continue;
		
		bool selected = std::any_of(m_selectedEntities.begin(), m_selectedEntities.end(),
			[&] (const std::weak_ptr<Ent>& e) { return e.lock() == entitySP; });
		
		eg::ColorLin color(eg::Color::White);
		
		//Selects which icon to use
		int iconIndex = 5;
		if (icon.type == IconType::ActTarget)
		{
			iconIndex = 6;
		}
		else if (icon.type == IconType::ActPathNewPoint)
		{
			iconIndex = 7;
			color = eg::ColorLin(color.ScaleAlpha(0.8f));
			selected = false;
		}
		else if (icon.type == IconType::ActPathExistingPoint)
		{
			iconIndex = 7;
			selected = false;//selected = (icon.wayPointIndex == m_editingWayPointIndex) && icon.entity == m_editingLightStripEntity;
		}
		//else if (const ECEditorVisible* edVisible = icon.entity.GetComponent<ECEditorVisible>())
		//{
		//	iconIndex = edVisible->iconIndex;
		//}
		
		//Draws the background sprite
		m_spriteBatch.Draw(iconsTexture, icon.rectangle, color,
			CreateSrcRectangle(selected ? 1 : 0), eg::SpriteFlags::None);
		
		//Draws the icon
		m_spriteBatch.Draw(iconsTexture, icon.rectangle, color, CreateSrcRectangle(iconIndex), eg::SpriteFlags::None);
	}
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
