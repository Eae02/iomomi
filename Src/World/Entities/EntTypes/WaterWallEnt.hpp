#pragma once

#include <optional>
#include "../Entity.hpp"
#include "../Components/WaterBlockComp.hpp"
#include "../Components/AxisAlignedQuadComp.hpp"

class WaterWallEnt : public Ent
{
public:
	WaterWallEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::WaterWall;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::EditorDrawable;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	glm::vec3 GetPosition() const override;
	
private:
	glm::vec3 m_position;
	AxisAlignedQuadComp m_aaQuad;
	WaterBlockComp m_waterBlockComp;
};
