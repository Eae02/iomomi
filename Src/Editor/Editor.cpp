#include "Editor.hpp"

#include <filesystem>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

std::unique_ptr<Editor> editor;

Editor::Editor()
{
	
}

void Editor::RunFrame(float dt)
{
	if (m_world == nullptr)
	{
		ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_Always);
		if (ImGui::Begin("Level Editor", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
		{
			ImGui::Text("New Level");
			ImGui::InputText("Name", &m_newLevelName);
			if (ImGui::Button("Create"))
			{
				
			}
			
			ImGui::Separator();
			
			
		}
		
		ImGui::End();
		return;
	}
}
