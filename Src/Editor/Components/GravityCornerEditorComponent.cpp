#include "GravityCornerEditorComponent.hpp"
#include "../PrimitiveRenderer.hpp"

void GravityCornerEditorComponent::Update(float dt, const EditorState& editorState)
{
	VoxelRayIntersectResult pickResult = editorState.world->voxels.RayIntersect(editorState.viewRay);
	
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
			if (editorState.world->voxels.IsCorner(m_hoveredCornerPos, (Dir)(uDim * 2)))
				m_hoveredCornerDim = uDim;
		};
		auto TryU = [&]
		{
			if (dui > MAX_DIST)
				return;
			m_hoveredCornerPos[nDim] = ni;
			m_hoveredCornerPos[uDim] = ui;
			m_hoveredCornerPos[vDim] = std::floor(v);
			if (editorState.world->voxels.IsCorner(m_hoveredCornerPos, (Dir)(vDim * 2)))
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
}

bool GravityCornerEditorComponent::UpdateInput(float dt, const EditorState& editorState)
{
	if (eg::IsButtonDown(eg::Button::MouseLeft))
	{
		if (m_modCornerDim != m_hoveredCornerDim || m_modCornerPos != m_hoveredCornerPos)
		{
			const Dir cornerDir = (Dir)(m_hoveredCornerDim * 2);
			const bool isGravityCorner = editorState.world->voxels.IsGravityCorner(m_hoveredCornerPos, cornerDir);
			editorState.world->voxels.SetIsGravityCorner(m_hoveredCornerPos, cornerDir, !isGravityCorner);
			
			m_modCornerDim = m_hoveredCornerDim;
			m_modCornerPos = m_hoveredCornerPos;
		}
	}
	else
	{
		m_modCornerDim = -1;
	}
	
	return false;
}

void GravityCornerEditorComponent::EarlyDraw(const EditorState& editorState) const
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
		editorState.primitiveRenderer->AddQuad(lineCorners, color);
		editorState.primitiveRenderer->AddQuad(lineCorners + 4, color);
	}
}
