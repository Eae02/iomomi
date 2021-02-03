#pragma once

#include "EntityTypes.hpp"
#include "../Dir.hpp"
#include "../../Graphics/Lighting/PointLight.hpp"

struct EntDrawArgs
{
	eg::MeshBatch* meshBatch;
	eg::MeshBatchOrdered* transparentMeshBatch; //maybe nullptr!
	class World* world;
	const eg::Frustum* frustum;
	const PointLightShadowDrawArgs* shadowDrawArgs; //maybe nullptr!
};

struct EntGameDrawArgs : EntDrawArgs { };

enum class EntEditorDrawMode
{
	Default,
	Selected,
	Targeted
};

struct EntEditorDrawArgs : EntDrawArgs
{
	eg::SpriteBatch* spriteBatch;
	class PrimitiveRenderer* primitiveRenderer;
	std::function<EntEditorDrawMode(const class Ent*)> getDrawMode;
};

enum class EntTypeFlags
{
	EditorInvisible    = 0x1,
	EditorWallMove     = 0x2,
	Drawable           = 0x4,
	EditorDrawable     = 0x8,
	ShadowDrawableD    = 0x10,
	ShadowDrawableS    = 0x20,
	Interactable       = 0x40,
	DisableClone       = 0x80,
	HasPhysics         = 0x100,
	EditorRotatable    = 0x200
};

struct EditorSelectionMesh
{
	const eg::Model* model = nullptr;
	const eg::CollisionMesh* collisionMesh = nullptr;
	int meshIndex = -1;
	glm::mat4 transform;
};

class Ent : public std::enable_shared_from_this<Ent>
{
public:
	Ent() = default;
	
	Ent(Ent&& other) = delete;
	Ent(const Ent& other) = default;
	Ent& operator=(Ent&& other) = delete;
	Ent& operator=(const Ent& other) = default;
	
	virtual void Serialize(std::ostream& stream) const = 0;
	virtual void Deserialize(std::istream& stream) = 0;
	
	virtual void RenderSettings();
	
	virtual void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) { }
	virtual glm::vec3 GetEditorGridAlignment() const;
	virtual int GetEditorIconIndex() const;
	virtual glm::vec3 GetPosition() const = 0;
	virtual Dir GetFacingDirection() const { return Dir::PosY; }
	
	virtual eg::Span<const EditorSelectionMesh> GetEditorSelectionMeshes() const { return { }; }
	
	virtual glm::quat GetEditorRotation() { return glm::quat(); }
	virtual void EditorRotated(const glm::quat& newRotation) { }
	
	virtual void Spawned(bool isEditor);
	
	virtual void CommonDraw(const EntDrawArgs& args);
	virtual void GameDraw(const EntGameDrawArgs& args);
	virtual void EditorDraw(const EntEditorDrawArgs& args);
	
	virtual void Update(const struct WorldUpdateArgs& args);
	
	virtual void CollectPhysicsObjects(class PhysicsEngine& physicsEngine, float dt) { }
	virtual void CollectPointLights(std::vector<std::shared_ptr<PointLight>>& lights) { }
	
	static glm::mat3 GetRotationMatrix(Dir dir);
	
	uint32_t Name() const { return m_name; }
	EntTypeID TypeID() const { return m_typeID; }
	EntTypeFlags TypeFlags() const;
	
	virtual const void* GetComponent(const std::type_info& type) const;
	
	std::shared_ptr<Ent> Clone() const;
	
	//Utility functions for getting components
	template <typename T>
	const T* GetComponent() const { return static_cast<const T*>(GetComponent(typeid(T))); }
	template <typename T>
	T* GetComponentMut() { return static_cast<T*>(GetComponentMut(typeid(T))); }
	void* GetComponentMut(const std::type_info& type)
	{
		return const_cast<void*>(GetComponent(type));
	}
	
	template <typename T, typename... Args>
	static std::shared_ptr<T> Create(Args&&... args)
	{
		std::shared_ptr<T> ent = std::make_shared<T>(std::forward<Args>(args)...);
		ent->m_typeID = T::TypeID;
		ent->Ent::m_name = static_cast<uint32_t>(s_nameGen());
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
	
	template <typename T>
	friend std::shared_ptr<Ent> CloneEntity(const Ent& entity);
	
protected:
	eg::AABB GetAABB(float scale, float upDist, Dir facingDirection) const;
	
private:
	static std::mt19937 s_nameGen;
	
	uint32_t m_name = 0;
	EntTypeID m_typeID = (EntTypeID)-1;
	bool m_shouldSerialize = true;
};

template <typename T>
std::shared_ptr<Ent> CloneEntity(const Ent& entity)
{
	if constexpr ((int)T::EntFlags & (int)EntTypeFlags::DisableClone)
		return nullptr;
	else
		return Ent::Create<T>(static_cast<const T&>(entity));
}

template <typename ST>
inline void SerializePos(ST& st, const glm::vec3& position)
{
	st.set_posx(position.x);
	st.set_posy(position.y);
	st.set_posz(position.z);
}

template <typename ST>
[[nodiscard]] inline glm::vec3 DeserializePos(const ST& st)
{
	return glm::vec3(st.posx(), st.posy(), st.posz());
}

EG_BIT_FIELD(EntTypeFlags)

struct EntType
{
	EntTypeFlags flags;
	std::string name;
	std::string prettyName;
	std::shared_ptr<Ent> (*create)();
	std::shared_ptr<Ent> (*clone)(const Ent& ent);
};

extern std::unordered_map<EntTypeID, EntType> entTypeMap;
