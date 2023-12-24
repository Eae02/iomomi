#pragma once

#include "../../../Graphics/Materials/GooPlaneMaterial.hpp"
#include "../Components/LiquidPlaneComp.hpp"
#include "../Entity.hpp"

class GooPlaneEnt : public Ent
{
public:
	GooPlaneEnt();

	static constexpr EntTypeID TypeID = EntTypeID::GooPlane;
	static constexpr EntTypeFlags EntFlags =
		EntTypeFlags::Drawable | EntTypeFlags::EditorWallMove | EntTypeFlags::DisableClone;

	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;

	void RenderSettings() override;

	void EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;

	void GameDraw(const EntGameDrawArgs& args) override;

	const void* GetComponent(const std::type_info& type) const override;

	bool IsUnderwater(const eg::Sphere& sphere) const { return m_liquidPlane.IsUnderwater(sphere); }

	glm::vec3 GetPosition() const override { return m_liquidPlane.position; }

	int EdGetIconIndex() const override;

private:
	LiquidPlaneComp m_liquidPlane;
	GooPlaneMaterial m_material;
};
