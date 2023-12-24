#pragma once

#include <unordered_map>

#include "Entity.hpp"

class EntityManager
{
public:
	EntityManager();

	void AddEntity(std::shared_ptr<Ent> entity);

	void RemoveEntity(const Ent& entity) { m_entitiesToRemove.emplace_back(entity.TypeID(), entity.Name()); }

	template <typename T, typename CallbackTp>
	void ForEachOfType(CallbackTp callback);

	template <typename CallbackTp>
	void ForEachWithFlag(EntTypeFlags flags, CallbackTp callback);

	template <typename ComponentType, typename CallbackTp>
	void ForEachWithComponent(CallbackTp callback)
	{
		ForEachWithComponent<CallbackTp>(typeid(ComponentType), callback);
	}

	template <typename CallbackTp>
	void ForEachWithComponent(const std::type_info& componentType, CallbackTp callback);

	template <typename CallbackTp>
	void ForEach(CallbackTp callback);

	void Update(const struct WorldUpdateArgs& args);

	static EntityManager Deserialize(std::istream& stream);

	void Serialize(std::ostream& stream) const;

	bool isEditor = false;

private:
	std::array<std::unordered_map<uint32_t, std::shared_ptr<Ent>>, NUM_ENTITY_TYPES> m_entities;

	std::vector<std::pair<EntTypeID, uint32_t>> m_entitiesToRemove;

	struct FlagTracker
	{
		EntTypeFlags flags;
		std::vector<std::weak_ptr<Ent>> entities;

		void MaybeAdd(const std::shared_ptr<Ent>& entity);
	};

	struct ComponentTracker
	{
		const std::type_info* componentType;
		std::vector<std::weak_ptr<Ent>> entities;

		void MaybeAdd(const std::shared_ptr<Ent>& entity);
	};

	template <typename CallbackTp>
	static void ForEachInEntityVector(std::vector<std::weak_ptr<Ent>>& entities, CallbackTp callback);

	std::array<FlagTracker, 7> m_flagTrackers;
	std::array<ComponentTracker, 3> m_componentTrackers;
};

#include "EntityManager.inl"
