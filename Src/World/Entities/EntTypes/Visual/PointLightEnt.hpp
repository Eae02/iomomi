#pragma once

#include "../../../../Graphics/Lighting/PointLight.hpp"
#include "../../Entity.hpp"

class PointLightEnt : public Ent
{
public:
	PointLightEnt();

	static constexpr EntTypeID TypeID = EntTypeID::PointLight;
	static constexpr EntTypeFlags EntFlags = {};

	void Serialize(std::ostream& stream) const override;

	void Deserialize(std::istream& stream) override;

	int EdGetIconIndex() const override;

	void RenderSettings() override;

	glm::vec3 GetPosition() const override { return m_position; }
	void EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;

	void CollectPointLights(std::vector<std::shared_ptr<PointLight>>& lights) override;

	static const eg::ColorSRGB DefaultColor;
	static const float DefaultIntensity;

	static void ColorAndIntensitySettings(eg::ColorSRGB& color, float& intensity, bool& enableSpecularHighlights);

private:
	glm::vec3 m_position;
	eg::ColorSRGB m_color;
	float m_intensity;
	bool m_enableSpecularHighlights = true;
};
