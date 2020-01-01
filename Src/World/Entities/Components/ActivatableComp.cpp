#include "ActivatableComp.hpp"
#include "../EntityManager.hpp"

#include <ctime>

void ActivatableComp::SetActivated(int connection, bool activated)
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

bool ActivatableComp::AllSourcesActive() const
{
	return (m_activations & m_enabledConnections) == m_enabledConnections;
}

static std::mt19937 nameGen { (uint32_t)std::time(nullptr) };

static std::vector<glm::vec3> GetConnectionPointsDefault(const Ent& entity)
{
	return { entity.Pos() };
}

ActivatableComp::ActivatableComp(GetConnectionPointsCallback getConnectionPoints)
	: m_name(std::max<uint32_t>(nameGen(), 1))
{
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
	entityManager.ForEachWithFlag(EntTypeFlags::Activatable, [&] (Ent& entity)
	{
		const ActivatableComp* activatable = entity.GetComponent<ActivatableComp>();
		if (activatable && activatable->m_name == name)
			result = &entity;
	});
	return result;
}