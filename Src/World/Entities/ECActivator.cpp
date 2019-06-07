#include "ECActivator.hpp"
#include "ECActivatable.hpp"
#include "../WorldUpdateArgs.hpp"
#include "../World.hpp"
#include "../../ProtobufUtils.hpp"

eg::MessageReceiver ECActivator::MessageReceiver = eg::MessageReceiver::Create<ECActivator, ActivateMessage>();

eg::EntitySignature ECActivator::Signature = eg::EntitySignature::Create<ECActivator>();

void ECActivator::HandleMessage(eg::Entity& entity, const ActivateMessage& message)
{
	m_lastActivatedFrame = eg::FrameIdx();
}

constexpr uint64_t ACTIVATED_FRAMES = 5;

void ECActivator::Update(const WorldUpdateArgs& args)
{
	if (args.player == nullptr)
		return;
	
	for (eg::Entity& entity : args.world->EntityManager().GetEntitySet(Signature))
	{
		ECActivator& activator = entity.GetComponent<ECActivator>();
		bool activated = activator.m_lastActivatedFrame + ACTIVATED_FRAMES > eg::FrameIdx();
		if (activated != activator.m_isActivated)
		{
			activator.m_isActivated = activated;
			eg::Entity* activatableEntity = ECActivatable::FindByName(*entity.Manager(), activator.activatableName);
			if (activatableEntity != nullptr)
			{
				activatableEntity->GetComponent<ECActivatable>().SetActivated(activator.targetConnectionIndex, activated);
				break;
			}
		}
	}
}

void ECActivator::Initialize(eg::EntityManager& entityManager)
{
	for (eg::Entity& entity : entityManager.GetEntitySet(Signature))
	{
		ECActivator& activator = entity.GetComponent<ECActivator>();
		eg::Entity* activatableEntity = ECActivatable::FindByName(entityManager, activator.activatableName);
		if (activatableEntity != nullptr)
		{
			activatableEntity->GetComponent<ECActivatable>().SetConnected(activator.targetConnectionIndex);
		}
	}
}

void ECActivator::LoadProtobuf(const gravity_pb::Activator& activator)
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

gravity_pb::Activator* ECActivator::SaveProtobuf(google::protobuf::Arena* arena) const
{
	gravity_pb::Activator* activatorPB = google::protobuf::Arena::CreateMessage<gravity_pb::Activator>(arena);
	
	activatorPB->set_target_connection_index(targetConnectionIndex);
	activatorPB->set_target_name(activatableName);
	for (const ECActivationLightStrip::WayPoint& wp : waypoints)
	{
		gravity_pb::ActWayPoint* wpPb = activatorPB->add_way_points();
		wpPb->set_allocated_position(GLMToPB(wp.position, arena));
		wpPb->set_wall_normal((gravity_pb::Dir)wp.wallNormal);
	}
	
	return activatorPB;
}
