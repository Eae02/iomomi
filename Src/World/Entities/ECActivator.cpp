#include "ECActivator.hpp"
#include "ECActivatable.hpp"
#include "../WorldUpdateArgs.hpp"
#include "../World.hpp"

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
			for (eg::Entity& activatableEntity : entity.Manager()->GetEntitySet(ECActivatable::Signature))
			{
				ECActivatable& activatable = activatableEntity.GetComponent<ECActivatable>();
				if (activatable.Name() == activator.activatableName)
				{
					activatable.SetActivated(activator.sourceIndex, activated);
					break;
				}
			}
		}
	}
}
