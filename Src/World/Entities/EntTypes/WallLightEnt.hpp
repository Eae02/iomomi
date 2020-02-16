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
	
private:
	EmissiveMaterial::InstanceData GetInstanceData(float colorScale) const;
	
	PointLight m_pointLight;
};


