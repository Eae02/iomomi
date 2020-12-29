#pragma once

#include "../Entity.hpp"
#include "../Components/ActivatableComp.hpp"

class CubeSpawnerEnt : public Ent
{
public:
	static constexpr EntTypeID TypeID = EntTypeID::CubeSpawner;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::EditorWallMove;
	
	CubeSpawnerEnt() = default;
	
	int GetEditorIconIndex() const override;
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	glm::vec3 GetPosition() const override { return m_position; }
	Dir GetFacingDirection() const override { return m_direction; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
private:
	glm::vec3 m_position;
	Dir m_direction;
	
	ActivatableComp m_activatable;
	
	std::weak_ptr<Ent> m_cube;
	bool m_wasActivated = false;
	bool m_cubeCanFloat = false;
	bool m_requireActivation = false;
	bool m_activationResets = false;
};
