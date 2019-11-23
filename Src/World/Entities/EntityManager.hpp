#pragma once

#include "Entity.hpp"

#include <unordered_map>
#include <unordered_set>

class EntityManager
{
public:
	EntityManager();
	
	void AddEntity(std::shared_ptr<Ent> entity);
	
	void RemoveEntity(const Ent& entity)
	{
		m_entitiesToRemove.emplace_back(entity.TypeID(), entity.Name());
	}
	
	template <typename T, typename CallbackTp>
	void ForEachOfType(CallbackTp callback);
	
	template <typename CallbackTp>
	void ForEachWithFlag(EntTypeFlags flags, CallbackTp callback);
	
	template <typename CallbackTp>
	void ForEach(CallbackTp callback);
	
	void Update(const struct WorldUpdateArgs& args);
	
	static EntityManager Deserialize(std::istream& stream);
	
	void Serialize(std::ostream& stream) const;
	
private:
	std::unordered_map<uint32_t, std::shared_ptr<Ent>> m_entities[(size_t)EntTypeID::MAX];
	
	std::vector<std::pair<EntTypeID, uint32_t>> m_entitiesToRemove;
	
	struct FlagTracker
	{
		EntTypeFlags flags;
		std::vector<std::weak_ptr<Ent>> entities;
		
		void MaybeAdd(const std::shared_ptr<Ent>& entity);
		
		template <typename CallbackTp>
		void ForEach(CallbackTp callback);
	};
	
	std::array<FlagTracker, 5> m_trackers;
};

#include "EntityManager.inl"
