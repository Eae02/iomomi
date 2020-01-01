#pragma once

#include "../Entity.hpp"
#include "../Components/RigidBodyComp.hpp"
#include "../Components/ActivatableComp.hpp"
#include "../../WorldUpdateArgs.hpp"
#include "../../Door.hpp"

class EntranceExitEnt : public Ent
{
public:
	enum class Type
	{
		Entrance = 0,
		Exit = 1
	};
	
	static constexpr EntTypeID TypeID = EntTypeID::EntranceExit;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::DisableClone |
		EntTypeFlags::EditorWallMove | EntTypeFlags::Activatable;
	
	EntranceExitEnt();
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	void InitPlayer(class Player& player);
	
	static void MovePlayer(const EntranceExitEnt& oldExit, const EntranceExitEnt& newEntrance, class Player& player);
	
	void Update(const WorldUpdateArgs& args);
	
	void Draw(const EntDrawArgs& args) override;
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
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
	
	RigidBodyComp m_rigidBody;
	btRigidBody* m_door1RigidBody;
	btRigidBody* m_door2RigidBody;
	
	ActivatableComp m_activatable;
	
	PointLight m_pointLight;
	
	std::string m_name = "main";
	float m_doorOpenProgress = 0;
	float m_timeBeforeClose = 0;
	
	bool m_shouldSwitchEntrance = false;
};