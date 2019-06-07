#pragma once

#include "ECActivationLightStrip.hpp"
#include "../../../Protobuf/Build/Activator.pb.h"

struct ActivateMessage : eg::Message<ActivateMessage> { };

class ECActivator
{
public:
	uint32_t activatableName = 0;
	int targetConnectionIndex = 0;
	
	void HandleMessage(eg::Entity& entity, const ActivateMessage& message);
	
	static void Update(const struct WorldUpdateArgs& args);
	
	static void Initialize(eg::EntityManager& entityManager);
	
	void LoadProtobuf(const gravity_pb::Activator& activator);
	gravity_pb::Activator* SaveProtobuf(google::protobuf::Arena* arena) const;
	
	bool IsActivated() const
	{
		return m_isActivated;
	}
	
	static eg::MessageReceiver MessageReceiver;
	
	static eg::EntitySignature Signature;
	
	std::vector<ECActivationLightStrip::WayPoint> waypoints;
	
private:
	uint64_t m_lastActivatedFrame = 0;
	bool m_isActivated = false;
};
