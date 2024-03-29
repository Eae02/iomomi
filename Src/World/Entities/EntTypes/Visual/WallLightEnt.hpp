#pragma once

#include "../../../../Graphics/Lighting/PointLight.hpp"
#include "../../../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../Entity.hpp"

class WallLightEnt : public Ent
{
public:
	WallLightEnt();

	static constexpr EntTypeID TypeID = EntTypeID::WallLight;
	static constexpr EntTypeFlags EntFlags =
		EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::EditorWallMove;

	void Serialize(std::ostream& stream) const override;

	void Deserialize(std::istream& stream) override;

	int EdGetIconIndex() const override;

	void RenderSettings() override;

	void GameDraw(const EntGameDrawArgs& args) override;
	void EditorDraw(const EntEditorDrawArgs& args) override;

	glm::vec3 GetPosition() const override { return m_position; }
	Dir GetFacingDirection() const override { return m_forwardDir; }
	void EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;

	void CollectPointLights(std::vector<std::shared_ptr<PointLight>>& lights) override;

private:
	EmissiveMaterial::InstanceData GetInstanceData(float colorScale) const;

	void DrawEmissiveModel(const EntDrawArgs& drawArgs, float colorScale) const;

	Dir m_forwardDir;
	glm::vec3 m_position;
	eg::ColorSRGB m_color;
	float m_intensity;
	bool m_enableSpecularHighlights = true;
};
