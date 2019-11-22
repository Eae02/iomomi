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
	
	void Serialize(std::ostream& stream) override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void Draw(const EntDrawArgs& args) override;
	
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
private:
	EmissiveMaterial::InstanceData GetInstanceData(float colorScale) const;
	
	PointLight m_pointLight;
};


