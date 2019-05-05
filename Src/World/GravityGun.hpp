#pragma once

#include "Dir.hpp"

struct GravityChargeSetMessage : eg::Message<GravityChargeSetMessage>
{
	Dir newDown;
	bool* set;
};

struct GravityChargeResetMessage : eg::Message<GravityChargeResetMessage>
{
	
};

class GravityGun
{
public:
	
	void Update(class World& world, const class Player& player, const glm::mat4& inverseViewProj);
	
private:
	eg::EntityHandle m_gravityChargedEntity;
};
