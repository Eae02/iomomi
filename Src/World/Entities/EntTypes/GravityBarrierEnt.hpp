#pragma once

#include "../Entity.hpp"
#include "../Components/ActivatableComp.hpp"
#include "../Components/RigidBodyComp.hpp"

struct GravityBarrierInteractableComp
{
	Dir currentDown;
};

class GravityBarrierEnt : public Ent
{
public:
	static constexpr EntTypeID TypeID = EntTypeID::GravityBarrier;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::Activatable | EntTypeFlags::DisableClone;
	
	GravityBarrierEnt();
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void Draw(const EntDrawArgs& args) override;
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	void Update(const WorldUpdateArgs& args) override;
	
	void Spawned(bool isEditor) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	int BlockedAxis() const;
	
	glm::mat4 GetTransform() const;
	
	enum class ActivateAction
	{
		Disable,
		Enable,
		Rotate
	};
	
	int flowDirection = 0;
	int upPlane = 0;
	ActivateAction activateAction = ActivateAction::Disable;
	glm::vec2 size { 2.0f };
	
private:
	static std::vector<glm::vec3> GetConnectionPoints(const Ent& entity);
	
	std::tuple<glm::vec3, glm::vec3> GetTangents() const;
	
	void Draw(eg::MeshBatchOrdered& meshBatchOrdered, eg::MeshBatch& meshBatch) const;
	
	ActivatableComp m_activatable;
	
	float m_opacity = 1;
	bool m_enabled = true;
	bool m_blockFalling = false;
	int m_flowDirectionOffset = 0;
	btBoxShape m_collisionShape;
	
	RigidBodyComp m_rigidBody;
};
