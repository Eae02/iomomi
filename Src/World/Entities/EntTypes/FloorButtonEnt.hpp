#pragma once

#include "../Entity.hpp"
#include "../Components/ActivatorComp.hpp"
#include "../../PhysicsEngine.hpp"

class FloorButtonEnt : public Ent
{
public:
	FloorButtonEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::FloorButton;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::EditorWallMove | EntTypeFlags::DisableClone | EntTypeFlags::HasPhysics;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine) override;
	
	eg::AABB GetAABB() const;
	
	inline void Activate()
	{
		m_activator.Activate();
	}
	
private:
	ActivatorComp m_activator;
	PhysicsObject m_physicsObject;
	
	float m_timeSincePushed = 0;
	float m_padPushDist = 0;
};
