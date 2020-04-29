#pragma once

#include "../Entity.hpp"
#include "../EntInteractable.hpp"
#include "../../../Graphics/Materials/GravitySwitchVolLightMaterial.hpp"

class GravitySwitchEnt : public Ent, public EntInteractable
{
public:
	GravitySwitchEnt() = default;
	
	static constexpr EntTypeID TypeID = EntTypeID::GravitySwitch;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::Interactable | EntTypeFlags::EditorWallMove;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void GameDraw(const EntGameDrawArgs& args) override;
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	void Interact(class Player& player) override;
	
	int CheckInteraction(const class Player& player, const class PhysicsEngine& physicsEngine) const override;
	
	std::string_view GetInteractDescription() const override;
	
	static constexpr float SCALE = 1.0f;
	
	inline eg::AABB GetAABB() const
	{
		return Ent::GetAABB(SCALE, 0.2f);
	}
	
private:
	GravitySwitchVolLightMaterial volLightMaterial;
};


