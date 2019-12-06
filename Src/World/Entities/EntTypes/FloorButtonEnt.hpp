#pragma once

#include "../Entity.hpp"
#include "../Components/ActivatorComp.hpp"
#include "../Components/RigidBodyComp.hpp"

class FloorButtonEnt : public Ent
{
public:
	FloorButtonEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::FloorButton;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::EditorWallMove | EntTypeFlags::DisableClone;
	
	void Draw(const EntDrawArgs& args) override;
	
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	static constexpr float SCALE = 1.0f;
	
	eg::AABB GetAABB() const;
	
	inline void Activate()
	{
		m_activator.Activate();
	}
	
private:
	ActivatorComp m_activator;
	RigidBodyComp m_rigidBody;
	
	float m_padPushDist = 0;
};
