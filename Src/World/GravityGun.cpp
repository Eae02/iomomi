#include "GravityGun.hpp"
#include "World.hpp"
#include "Player.hpp"

void GravityGun::Update(World& world, const Player& player, const glm::mat4& inverseViewProj)
{
	if (eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
	{
		eg::Ray viewRay = eg::Ray::UnprojectNDC(inverseViewProj, glm::vec2(0.0f));
		RayIntersectResult intersectResult = world.RayIntersect(viewRay);
		
		if (intersectResult.intersected && intersectResult.entity != nullptr)
		{
			eg::Entity* prevChargedEntity = m_gravityChargedEntity.Get();
			bool set = false;
			
			GravityChargeSetMessage chargeSetMessage;
			chargeSetMessage.newDown = player.CurrentDown();
			chargeSetMessage.set = &set;
			
			intersectResult.entity->HandleMessage(chargeSetMessage);
			
			if (set)
			{
				if (prevChargedEntity != nullptr && intersectResult.entity != prevChargedEntity)
				{
					GravityChargeResetMessage chargeResetMessage;
					prevChargedEntity->HandleMessage(chargeResetMessage);
				}
				m_gravityChargedEntity = *intersectResult.entity;
			}
		}
	}
}
