#pragma once

#include "../EditorComponent.hpp"

class WallDragEditorComponent : public EditorComponent
{
public:
	void Update(float dt, const EditorState& editorState) override;
	
	bool UpdateInput(float dt, const EditorState& editorState) override;
	
	void EarlyDraw(class PrimitiveRenderer& primitiveRenderer) const override;
	
	void RenderSettings(const EditorState& editorState) override;
	
private:
	template <typename CallbackTp>
	void IterateSelection(CallbackTp callback);
	
	enum class SelState
	{
		NoSelection,
		Selecting,
		SelectDone,
		Dragging
	};
	
	glm::vec3 m_dragStartPos;
	int m_dragDistance;
	int m_dragDir;
	int m_dragAirMode;
	
	SelState m_selState = SelState::NoSelection;
	glm::ivec3 m_selection1;
	glm::ivec3 m_selection2;
	glm::vec3 m_selection2Anim;
	Dir m_selectionNormal;
};
