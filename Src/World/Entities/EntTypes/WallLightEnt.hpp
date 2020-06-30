#pragma once

#include "../Entity.hpp"
#include "../../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../../Graphics/Lighting/PointLight.hpp"

class WallLightEnt : public Ent
{
public:
	WallLightEnt() = default;
	
	static constexpr EntTypeID TypeID = EntTypeID::WallLight;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::EditorWallMove;
	static constexpr int EditorIconIndex = 3;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void GameDraw(const EntGameDrawArgs& args) override;
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	void HalfLightIntensity();
	
	glm::vec3 GetPosition() const override { return m_position; }
	Dir GetFacingDirection() const override { return m_forwardDir; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
private:
	EmissiveMaterial::InstanceData GetInstanceData(float colorScale) const;
	
	void DrawEmissiveModel(const EntDrawArgs& drawArgs, float colorScale) const;
	
	Dir m_forwardDir;
	glm::vec3 m_position;
	
	PointLight m_pointLight;
};
