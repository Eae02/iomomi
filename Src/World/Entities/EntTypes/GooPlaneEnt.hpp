#pragma once

#include "../Entity.hpp"
#include "../Components/LiquidPlaneComp.hpp"
#include "../../../Graphics/Materials/GooPlaneMaterial.hpp"

class GooPlaneEnt : public Ent
{
public:
	GooPlaneEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::GooPlane;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorWallMove;
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	void Draw(const EntDrawArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	bool IsUnderwater(const eg::Sphere& sphere) const
	{
		return m_liquidPlane.IsUnderwater(*this, sphere);
	}
	
private:
	LiquidPlaneComp m_liquidPlane;
	GooPlaneMaterial m_material;
};
