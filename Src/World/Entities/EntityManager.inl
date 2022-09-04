template <typename CallbackTp>
void EntityManager::ForEachInEntityVector(std::vector<std::weak_ptr<Ent>>& entities, CallbackTp callback)
{
	for (int64_t i = eg::ToInt64(entities.size()) - 1; i >= 0; i--)
	{
		if (std::shared_ptr<Ent> ent = entities[i].lock())
		{
			callback(*ent);
		}
		else
		{
			entities[i].swap(entities.back());
			entities.pop_back();
		}
	}
}

template <typename T, typename CallbackTp>
void EntityManager::ForEachOfType(CallbackTp callback)
{
	for (const auto& entity : m_entities[static_cast<int>(T::TypeID)])
	{
		callback(static_cast<T&>(*entity.second));
	}
}

template <typename CallbackTp>
void EntityManager::ForEachWithFlag(EntTypeFlags flags, CallbackTp callback)
{
	for (FlagTracker& tracker : m_flagTrackers)
	{
		if (tracker.flags == flags)
		{
			ForEachInEntityVector(tracker.entities, callback);
			return;
		}
	}
	eg::Log(eg::LogLevel::Error, "ent", "No entity tracker set up for the flag set {0}", static_cast<int>(flags));
}

template <typename CallbackTp>
void EntityManager::ForEachWithComponent(const std::type_info& componentType, CallbackTp callback)
{
	for (ComponentTracker& tracker : m_componentTrackers)
	{
		if (*tracker.componentType == componentType)
		{
			ForEachInEntityVector(tracker.entities, callback);
			return;
		}
	}
	eg::Log(eg::LogLevel::Error, "ent", "No entity tracker set up for the component {0}", componentType.name());
}

template <typename CallbackTp>
void EntityManager::ForEach(CallbackTp callback)
{
	for (const auto& entityList : m_entities)
		for (const auto& entity : entityList)
			callback(*entity.second);
}
