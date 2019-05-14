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
	GravityGun();
	
	void Update(class World& world, const class Player& player, const glm::mat4& inverseViewProj, float dt);
	
	void Draw(eg::MeshBatch& meshBatch);
	
private:
	eg::EntityHandle m_gravityChargedEntity;
	
	glm::vec3 m_gunOffset;
	glm::mat4 m_gunTransform;
	
	float m_bobTime = 0;
	
	const eg::Model* m_model;
	std::array<const eg::IMaterial*, 4> m_materials;
};
