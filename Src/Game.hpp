#pragma once

#include <EGame/EG.hpp>

#include "ImGuiInterface.hpp"

class Game : public eg::IGame
{
public:
	void Update() override;
	
	void Draw() override;
	
private:
	ImGuiInterface m_imGuiInterface;
};
