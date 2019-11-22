#pragma once

#include "../../../../Protobuf/Build/Activator.pb.h"
#include "../EntTypes/ActivationLightStripEnt.hpp"

class ActivatorComp
{
public:
	uint32_t activatableName = 0;
	int targetConnectionIndex = 0;
	
	std::weak_ptr<ActivationLightStripEnt> lightStripEntity;
	
	//void HandleMessage(eg::Entity& entity, const ActivateMessage& message);
	
	void Update(const struct WorldUpdateArgs& args);
	
	static void Initialize(class EntityManager& entityManager);
	
	void LoadProtobuf(const gravity_pb::Activator& activator);
	gravity_pb::Activator* SaveProtobuf(google::protobuf::Arena* arena) const;
	
	bool IsActivated() const
	{
		return m_isActivated;
	}
	
	std::vector<ActivationLightStripEnt::WayPoint> waypoints;
	
private:
	uint64_t m_lastActivatedFrame = 0;
	bool m_isActivated = false;
};
