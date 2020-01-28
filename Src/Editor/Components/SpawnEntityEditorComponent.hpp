#pragma once

#include "../EditorComponent.hpp"

class SpawnEntityEditorComponent : public EditorComponent
{
public:
	SpawnEntityEditorComponent();
	
	void Update(float dt, const EditorState& editorState) override;
	
	bool UpdateInput(float dt, const EditorState& editorState) override;
	
private:
	std::vector<const EntType*> m_spawnEntityList;
	WallRayIntersectResult m_spawnEntityPickResult;
	bool m_open = false;
};


