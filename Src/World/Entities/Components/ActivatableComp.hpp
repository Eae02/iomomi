#pragma once

#include "../Entity.hpp"

class ActivatableComp
{
public:
	using GetConnectionPointsCallback = std::vector<glm::vec3> (*)(const Ent&);

	explicit ActivatableComp(GetConnectionPointsCallback getConnectionPoints = nullptr);

	uint32_t m_name;
	uint32_t m_enabledConnections = 0;

	void SetConnected(int connection);

	void SetDisconnected(int connection);

	bool AllSourcesActive() const;

	void SetActivated(int connection, bool activated);

	std::vector<glm::vec3> GetConnectionPoints(const Ent& entity) const;

	void GiveNewName();

	static Ent* FindByName(class EntityManager& entityManager, uint32_t name);

private:
	uint32_t m_activations = 0;
	GetConnectionPointsCallback m_getConnectionPoints;
};
