#include "ActivatableComp.hpp"

#include <ctime>

#include "../../../Game.hpp"
#include "../EntityManager.hpp"

void ActivatableComp::SetActivated(int connection, bool activated)
{
	if (activated)
		m_activations |= 1U << connection;
	else
		m_activations &= ~(1U << connection);

	if (AllSourcesActive())
	{
		// Send message or something...
	}
}

bool ActivatableComp::AllSourcesActive() const
{
	return (m_activations & m_enabledConnections) == m_enabledConnections;
}

static std::vector<glm::vec3> GetConnectionPointsDefault(const Ent& entity)
{
	return { entity.GetPosition() };
}

ActivatableComp::ActivatableComp(GetConnectionPointsCallback getConnectionPoints)
{
	GiveNewName();
	if (getConnectionPoints != nullptr)
		m_getConnectionPoints = getConnectionPoints;
	else
		m_getConnectionPoints = &GetConnectionPointsDefault;
}

void ActivatableComp::SetDisconnected(int connection)
{
	m_enabledConnections &= ~(1U << connection);
}

void ActivatableComp::SetConnected(int connection)
{
	m_enabledConnections |= 1U << connection;
}

std::vector<glm::vec3> ActivatableComp::GetConnectionPoints(const Ent& entity) const
{
	return m_getConnectionPoints(entity);
}

Ent* ActivatableComp::FindByName(EntityManager& entityManager, uint32_t name)
{
	Ent* result = nullptr;
	entityManager.ForEachWithComponent<ActivatableComp>(
		[&](Ent& entity)
		{
			const auto* activatable = entity.GetComponent<ActivatableComp>();
			if (activatable && activatable->m_name == name)
				result = &entity;
		}
	);
	return result;
}

void ActivatableComp::GiveNewName()
{
	m_name = std::uniform_int_distribution<uint32_t>(1)(globalRNG);
}
