#pragma once

#include <random>

class ECActivatable
{
public:
	using GetConnectionPointsCallback = std::vector<glm::vec3>(*)(const eg::Entity&);
	
	explicit ECActivatable(GetConnectionPointsCallback getConnectionPoints = nullptr);
	
	uint32_t Name() const
	{
		return m_name;
	}
	
	void SetName(uint32_t name)
	{
		m_name = name;
	}
	
	uint32_t EnabledConnections() const
	{
		return m_enabledConnections;
	}
	
	void SetEnabledConnections(uint32_t enabledConnections)
	{
		m_enabledConnections = enabledConnections;
	}
	
	void SetConnected(int connection);
	
	void SetDisconnected(int connection);
	
	bool AllSourcesActive() const;
	
	void SetActivated(int connection, bool activated);
	
	static std::vector<glm::vec3> GetConnectionPoints(const eg::Entity& entity);
	
	static eg::Entity* FindByName(eg::EntityManager& entityManager, uint32_t name);
	
	static eg::EntitySignature Signature;
	
private:
	uint32_t m_name;
	uint32_t m_enabledConnections = 0;
	uint32_t m_activations = 0;
	GetConnectionPointsCallback m_getConnectionPoints;
};
