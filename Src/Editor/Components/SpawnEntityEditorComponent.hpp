#pragma once

#include "../EditorComponent.hpp"

class SpawnEntityEditorComponent : public EditorComponent
{
public:
	SpawnEntityEditorComponent()
		: m_searchBuffer(256) { }
	
	void Update(float dt, const EditorState& editorState) override;
	
	bool UpdateInput(float dt, const EditorState& editorState) override;
	
private:
	VoxelRayIntersectResult m_spawnEntityPickResult;
	std::vector<char> m_searchBuffer;
	bool m_open = false;
};
