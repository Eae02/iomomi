#pragma once

#include "GravityBarrierEnt.hpp"
#include "../Entity.hpp"
#include "../EntInteractable.hpp"
#include "../EntGravityChargeable.hpp"
#include "../Components/RigidBodyComp.hpp"
#include "../../Dir.hpp"
#include "../../../Graphics/WaterSimulator.hpp"

class CubeEnt : public Ent, public EntInteractable, public EntGravityChargeable
{
public:
	static constexpr EntTypeID TypeID = EntTypeID::Cube;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::Interactable;
	static constexpr int EditorIconIndex = 4;
	
	CubeEnt() : CubeEnt(glm::vec3(0.0f), false) { }
	CubeEnt(const glm::vec3& position, bool canFloat);
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void Spawned(bool isEditor) override;
	
	void RenderSettings() override;
	
	void Draw(const EntDrawArgs& args) override;
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	void UpdatePostSim(const struct WorldUpdateArgs& args);
	
	const void* GetComponent(const std::type_info& type) const override;
	
	void Interact(class Player& player) override;
	int CheckInteraction(const class Player& player) const override;
	
	bool SetGravity(Dir newGravity) override;
	
	std::string_view GetInteractDescription() const override;
	
	static constexpr float RADIUS = 0.4f;
	
	eg::Sphere GetSphere() const
	{
		return eg::Sphere(m_position, RADIUS * std::sqrt(3.0f));
	}
	
	bool canFloat = false;
	
private:
	RigidBodyComp m_rigidBody;
	GravityBarrierInteractableComp m_barrierInteractableComp;
	
	bool m_isPickedUp = false;
	Dir m_currentDown = Dir::NegY;
	
	std::shared_ptr<WaterSimulator::QueryAABB> m_waterQueryAABB;
	
	glm::quat m_rotation;
};

template <>
std::shared_ptr<Ent> CloneEntity<CubeEnt>(const Ent& entity);
