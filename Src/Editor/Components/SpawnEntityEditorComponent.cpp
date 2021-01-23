#include "SpawnEntityEditorComponent.hpp"

#include <imgui.h>

const static std::pair<std::string, std::vector<EntTypeID>> entityGroups[] = 
{
	{ "Visual", { EntTypeID::WallLight, EntTypeID::Decal, EntTypeID::Window, EntTypeID::Ramp, EntTypeID::Mesh } },
	{ "Gameplay", {
		EntTypeID::EntranceExit, EntTypeID::Cube, EntTypeID::CubeSpawner, EntTypeID::Platform, EntTypeID::SlidingWall,
		EntTypeID::ForceField, EntTypeID::GravityBarrier, EntTypeID::GravitySwitch, EntTypeID::FloorButton, EntTypeID::Ladder
	} },
	{ "Water", { EntTypeID::GooPlane, EntTypeID::WaterPlane, EntTypeID::WaterWall } }
};

void SpawnEntityEditorComponent::Update(float dt, const EditorState& editorState)
{
	if (m_open)
	{
		m_searchBuffer[0] = '\0';
		ImGui::OpenPopup("SpawnEntity");
		m_open = false;
	}
	
	if (ImGui::BeginPopup("SpawnEntity"))
	{
		ImGui::Text("Spawn Entity");
		
		ImGui::InputText("##search", m_searchBuffer.data(), m_searchBuffer.size());
		if (ImGui::IsItemHovered() || (!ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
		{
			ImGui::SetKeyboardFocusHere(-1);
		}
		
		auto searchStringEnd = std::find(m_searchBuffer.begin(), m_searchBuffer.end(), '\0');
		assert(searchStringEnd != m_searchBuffer.end());
		bool hasSearchString = m_searchBuffer[0] != '\0';
		bool noMatchesFound = true;
		bool isFirstItem = true;
		
		ImGui::Separator();
		
		ImGui::BeginChild("EntityTypes", ImVec2(0, 250));
		
		for (const auto& entityGroup : entityGroups)
		{
			if (!entityGroup.first.empty() && !hasSearchString && !ImGui::CollapsingHeader(entityGroup.first.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				continue;
			
			for (EntTypeID entityTypeId : entityGroup.second)
			{
				auto entityType = entTypeMap.find(entityTypeId);
				if (entityType == entTypeMap.end())
					continue;
				const std::string& entityName = entityType->second.prettyName;
				
				//Checks the entry against the search query
				if (hasSearchString)
				{
					auto it = std::search(entityName.begin(), entityName.end(), m_searchBuffer.begin(), searchStringEnd,
						[] (char a, char b) { return std::toupper(a) == std::toupper(b); });
					
					//If the search query wasn't found in the entry's label, skip this entry
					if (it == entityName.end())
						continue;
					noMatchesFound = false;
				}
				
				bool chooseOnEnter = hasSearchString && isFirstItem;
				if (ImGui::MenuItem(entityName.c_str(), chooseOnEnter ? "Enter" : "") || (chooseOnEnter && eg::IsButtonDown(eg::Button::Enter)))
				{
					std::shared_ptr<Ent> entity = entityType->second.create();
					entity->EditorMoved(m_spawnEntityPickResult.intersectPosition, m_spawnEntityPickResult.normalDir);
					editorState.world->entManager.AddEntity(std::move(entity));
					
					ImGui::CloseCurrentPopup();
				}
				
				isFirstItem = false;
			}
		}
		
		if (hasSearchString && noMatchesFound)
		{
			ImGui::Text("No Matches Found");
		}
		
		ImGui::EndChild();
		
		if (!ImGui::IsAnyItemHovered() && !eg::IsButtonDown(eg::Button::MouseLeft) && eg::WasButtonDown(eg::Button::MouseLeft))
			ImGui::CloseCurrentPopup();
		
		ImGui::EndPopup();
	}
}

bool SpawnEntityEditorComponent::UpdateInput(float dt, const EditorState& editorState)
{
	//Opens the spawn entity menu if the right mouse button is clicked
	if (eg::IsButtonDown(eg::Button::S) && !eg::WasButtonDown(eg::Button::S) && !eg::InputState::Current().IsCtrlDown())
	{
		m_spawnEntityPickResult = editorState.world->voxels.RayIntersect(editorState.viewRay);
		if (m_spawnEntityPickResult.intersected)
		{
			m_open = true;
			return true;
		}
	}
	return false;
}
