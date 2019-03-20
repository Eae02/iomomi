#pragma once

#include <yaml-cpp/yaml.h>

#include "../Dir.hpp"
#include "../Clipping.hpp"

class Entity
{
public:
	class IDrawable
	{
	public:
		virtual void Draw(eg::MeshBatch& meshBatch) = 0;
	};
	
	class IEditorWallDrag
	{
	public:
		virtual void EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir) = 0;
	};
	
	struct UpdateArgs
	{
		float dt;
		class World* world;
		class Player* player;
		std::function<void(const eg::Sphere&)> invalidateShadows;
	};
	
	class IUpdatable
	{
	public:
		virtual void Update(const UpdateArgs& args) = 0;
	};
	
	class ICollidable
	{
	public:
		virtual void CalcClipping(ClippingArgs& args) const = 0;
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
	
	struct EditorInteractArgs
	{
		const class World* world;
		eg::Ray viewRay;
	};
	
	const class EntityType* GetType() const
	{
		return m_type;
	}
	
	inline std::shared_ptr<Entity> Clone() const;
	
	virtual int GetEditorIconIndex() const;
	
	virtual void EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal);
	
	virtual void EditorRenderSettings();
	
	struct EditorDrawArgs
	{
		eg::SpriteBatch* spriteBatch;
		eg::MeshBatch* meshBatch;
		class PrimitiveRenderer* primitiveRenderer;
	};
	
	virtual void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const;
	
	virtual void Save(YAML::Emitter& emitter) const;
	virtual void Load(const YAML::Node& node);
	
private:
	friend class EntityType;
	
	glm::vec3 m_position;
	const class EntityType* m_type;
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
		type.m_createInstance = [] () -> std::shared_ptr<Entity>
		{
			return std::make_shared<T>();
		};
		type.m_clone = [] (const Entity& entity) -> std::shared_ptr<Entity>
		{
			return std::make_shared<T>(static_cast<const T&>(entity));
		};
		return type;
	}
	
	std::shared_ptr<Entity> CreateInstance() const
	{
		std::shared_ptr<Entity> instance = m_createInstance();
		instance->m_type = this;
		return instance;
	}
	
	std::shared_ptr<Entity> Clone(const Entity& entity) const
	{
		return m_clone(entity);
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
	std::shared_ptr<Entity> (*m_clone)(const Entity&);
};

inline std::shared_ptr<Entity> Entity::Clone() const
{
	return m_type->Clone(*this);
}

extern std::vector<EntityType> entityTypes;

void DefineEntityType(EntityType entityType);

const EntityType* FindEntityType(std::string_view name);
