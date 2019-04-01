#pragma once

using EntityFactory = eg::Entity* (*) (eg::EntityManager&);

struct SpawnableEntityType
{
	EntityFactory factory;
	std::string name;
};

extern std::vector<SpawnableEntityType> spawnableEntityTypes;

extern std::vector<const eg::IEntitySerializer*> entitySerializers;