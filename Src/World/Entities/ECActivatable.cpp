#include "ECActivatable.hpp"

#include <ctime>

eg::EntitySignature ECActivatable::Signature = eg::EntitySignature::Create<ECActivatable>();

void ECActivatable::SetActivated(int source, bool activated)
{
	if (activated)
		m_activations |= 1U << source;
	else
		m_activations &= ~(1U << source);
	
	if (AllSourcesActive())
	{
		//Send message or something...
	}
}

bool ECActivatable::AllSourcesActive() const
{
	return (m_activations & m_enabledSources) == m_enabledSources;
}

static std::mt19937 nameGen { (uint32_t)std::time(nullptr) };

ECActivatable::ECActivatable()
	: m_name(std::max<uint32_t>(nameGen(), 1)) { }

void ECActivatable::RemoveSource(int source)
{
	m_enabledSources &= ~(1U << source);
}

int ECActivatable::AddSource()
{
	for (int i = 0; i < 32; i++)
	{
		uint32_t mask = 1U << i;
		if ((m_enabledSources & mask) == 0)
		{
			m_enabledSources |= mask;
			return i;
		}
	}
	return -1;
}
