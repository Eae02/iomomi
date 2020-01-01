#include "ActivatorComp.hpp"
#include "ActivatableComp.hpp"
#include "../../World.hpp"
#include "../../../ProtobufUtils.hpp"

constexpr uint64_t ACTIVATED_FRAMES = 5;

void ActivatorComp::Activate()
{
	m_lastActivatedFrame = eg::FrameIdx();
}

void ActivatorComp::Update(const WorldUpdateArgs& args)
{
	if (args.player == nullptr)
		return;
	
	bool activated = m_lastActivatedFrame + ACTIVATED_FRAMES > eg::FrameIdx();
	if (activated != m_isActivated)
	{
		m_isActivated = activated;
		Ent* activatableEntity = ActivatableComp::FindByName(args.world->entManager, activatableName);
		if (activatableEntity != nullptr)
		{
			activatableEntity->GetComponentMut<ActivatableComp>()->SetActivated(targetConnectionIndex, activated);
		}
	}
}

void ActivatorComp::Initialize(EntityManager& entityManager)
{
	entityManager.ForEach([&] (Ent& entity)
	{
		ActivatorComp* activator = entity.GetComponentMut<ActivatorComp>();
		if (activator == nullptr)
			return;
		
		Ent* activatableEntity = ActivatableComp::FindByName(entityManager, activator->activatableName);
		if (activatableEntity != nullptr)
		{
			activatableEntity->GetComponentMut<ActivatableComp>()->SetConnected(activator->targetConnectionIndex);
		}
	});
}

void ActivatorComp::LoadProtobuf(const gravity_pb::Activator& activator)
{
	targetConnectionIndex = activator.target_connection_index();
	activatableName = activator.target_name();
	
	waypoints.clear();
	waypoints.reserve(activator.way_points_size());
	for (const gravity_pb::ActWayPoint& wp : activator.way_points())
	{
		waypoints.push_back({ (Dir)wp.wall_normal(), PBToGLM(wp.position()) });
	}
}

gravity_pb::Activator* ActivatorComp::SaveProtobuf(google::protobuf::Arena* arena) const
{
	gravity_pb::Activator* activatorPB = google::protobuf::Arena::CreateMessage<gravity_pb::Activator>(arena);
	
	activatorPB->set_target_connection_index(targetConnectionIndex);
	activatorPB->set_target_name(activatableName);
	for (const ActivationLightStripEnt::WayPoint& wp : waypoints)
	{
		gravity_pb::ActWayPoint* wpPb = activatorPB->add_way_points();
		wpPb->set_allocated_position(GLMToPB(wp.position, arena));
		wpPb->set_wall_normal((gravity_pb::Dir)wp.wallNormal);
	}
	
	return activatorPB;
}