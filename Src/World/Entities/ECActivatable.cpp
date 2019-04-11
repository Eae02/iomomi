#include "ECActivatable.hpp"

#include <ctime>

eg::EntitySignature ECActivatable::Signature = eg::EntitySignature::Create<ECActivatable>();

void ECActivatable::SetActivated(int connection, bool activated)
{
	if (activated)
		m_activations |= 1U << connection;
	else
		m_activations &= ~(1U << connection);
	
	if (AllSourcesActive())
	{
		//Send message or something...
	}
}

bool ECActivatable::AllSourcesActive() const
{
	return (m_activations & m_enabledConnections) == m_enabledConnections;
}

static std::mt19937 nameGen { (uint32_t)std::time(nullptr) };

static std::vector<glm::vec3> GetConnectionPointsDefault(const eg::Entity& entity)
{
	return { eg::GetEntityPosition(entity) };
}

ECActivatable::ECActivatable(GetConnectionPointsCallback getConnectionPoints)
	: m_name(std::max<uint32_t>(nameGen(), 1))
{
	if (getConnectionPoints != nullptr)
		m_getConnectionPoints = getConnectionPoints;
	else
		m_getConnectionPoints = &GetConnectionPointsDefault;
}

void ECActivatable::SetDisconnected(int connection)
{
	m_enabledConnections &= ~(1U << connection);
}

void ECActivatable::SetConnected(int connection)
{
	m_enabledConnections |= 1U << connection;
}

eg::Entity* ECActivatable::FindByName(eg::EntityManager& entityManager, uint32_t name)
{
	for (eg::Entity& activatableEntity : entityManager.GetEntitySet(ECActivatable::Signature))
	{
		ECActivatable& activatable = activatableEntity.GetComponent<ECActivatable>();
		if (activatable.Name() == name)
			return &activatableEntity;
	}
	return nullptr;
}

std::vector<glm::vec3> ECActivatable::GetConnectionPoints(const eg::Entity& entity)
{
	return entity.GetComponent<ECActivatable>().m_getConnectionPoints(entity);
}
