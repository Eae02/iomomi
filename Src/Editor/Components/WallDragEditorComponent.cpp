#include "WallDragEditorComponent.hpp"
#include "../PrimitiveRenderer.hpp"
#include "../../Graphics/WallShader.hpp"
#include "../../ImGuiInterface.hpp"

#include <imgui.h>

void WallDragEditorComponent::Update(float dt, const EditorState& editorState)
{
	m_selection2Anim += std::min(dt * 20, 1.0f) * (glm::vec3(m_selection2) - m_selection2Anim);
}

constexpr float DOUBLE_CLICK_TIMEOUT = 0.2f;

bool WallDragEditorComponent::UpdateInput(float dt, const EditorState& editorState)
{
	WallRayIntersectResult pickResult;// = editorState.world->RayIntersectWall(editorState.viewRay);
	bool hasPickResult = false;
	auto DoWallRayIntersect = [&] ()
	{
		if (!hasPickResult)
		{
			pickResult = editorState.world->RayIntersectWall(editorState.viewRay);
			hasPickResult = true;
		}
	};
	
	//Handles mouse move events
	if (eg::IsButtonDown(eg::Button::MouseLeft) && eg::WasButtonDown(eg::Button::MouseLeft) &&
	    eg::CursorPos() != eg::PrevCursorPos())
	{
		int selDim = (int)m_selectionNormal / 2;
		if (m_selState == SelState::Selecting)
		{
			//Updates the current selection if the new hovered voxel is
			// in the same plane as the voxel where the selection started.
			DoWallRayIntersect();
			if (pickResult.intersected && pickResult.voxelPosition[selDim] == m_selection1[selDim])
			{
				m_selection2 = pickResult.voxelPosition;
			}
		}
		else if (m_selState == SelState::Dragging)
		{
			eg::Ray dragLineRay(m_dragStartPos, glm::abs(DirectionVector(m_selectionNormal)));
			float closestPoint = dragLineRay.GetClosestPoint(editorState.viewRay);
			int newDragDist = (int)glm::round(closestPoint);
			if (!std::isnan(closestPoint) && newDragDist != m_dragDistance)
			{
				int newDragDir = newDragDist > m_dragDistance ? 1 : -1;
				if (newDragDir != m_dragDir)
					m_dragAirMode = 0;
				
				int dragDelta = newDragDist - m_dragDistance;
				int selDimOffsetMin = 0;
				int selDimOffsetMax = 0;
				
				if (dragDelta > 0)
				{
					selDimOffsetMax += dragDelta - 1;
				}
				else
				{
					selDimOffsetMin += dragDelta;
					selDimOffsetMax--;
				}
				
				if ((int)m_selectionNormal % 2 == 0)
				{
					selDimOffsetMin++;
					selDimOffsetMax++;
				}
				
				//Checks if all the next blocks are air.
				//In this case they should be filled in, otherwise carved out.
				bool allAir = true;
				IterateSelection([&] (const glm::ivec3& pos)
				{
					if (!editorState.world->IsAir(pos))
						allAir = false;
				}, selDimOffsetMin, selDimOffsetMax);
				
				const int airMode = allAir ? 1 : 2;
				if (m_dragAirMode == 0)
					m_dragAirMode = airMode;
				
				if (m_dragAirMode == airMode)
				{
					auto UpdateIsAir = [&] ()
					{
						editorState.InvalidateWater();
						IterateSelection([&] (const glm::ivec3& pos)
						{
							editorState.world->SetIsAir(pos, !allAir);
						}, selDimOffsetMin, selDimOffsetMax);
					};
					
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
						UpdateIsAir();
					}
					
					IterateSelection([&] (const glm::ivec3& pos)
					{
						int material = editorState.world->GetMaterial(pos + texSourceOffset, texSourceSide);
						editorState.world->SetMaterialSafe(pos + texDestOffset, texSourceSide, material);
						for (int s = 0; s < 4; s++)
						{
							const int stepDim = (selDim + 1 + s / 2) % 3;
							const Dir stepDir = (Dir)(stepDim * 2 + s % 2);
							if (!allAir)
							{
								editorState.world->SetMaterialSafe(pos, stepDir, material);
							}
							else
							{
								const glm::ivec3 npos = pos + DirectionVector(stepDir);
								editorState.world->SetMaterialSafe(npos, stepDir, material);
							}
						}
					}, selDimOffsetMin, selDimOffsetMax);
					
					if (allAir)
					{
						UpdateIsAir();
					}
					
					for (glm::ivec3& selected : m_finishedSelection)
					{
						selected[selDim] += newDragDist - m_dragDistance;
					}
					
					m_dragDir = newDragDist > m_dragDistance ? 1 : -1;
					m_dragDistance = newDragDist;
				}
			}
		}
		else
		{
			DoWallRayIntersect();
			if (m_selState == SelState::SelectDone && pickResult.intersected &&
				eg::Contains(m_finishedSelection, pickResult.voxelPosition))
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
	}
	
	//Handles mouse up events. These should complete the selection if the user was selecting,
	// go back to this mode if the user was dragging, or clear the selection otherwise.
	if (eg::WasButtonDown(eg::Button::MouseLeft) && !eg::IsButtonDown(eg::Button::MouseLeft))
	{
		if (m_selState == SelState::Selecting)
		{
			m_selState = SelState::SelectDone;
			m_finishedSelection.clear();
			glm::ivec3 mn = glm::min(m_selection1, m_selection2);
			glm::ivec3 mx = glm::max(m_selection1, m_selection2);
			for (int x = mn.x; x <= mx.x; x++)
			{
				for (int y = mn.y; y <= mx.y; y++)
				{
					for (int z = mn.z; z <= mx.z; z++)
					{
						glm::ivec3 pos(x, y, z);
						if (editorState.world->IsAir(pos + DirectionVector(m_selectionNormal)) &&
							!editorState.world->IsAir(pos))
						{
							m_finishedSelection.emplace_back(x, y, z);
						}
					}
				}
			}
		}
		else if (m_selState == SelState::Dragging)
		{
			m_selState = SelState::SelectDone;
		}
		else
		{
			m_finishedSelection.clear();
			m_selState = SelState::NoSelection;
			if (m_timeSinceLastClick < DOUBLE_CLICK_TIMEOUT)
			{
				DoWallRayIntersect();
				if (pickResult.intersected)
				{
					FillSelection(*editorState.world, pickResult.voxelPosition, pickResult.normalDir);
					if (!m_finishedSelection.empty())
					{
						m_selectionNormal = pickResult.normalDir;
						m_selState = SelState::SelectDone;
					}
				}
			}
			m_timeSinceLastClick = 0;
		}
	}
	
	m_timeSinceLastClick += dt;
	
	return false;
}

