#pragma once

#include "../Entity.hpp"

class DecalEnt : public Ent
{
public:
	DecalEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::Decal;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::EditorWallMove;
	
	void SetMaterialByName(std::string_view materialName);
	const char* GetMaterialName() const;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	glm::vec3 GetPosition() const override { return m_position; }
	Dir GetFacingDirection() const override { return m_direction; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	float rotation = 0;
	float scale = 1;
	
private:
	glm::vec3 m_position;
	Dir m_direction;
	
	glm::mat4 GetDecalTransform() const;
	
	void UpdateMaterialPointer();
	
	const class DecalMaterial* m_material = nullptr;
	int m_decalMaterialIndex = 0;
	glm::ivec2 m_repetitions;
};
