#pragma once

#include <random>

class ECActivatable
{
public:
	ECActivatable();
	
	uint32_t Name() const
	{
		return m_name;
	}
	
	void SetName(uint32_t name)
	{
		m_name = name;
	}
	
	uint32_t EnabledSources() const
	{
		return m_enabledSources;
	}
	
	void SetEnabledSources(uint32_t enabledSources)
	{
		m_enabledSources = enabledSources;
	}
	
	int AddSource();
	
	void RemoveSource(int source);
	
	bool AllSourcesActive() const;
	
	void SetActivated(int source, bool activated);
	
	static eg::EntitySignature Signature;
	
private:
	uint32_t m_name;
	uint32_t m_enabledSources = 0;
	uint32_t m_activations = 0;
};
