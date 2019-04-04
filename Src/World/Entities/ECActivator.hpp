#pragma once

struct ActivateMessage : eg::Message<ActivateMessage> { };

class ECActivator
{
public:
	uint32_t activatableName = 0;
	int sourceIndex = 0;
	
	void HandleMessage(eg::Entity& entity, const ActivateMessage& message);
	
	static void Update(const struct WorldUpdateArgs& args);
	
	static eg::MessageReceiver MessageReceiver;
	
	static eg::EntitySignature Signature;
	
private:
	uint64_t m_lastActivatedFrame = 0;
	bool m_isActivated = false;
};