void WallDragEditorComponent::FillSelection(const World& world, const glm::ivec3& pos, Dir normalDir)
{
	if (!world.IsAir(pos + DirectionVector(normalDir)) || world.IsAir(pos) || eg::Contains(m_finishedSelection, pos))
	{
		return;
	}
	
	m_finishedSelection.push_back(pos);
	
	int d1 = ((int)normalDir / 2 + 1) % 3;
	int d2 = ((int)normalDir / 2 + 2) % 3;
	static const glm::ivec2 toNeighbors[] = { { -1, 0 }, { 1, 0 }, { 0, 1 }, { 0, -1 } };
	for (const glm::ivec2& toNeighbor : toNeighbors)
	{
		glm::ivec3 neighbor = pos;
		neighbor[d1] += toNeighbor.x;
		neighbor[d2] += toNeighbor.y;
		FillSelection(world, neighbor, normalDir);
	}
}

void WallDragEditorComponent::EarlyDraw(PrimitiveRenderer& primitiveRenderer) const
{
	if (m_selState != SelState::NoSelection)
	{
		int nd = (int)m_selectionNormal / 2;
		int sd1 = (nd + 1) % 3;
		int sd2 = (nd + 2) % 3;
		
		if (m_selState == SelState::Selecting)
		{
			glm::vec3 quadCorners[4];
			for (glm::vec3& corner : quadCorners)
			{
				corner[nd] = m_selection1[nd] + (((int)m_selectionNormal % 2) ? 0 : 1);
			}
			quadCorners[1][sd1] = quadCorners[0][sd1] = std::min<float>(m_selection1[sd1], m_selection2Anim[sd1]);
			quadCorners[3][sd1] = quadCorners[2][sd1] = std::max<float>(m_selection1[sd1], m_selection2Anim[sd1]) + 1;
			quadCorners[2][sd2] = quadCorners[0][sd2] = std::min<float>(m_selection1[sd2], m_selection2Anim[sd2]);
			quadCorners[1][sd2] = quadCorners[3][sd2] = std::max<float>(m_selection1[sd2], m_selection2Anim[sd2]) + 1;
			
			primitiveRenderer.AddQuad(quadCorners, eg::ColorSRGB(eg::ColorSRGB::FromHex(0x91CAED).ScaleAlpha(0.5f)));
		}
		else
		{
			eg::ColorSRGB color;
			if (m_selState == SelState::Dragging)
			{
				color = eg::ColorSRGB(eg::ColorSRGB::FromHex(0xDB9951).ScaleAlpha(0.4f));
			}
			else
			{
				color = eg::ColorSRGB(eg::ColorSRGB::FromHex(0x91CAED).ScaleAlpha(0.4f));
			}
			
			for (glm::ivec3 pos : m_finishedSelection)
			{
				glm::vec3 quadCorners[4];
				glm::vec3* nextQuadCorner = quadCorners;
				
				for (int dx = 0; dx < 2; dx++)
				for (int dy = 0; dy < 2; dy++)
				{
					(*nextQuadCorner)[nd] = pos[nd] + (((int)m_selectionNormal % 2) ? 0 : 1);
					(*nextQuadCorner)[sd1] = pos[sd1] + dx;
					(*nextQuadCorner)[sd2] = pos[sd2] + dy;
					nextQuadCorner++;
				}
				
				primitiveRenderer.AddQuad(quadCorners, color);
			}
		}
	}
}

