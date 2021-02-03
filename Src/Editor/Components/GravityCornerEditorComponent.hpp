#pragma once

#include "../EditorComponent.hpp"

class GravityCornerEditorComponent : public EditorComponent
{
public:
	void Update(float dt, const EditorState& editorState) override;
	
	bool UpdateInput(float dt, const EditorState& editorState) override;
	
	void EarlyDraw(const EditorState& editorState) const override;
	
private:
	int m_hoveredCornerDim = -1;
	glm::ivec3 m_hoveredCornerPos;
	int m_modCornerDim = -1;
	glm::ivec3 m_modCornerPos;
};
