#pragma once

#include <optional>
#include <EGame/AABB.hpp>
#include "../Entity.hpp"
#include "../Components/AxisAlignedQuadComp.hpp"
#include "../../PhysicsEngine.hpp"

class WindowEnt : public Ent
{
public:
	WindowEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::Window;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::HasPhysics;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;
	
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	glm::vec3 GetPosition() const override;
	
private:
	const eg::IMaterial* m_material;
	float m_textureScale = 1;
	AxisAlignedQuadComp m_aaQuad;
	
	PhysicsObject m_physicsObject;
};
