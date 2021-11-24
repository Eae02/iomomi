#pragma once

#ifndef IOMOMI_NO_EDITOR

#include "../EditorComponent.hpp"

class AAQuadDragEditorComponent : public EditorComponent
{
public:
	bool UpdateInput(float dt, const EditorState& editorState) override;
	
	bool CollectIcons(const EditorState& editorState, std::vector<EditorIcon>& icons) override;
	
private:
	std::weak_ptr<Ent> m_activeEntity;
	bool draggingBitangent = false;
	bool draggingNegative = false;
};

#endif
