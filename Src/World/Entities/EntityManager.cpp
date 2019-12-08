#include "EntityManager.hpp"

void EntityManager::AddEntity(std::shared_ptr<Ent> entity)
{
	for (FlagTracker& tracker : m_trackers)
		tracker.MaybeAdd(entity);
	
	entity->Spawned(isEditor);
	
	uint32_t name = entity->Name();
	m_entities[(int)entity->TypeID()].emplace(name, std::move(entity));
}

EntityManager::EntityManager()
{
	m_trackers[0].flags = EntTypeFlags::Drawable;
	m_trackers[1].flags = EntTypeFlags::EditorDrawable;
	m_trackers[2].flags = EntTypeFlags::Interactable;
	m_trackers[3].flags = EntTypeFlags::Activatable;
}

void EntityManager::Update(const struct WorldUpdateArgs& args)
{
	for (size_t t = 0; t < NUM_UPDATABLE_ENTITY_TYPES; t++)
	{
		for (auto& entity : m_entities[(int)entUpdateOrder[t]])
		{
			entity.second->Update(args);
		}
	}
	
	for (std::pair<EntTypeID, uint32_t> entityToRemove : m_entitiesToRemove)
	{
		m_entities[(int)entityToRemove.first].erase(entityToRemove.second);
	}
	m_entitiesToRemove.clear();
}

void EntityManager::FlagTracker::MaybeAdd(const std::shared_ptr<Ent>& entity)
{
	if ((int)entity->TypeFlags() & (int)flags)
	{
		entities.emplace_back(entity);
	}
}

EntityManager EntityManager::Deserialize(std::istream& stream)
{
	std::unordered_map<uint32_t, const EntType*> serializerMap;
	for (const auto& entType : entTypeMap)
	{
		serializerMap.emplace(eg::HashFNV1a32(entType.second.name), &entType.second);
	}
	
	EntityManager mgr;
	
	std::vector<char> readBuffer;
	auto numEntities = eg::BinRead<uint32_t>(stream);
	for (uint32_t i = 0; i < numEntities; i++)
	{
		auto serializerHash = eg::BinRead<uint32_t>(stream);
		auto bytes = eg::BinRead<uint32_t>(stream);
		readBuffer.resize(bytes);
		stream.read(readBuffer.data(), bytes);
		
		auto it = serializerMap.find(serializerHash);
		if (it == serializerMap.end())
		{
			eg::Log(eg::LogLevel::Error, "ecs", "Failed to find entity serializer with hash {0}", serializerHash);
			continue;
		}
		
		eg::MemoryStreambuf streambuf(readBuffer);
		std::istream deserializeStream(&streambuf);
		
		std::shared_ptr<Ent> ent = it->second->create();
		ent->Deserialize(deserializeStream);
		mgr.AddEntity(std::move(ent));
	}
	
	return mgr;
}

void EntityManager::Serialize(std::ostream& stream) const
{
	std::vector<char> writeBuffer;
	
	uint32_t totalEntities = 0;
	for (const auto& entityList : m_entities)
		totalEntities += entityList.size();
	
	eg::BinWrite(stream, totalEntities);
	const_cast<EntityManager*>(this)->ForEach([&] (const Ent& entity)
	{
		const EntType& entType = entTypeMap.at(entity.TypeID());
		eg::BinWrite(stream, (uint32_t)eg::HashFNV1a32(entType.name));
		
		std::ostringstream serializeStream;
		entity.Serialize(serializeStream);
		
		std::string serializedData = serializeStream.str();
		eg::BinWrite<uint32_t>(stream, serializedData.size());
		stream.write(serializedData.data(), serializedData.size());
	});
}
