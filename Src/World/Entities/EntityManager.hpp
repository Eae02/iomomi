#pragma once

#include "Entity.hpp"

#include <unordered_map>

class EntityManager
{
public:
	EntityManager();
	
	void AddEntity(std::shared_ptr<Ent> entity);
	
	void RemoveEntity(uint32_t name);
	
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
	std::unordered_map<uint32_t, std::shared_ptr<Ent>> m_entities;
	
	std::vector<std::weak_ptr<Ent>> m_updatableEntities[NUM_UPDATABLE_ENTITY_TYPES];
	
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
