#include "Editor.hpp"
#include "../Levels.hpp"

#include <filesystem>
#include <fstream>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

Editor* editor;

Editor::Editor(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx)
{
	
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
	
	glm::mat4 viewMatrix, inverseViewMatrix;
	m_camera.GetViewMatrix(viewMatrix, inverseViewMatrix);
	
	m_renderCtx->renderSettings.viewProjection = m_renderCtx->projection.Matrix() * viewMatrix;
	m_renderCtx->renderSettings.invViewProjection = inverseViewMatrix * m_renderCtx->projection.InverseMatrix();
	m_renderCtx->renderSettings.UpdateBuffer();
	
	auto viewRay = eg::MakeLazy([&]
	{
		return eg::Ray::UnprojectScreen(m_renderCtx->renderSettings.invViewProjection, eg::CursorPos());
	});
	
	if (!eg::console::IsShown())
	{
		m_camera.Update(dt);
		
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
						
						for (int x = mn.x; x <= mx.x; x++)
						{
							for (int y = mn.y; y <= mx.y; y++)
							{
								for (int z = mn.z; z <= mx.z; z++)
								{
									m_world->SetIsAir({x, y, z}, !m_world->IsAir({x, y, z}));
								}
							}
						}
						
						m_selection1[selDim] += newDragDist - m_dragDistance;
						m_selection2[selDim] += newDragDist - m_dragDistance;
						m_dragDistance = newDragDist;
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
	
	m_selection2Anim += std::min(dt * 20, 1.0f) * (glm::vec3(m_selection2) - m_selection2Anim);
	
	DrawWorld();
}

void Editor::DrawWorld()
{
	m_primRenderer.Begin(m_renderCtx->renderSettings.viewProjection);
	m_renderCtx->objectRenderer.Begin();
	
	m_world->PrepareForDraw(m_renderCtx->objectRenderer);
	
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
		
		eg::Color color;
		if (m_selState == SelState::Selecting)
		{
			color = eg::ColorSRGB::FromHex(0x91CAED).ScaleAlpha(0.5f);
		}
		else if (m_selState == SelState::Dragging)
		{
			color = eg::ColorSRGB::FromHex(0xDB9951).ScaleAlpha(0.4f);
		}
		else
		{
			color = eg::ColorSRGB::FromHex(0x91CAED).ScaleAlpha(0.4f);
		}
		
		m_primRenderer.AddQuad(quadCorners, color);
	}
	
	m_primRenderer.End();
	m_renderCtx->objectRenderer.End();
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorSRGB::FromHex(0x1C1E26);
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	m_world->Draw(m_renderCtx->renderSettings);
	
	m_renderCtx->objectRenderer.Draw(m_renderCtx->renderSettings);
	
	m_primRenderer.Draw();
	
	eg::DC.EndRenderPass();
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
