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
