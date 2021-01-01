#pragma once

#include "../Entity.hpp"
#include "../Components/ActivatableComp.hpp"
#include "CubeEnt.hpp"

class CubeSpawnerEnt : public Ent
{
public:
	static constexpr EntTypeID TypeID = EntTypeID::CubeSpawner;
	static constexpr EntTypeFlags EntFlags =
		EntTypeFlags::EditorWallMove | EntTypeFlags::EditorDrawable | EntTypeFlags::Drawable;
	
	CubeSpawnerEnt();
	
	int GetEditorIconIndex() const override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	glm::vec3 GetPosition() const override { return m_position; }
	Dir GetFacingDirection() const override { return m_direction; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
private:
	static std::vector<glm::vec3> GetConnectionPoints(const Ent& entity);
	
	glm::vec3 m_position;
	Dir m_direction;
	
	enum class State
	{
		Initial,
		OpeningDoor,
		PushingCube,
		ClosingDoor
	};
	State m_state = State::Initial;
	
	float m_doorOpenProgress = 0;
	
	ActivatableComp m_activatable;
	
	std::weak_ptr<CubeEnt> m_cube;
	bool m_wasActivated = false;
	bool m_cubeCanFloat = false;
	bool m_requireActivation = false;
	bool m_activationResets = false;
};
