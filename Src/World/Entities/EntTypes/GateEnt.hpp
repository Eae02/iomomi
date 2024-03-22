#pragma once

#include "../../Door.hpp"
#include "../../PhysicsEngine.hpp"
#include "../Components/ActivatableComp.hpp"
#include "../Entity.hpp"

enum class GateVariant : int
{
	Neutral = 0,
	LevelExit = 1,
};

class GateEnt : public Ent
{
public:
	GateEnt();

	static constexpr EntTypeID TypeID = EntTypeID::Gate;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
	                                         EntTypeFlags::ShadowDrawableS | EntTypeFlags::ShadowDrawableD |
	                                         EntTypeFlags::EditorWallMove | EntTypeFlags::HasPhysics;

	static constexpr const char* EntName = "Gate";

	void RenderSettings() override;

	int EdGetIconIndex() const override;

	void EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;

	void Update(const WorldUpdateArgs& args) override;

	void CommonDraw(const EntDrawArgs& args) override;

	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;

	Door GetDoorDescription() const;

	void CollectPhysicsObjects(class PhysicsEngine& physicsEngine, float dt) override;

	const void* GetComponent(const std::type_info& type) const override;

	static void MovePlayer(const GateEnt* oldGate, const GateEnt& newGate, class Player& player);

	glm::vec3 GetPosition() const override { return position; }
	Dir GetFacingDirection() const override { return direction; }

	glm::mat4 GetTransform() const;
	std::pair<glm::vec3, glm::mat3> GetTranslationAndRotation() const;

	Dir direction{};
	glm::vec3 position;
	GateVariant variant = GateVariant::Neutral;

	std::string name;

	uint32_t InstanceID() const { return m_instanceID; }
	bool IsPlayerInside() const { return m_isPlayerInside; }
	bool CanLoadNext() const { return m_canLoadNext; }

private:
	static std::vector<glm::vec3> GetConnectionPoints(const Ent& entity);

	float m_doorOpenProgress = 0.0f;
	float m_timeBeforeClose = 0.0f;
	bool m_isPlayerCloseWithWrongGravity = false;

	ActivatableComp m_activatable;

	int m_doorOpenedSide = 0;

	bool m_isPlayerInside = false;
	bool m_canLoadNext = false;

	uint32_t m_instanceID;

	PhysicsObject m_physicsObject;
	PhysicsObject m_doorPhysicsObject;
};
