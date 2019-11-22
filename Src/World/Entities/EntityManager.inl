template <typename T, typename CallbackTp>
void EntityManager::ForEachOfType(CallbackTp callback)
{
	for (const auto& entity : m_entities)
	{
		if (entity.second->TypeID() == T::TypeID)
			callback(static_cast<T&>(*entity.second));
	}
}

template <typename CallbackTp>
void EntityManager::ForEachWithFlag(EntTypeFlags flags, CallbackTp callback)
{
	for (FlagTracker& tracker : m_trackers)
	{
		if (tracker.flags == flags)
		{
			tracker.ForEach(callback);
			return;
		}
	}
	eg::Log(eg::LogLevel::Error, "ent", "No entity tracker set up for the flag set {0}", (int)flags);
}

template <typename CallbackTp>
void EntityManager::ForEach(CallbackTp callback)
{
	for (const auto& entity : m_entities)
		callback(*entity.second);
}

template <typename CallbackTp>
void EntityManager::FlagTracker::ForEach(CallbackTp callback)
{
	for (int64_t i = entities.size() - 1; i >= 0; i--)
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
