#include "WallDragEditorComponent.hpp"
#include "../PrimitiveRenderer.hpp"

#include <imgui.h>

void WallDragEditorComponent::Update(float dt, const EditorState& editorState)
{
	m_selection2Anim += std::min(dt * 20, 1.0f) * (glm::vec3(m_selection2) - m_selection2Anim);
}

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
								if (!editorState.world->IsAir({x, y, z}))
									allAir = false;
							}
						}
					}
					
					const int airMode = allAir ? 1 : 2;
					if (m_dragAirMode == 0)
						m_dragAirMode = airMode;
					
					if (m_dragAirMode == airMode)
					{
						auto UpdateIsAir = [&] ()
						{
							for (int x = mn.x; x <= mx.x; x++)
							{
								for (int y = mn.y; y <= mx.y; y++)
								{
									for (int z = mn.z; z <= mx.z; z++)
									{
										editorState.world->SetIsAir(glm::ivec3(x, y, z), !allAir);
									}
								}
							}
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
						
						for (int x = mn.x; x <= mx.x; x++)
						{
							for (int y = mn.y; y <= mx.y; y++)
							{
								for (int z = mn.z; z <= mx.z; z++)
								{
									const glm::ivec3 pos(x, y, z);
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
								}
							}
						}
						
						if (allAir)
						{
							UpdateIsAir();
						}
						
						m_selection1[selDim] += newDragDist - m_dragDistance;
						m_selection2[selDim] += newDragDist - m_dragDistance;
						m_dragDir = newDragDist > m_dragDistance ? 1 : -1;
						m_dragDistance = newDragDist;
					}
				}
			}
		}
		else
		{
			DoWallRayIntersect();
			if (m_selState == SelState::SelectDone && pickResult.intersected &&
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
	
	return false;
}

void WallDragEditorComponent::EarlyDraw(PrimitiveRenderer& primitiveRenderer) const
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
		
		primitiveRenderer.AddQuad(quadCorners, color);
	}
}

template <typename CallbackTp>
void WallDragEditorComponent::IterateSelection(CallbackTp callback)
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

static const char* WALL_TEXTURE_NAMES[] =
{
	"No Draw",
	"Tactile Gray",
	"Clear Metal",
	"Metal Grid",
	"Cement",
	"Panels 1",
	"Panels 2",
	"Panels 1 (Striped)",
};

void WallDragEditorComponent::RenderSettings(const EditorState& editorState)
{
	if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::BeginChildFrame(ImGui::GetID("TexturesList"), ImVec2(0, 300));
		
		for (size_t i = 0; i < eg::ArrayLen(WALL_TEXTURE_NAMES); i++)
		{
			if (ImGui::Selectable(WALL_TEXTURE_NAMES[i], false) && m_selState == SelState::SelectDone)
			{
				IterateSelection([&] (glm::ivec3 pos)
				{
					editorState.world->SetMaterialSafe(pos, m_selectionNormal, i);
				});
			}
		}
		
		ImGui::EndChildFrame();
	}
}
