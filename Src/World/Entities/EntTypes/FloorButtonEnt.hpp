#pragma once

#include "../Entity.hpp"
#include "../Components/ActivatorComp.hpp"

class FloorButtonEnt : public Ent
{
public:
	FloorButtonEnt() = default;
	
	static constexpr EntTypeID TypeID = EntTypeID::FloorButton;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::EditorWallMove;
	
	void Draw(const EntDrawArgs& args) override;
	
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	static constexpr float SCALE = 1.0f;
	
	inline eg::AABB GetAABB() const
	{
		return Ent::GetAABB(SCALE, 0.2f);
	}
	
	inline void Activate()
	{
		m_activator.Activate();
	}
	
private:
	ActivatorComp m_activator;
};
