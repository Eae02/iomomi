#pragma once

#include "../../../../Protobuf/Build/Activator.pb.h"
#include "../EntTypes/Activation/ActivationLightStripEnt.hpp"

class ActivatorComp
{
public:
	uint32_t activatableName = 0;
	int targetConnectionIndex = 0;

	std::weak_ptr<ActivationLightStripEnt> lightStripEntity;

	void Activate();

	void Update(const struct WorldUpdateArgs& args);

	static void Initialize(class EntityManager& entityManager);

	void LoadProtobuf(const iomomi_pb::Activator& activator);
	iomomi_pb::Activator* SaveProtobuf(google::protobuf::Arena* arena) const;

	bool IsActivated() const { return m_isActivated; }

	std::vector<ActivationLightStripEnt::WayPoint> waypoints;

private:
	std::optional<uint64_t> m_lastActivatedFrame;
	bool m_isActivated = false;
};
