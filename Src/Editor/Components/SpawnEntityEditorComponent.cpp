#include "SpawnEntityEditorComponent.hpp"

#include <imgui.h>

SpawnEntityEditorComponent::SpawnEntityEditorComponent()
{
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

void SpawnEntityEditorComponent::Update(float dt, const EditorState& editorState)
{
	if (m_open)
	{
		ImGui::OpenPopup("SpawnEntity");
		m_open = false;
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
				editorState.world->entManager.AddEntity(std::move(entity));
				
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndChild();
		
		ImGui::EndPopup();
	}
}

bool SpawnEntityEditorComponent::UpdateInput(float dt, const EditorState& editorState)
{
	//Opens the spawn entity menu if the right mouse button is clicked
	if (eg::IsButtonDown(eg::Button::MouseRight) && !eg::WasButtonDown(eg::Button::MouseRight))
	{
		m_spawnEntityPickResult = editorState.world->RayIntersectWall(editorState.viewRay);
		if (m_spawnEntityPickResult.intersected)
		{
			m_open = true;
			return true;
		}
	}
	return false;
}
