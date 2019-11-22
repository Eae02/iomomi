#include "EntityManager.hpp"

void EntityManager::AddEntity(std::shared_ptr<Ent> entity)
{
	for (FlagTracker& tracker : m_trackers)
		tracker.MaybeAdd(entity);
	
	for (size_t i = 0; i < NUM_UPDATABLE_ENTITY_TYPES; i++)
	{
		if (entUpdateOrder[i] == entity->TypeID())
		{
			m_updatableEntities[i].emplace_back(entity);
			break;
		}
	}
	
	uint32_t name = entity->Name();
	m_entities.emplace(name, std::move(entity));
}

EntityManager::EntityManager()
{
	m_trackers[0].flags = EntTypeFlags::Drawable;
	m_trackers[1].flags = EntTypeFlags::EditorDrawable;
	m_trackers[2].flags = EntTypeFlags::HasCollision;
	m_trackers[3].flags = EntTypeFlags::Interactable;
	m_trackers[4].flags = EntTypeFlags::Activatable;
}

void EntityManager::RemoveEntity(uint32_t name)
{
	m_entities.erase(name);
}

void EntityManager::Update(const struct WorldUpdateArgs& args)
{
	for (size_t t = 0; t < NUM_UPDATABLE_ENTITY_TYPES; t++)
	{
		for (int64_t i = m_updatableEntities[t].size() - 1; i >= 0; i--)
		{
			if (std::shared_ptr<Ent> ent = m_updatableEntities[t][i].lock())
			{
				ent->Update(args);
			}
			else
			{
				m_updatableEntities[t][i].swap(m_updatableEntities[t].back());
				m_updatableEntities[t].pop_back();
			}
		}
	}
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
	
	eg::BinWrite(stream, (uint32_t)m_entities.size());
	for (const auto& entity : m_entities)
	{
		const EntType& entType = entTypeMap.at(entity.second->TypeID());
		eg::Log(eg::LogLevel::Info, "ecs", "Serializing {0}", (uint32_t)eg::HashFNV1a32(entType.name));
		eg::BinWrite(stream, (uint32_t)eg::HashFNV1a32(entType.name));
		
		std::ostringstream serializeStream;
		entity.second->Serialize(serializeStream);
		
		std::string serializedData = serializeStream.str();
		eg::BinWrite<uint32_t>(stream, serializedData.size());
		stream.write(serializedData.data(), serializedData.size());
	}
}
