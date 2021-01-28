#pragma once

#include "../../Entity.hpp"

class MeshEnt : public Ent
{
public:
	MeshEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::Mesh;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	glm::vec3 GetPosition() const override { return m_position; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
private:
	glm::vec3 m_position;
	
	int m_typeIndex = 0;
};
