#pragma once

#include "../Entity.hpp"
#include "../Components/ActivatableComp.hpp"
#include "../../WorldUpdateArgs.hpp"
#include "../../Door.hpp"
#include "../../PhysicsEngine.hpp"
#include "../../../Graphics/Materials/ScreenMaterial.hpp"

class EntranceExitEnt : public Ent
{
public:
	enum class Type
	{
		Entrance = 0,
		Exit = 1
	};
	
	static constexpr EntTypeID TypeID = EntTypeID::EntranceExit;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::EditorWallMove | EntTypeFlags::Activatable | EntTypeFlags::HasPhysics;
	
	EntranceExitEnt();
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	void InitPlayer(class Player& player);
	
	static void MovePlayer(const EntranceExitEnt& oldExit, const EntranceExitEnt& newEntrance, class Player& player);
	
	void Update(const WorldUpdateArgs& args) override;
	
	void GameDraw(const EntGameDrawArgs& args) override;
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	void CollectPhysicsObjects(class PhysicsEngine& physicsEngine, float dt) override;
	
	Door GetDoorDescription() const;
	
	Type m_type = Type::Entrance;
	
	const std::string& GetName() const
	{
		return m_name;
	}
	
	bool ShouldSwitchEntrance() const
	{
		return m_shouldSwitchEntrance;
	}
	
private:
	std::tuple<glm::mat3, glm::vec3> GetTransformParts() const;
	glm::mat4 GetTransform() const;
	
	static std::vector<glm::vec3> GetConnectionPoints(const Ent& entity);
	
	PhysicsObject m_roomPhysicsObject;
	PhysicsObject m_door1PhysicsObject;
	PhysicsObject m_door2PhysicsObject;
	
	ActivatableComp m_activatable;
	
	PointLight m_pointLight;
	
	mutable ScreenMaterial m_screenMaterial;
	
	std::string m_name = "main";
	float m_doorOpenProgress = 0;
	float m_timeBeforeClose = 0;
	
	bool m_shouldSwitchEntrance = false;
	bool m_door1Open = false;
	bool m_door2Open = false;
	
	std::string_view m_levelTitle;
};

template <>
std::shared_ptr<Ent> CloneEntity<EntranceExitEnt>(const Ent& entity);
