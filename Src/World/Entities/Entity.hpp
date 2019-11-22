#pragma once

#include "EntityTypes.hpp"
#include "../Dir.hpp"
#include "../../Graphics/Lighting/PointLight.hpp"

struct EntDrawArgs
{
	eg::MeshBatch* meshBatch;
	eg::MeshBatchOrdered* transparentMeshBatch;
	std::vector<struct ReflectionPlane*>* reflectionPlanes;
	std::vector<PointLightDrawData>* pointLights;
	const class World* world;
};

enum class EntEditorDrawMode
{
	Default,
	Selected,
	Targeted
};

struct EntEditorDrawArgs
{
	eg::SpriteBatch* spriteBatch;
	eg::MeshBatch* meshBatch;
	eg::MeshBatchOrdered* transparentMeshBatch;
	class PrimitiveRenderer* primitiveRenderer;
	std::function<EntEditorDrawMode(const class Ent*)> getDrawMode;
};

enum class EntTypeFlags
{
	EditorInvisible = 0x1,
	EditorWallMove  = 0x2,
	Drawable        = 0x4,
	EditorDrawable  = 0x8,
	HasCollision    = 0x10,
	Interactable    = 0x20,
	Activatable     = 0x40
};

class Ent : public std::enable_shared_from_this<Ent>
{
public:
	Ent();
	
	virtual void Serialize(std::ostream& stream) = 0;
	virtual void Deserialize(std::istream& stream) = 0;
	
	virtual void RenderSettings();
	
	virtual void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection);
	
	virtual void Draw(const EntDrawArgs& args);
	virtual void EditorDraw(const EntEditorDrawArgs& args);
	
	virtual void Update(const struct WorldUpdateArgs& args);
	
	glm::mat4 GetTransform(float scale) const;
	glm::mat3 GetRotationMatrix() const;
	eg::AABB GetAABB(float scale, float upDist) const;
	
	glm::vec3 Pos() const { return m_position; }
	Dir Direction() const { return m_direction; }
	
	uint32_t Name() const { return m_name; }
	EntTypeID TypeID() const { return m_typeID; }
	EntTypeFlags TypeFlags() const;
	
	virtual const void* GetComponent(const std::type_info& type) const;
	
	//Utility functions for getting components
	template <typename T>
	const T* GetComponent() const { return static_cast<const T*>(GetComponent(typeid(T))); }
	template <typename T>
	T* GetComponentMut() { return static_cast<T*>(GetComponentMut(typeid(T))); }
	void* GetComponentMut(const std::type_info& type)
	{
		return const_cast<void*>(GetComponent(type));
	}
	
	template <typename T>
	static std::shared_ptr<T> Create()
	{
		std::shared_ptr<T> ent = std::make_shared<T>();
		ent->m_typeID = T::TypeID;
		return ent;
	}
	
	template <typename T>
	static std::shared_ptr<Ent> CreateCallback()
	{
		return Create<T>();
	}
	
	void PreventSerialize()
	{
		m_shouldSerialize = false;
	}
	
	bool ShouldSerialize() const
	{
		return m_shouldSerialize;
	}
	
protected:
	Dir m_direction = Dir::PosX;
	glm::vec3 m_position;
	
	template <typename ST>
	inline void SerializePos(ST& st) const
	{
		st.set_posx(m_position.x);
		st.set_posy(m_position.y);
		st.set_posz(m_position.z);
	}
	
	template <typename ST>
	inline void DeserializePos(const ST& st)
	{
		m_position = glm::vec3(st.posx(), st.posy(), st.posz());
	}
	
private:
	uint32_t m_name;
	EntTypeID m_typeID;
	bool m_shouldSerialize = true;
};

EG_BIT_FIELD(EntTypeFlags)

struct EntType
{
	EntTypeFlags flags;
	std::string name;
	std::string prettyName;
	std::shared_ptr<Ent> (*create)();
};

extern std::unordered_map<EntTypeID, EntType> entTypeMap;
