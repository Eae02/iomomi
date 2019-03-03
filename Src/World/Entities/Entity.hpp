#pragma once

#include <yaml-cpp/yaml.h>
#include "../Dir.hpp"

class Entity
{
public:
	class IDrawable
	{
	public:
		virtual void Draw(class ObjectRenderer& renderer) = 0;
	};
	
	class IEditorWallDrag
	{
	public:
		virtual void EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir) = 0;
	};
	
	virtual ~Entity() = default;
	
	const glm::vec3& Position() const
	{
		return m_position;
	}
	
	void SetPosition(const glm::vec3& position)
	{
		m_position = position;
	}
	
	std::string_view TypeName() const
	{
		return m_typeName;
	}
	
	struct EditorInteractArgs
	{
		const class World* world;
		eg::Ray viewRay;
	};
	
	virtual int GetEditorIconIndex() const;
	
	virtual void EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal);
	
	virtual bool EditorInteract(const EditorInteractArgs& args);
	
	virtual void EditorRenderSettings();
	
	struct EditorDrawArgs
	{
		eg::SpriteBatch* spriteBatch;
		class PrimitiveRenderer* primitiveRenderer;
		class ObjectRenderer* objectRenderer;
	};
	
	virtual void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const;
	
	virtual void Save(YAML::Emitter& emitter) const;
	virtual void Load(const YAML::Node& node);
	
private:
	friend class EntityType;
	
	glm::vec3 m_position;
	std::string_view m_typeName;
};

class EntityType
{
public:
	template <typename T>
	static EntityType Make(std::string name, std::string displayName)
	{
		EntityType type;
		type.m_name = std::move(name); 
		type.m_displayName = std::move(displayName);
		type.m_createInstance = [] () -> std::shared_ptr<Entity> { return std::make_shared<T>(); };
		return type;
	}
	
	std::shared_ptr<Entity> CreateInstance() const
	{
		std::shared_ptr<Entity> instance = m_createInstance();
		instance->m_typeName = m_name;
		return instance;
	}
	
	const std::string& Name() const
	{
		return m_name;
	}
	
	const std::string& DisplayName() const
	{
		return m_displayName;
	}
	
private:
	std::string m_name;
	std::string m_displayName;
	std::shared_ptr<Entity> (*m_createInstance)();
};

extern std::vector<EntityType> entityTypes;

void DefineEntityType(EntityType entityType);

const EntityType* FindEntityType(std::string_view name);