template <typename CallbackTp>
void WallDragEditorComponent::IterateSelection(CallbackTp callback,
	int minOffsetSelDir, int maxOffsetSelDir)
{
	if (m_selState == SelState::Selecting || m_selState == SelState::NoSelection)
		return;
	for (const glm::ivec3& pos : m_finishedSelection)
	{
		for (int o = minOffsetSelDir; o <= maxOffsetSelDir; o++)
		{
			glm::ivec3 pos2 = pos;
			pos2[(int)m_selectionNormal / 2] += o;
			callback(pos2);
		}
	}
}

void WallDragEditorComponent::RenderSettings(const EditorState& editorState)
{
	constexpr float ITEM_HEIGHT = 40;
	constexpr float ICON_PADDING = 3;
	
	eg::TextureRef albedoTex = eg::GetAsset<eg::Texture>("WallTextures/Albedo");
	eg::TextureRef noDrawTex = eg::GetAsset<eg::Texture>("Textures/NoDraw.png");
	
	if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::BeginChildFrame(ImGui::GetID("TexturesList"), ImVec2(0, 300));
		
		for (size_t i = 0; i < MAX_WALL_MATERIALS; i++)
		{
			if (!wallMaterials[i].initialized)
				continue;
			
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			glm::vec2 imguiCursorPos = ImGui::GetCursorScreenPos();
			
			ImGui::PushID(wallMaterials[i].name);
			if (ImGui::Selectable("##sel", false, 0, ImVec2(0, ITEM_HEIGHT)) && m_selState == SelState::SelectDone)
			{
				IterateSelection([&](glm::ivec3 pos)
				{
					editorState.world->SetMaterialSafe(pos + DirectionVector(m_selectionNormal), m_selectionNormal, i);
				}, 0, 0);
			}
			ImGui::PopID();
			
			drawList->AddImage(i ? MakeImTextureID(albedoTex, i - 1) : MakeImTextureID(noDrawTex),
			                   ImVec2(imguiCursorPos.x + ICON_PADDING, imguiCursorPos.y + ICON_PADDING),
			                   ImVec2(imguiCursorPos.x + ITEM_HEIGHT - ICON_PADDING,
			                          imguiCursorPos.y + ITEM_HEIGHT - ICON_PADDING));
			
			drawList->AddText(ImVec2(imguiCursorPos.x + ITEM_HEIGHT + 10, imguiCursorPos.y + ITEM_HEIGHT / 2 - 8),
			                  0xFFFFFFFFU, wallMaterials[i].name);
		}
		
		ImGui::EndChildFrame();
	}
}
