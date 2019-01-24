#pragma once

#include "../World/World.hpp"
#include "../GameState.hpp"

class Editor : public GameState
{
public:
	Editor();
	
	void RunFrame(float dt) override;
	
private:
	std::string m_newLevelName;
	std::unique_ptr<World> m_world;
};

extern std::unique_ptr<Editor> editor;
