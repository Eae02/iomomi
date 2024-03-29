#pragma once

#include "../Components/LiquidPlaneComp.hpp"
#include "../Entity.hpp"

class WaterPlaneEnt : public Ent
{
public:
	WaterPlaneEnt();

	static constexpr EntTypeID TypeID = EntTypeID::WaterPlane;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::EditorWallMove | EntTypeFlags::DisableClone;

	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;

	const void* GetComponent(const std::type_info& type) const override;

	void EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;

	void RenderSettings() override;

	glm::vec3 GetPosition() const override;

	int EdGetIconIndex() const override;

	LiquidPlaneComp liquidPlane;
	int densityBoost = 0;
};
