#pragma once

#include "../Entity.hpp"
#include "../Components/AxisAlignedQuadComp.hpp"
#include "../Components/WaterBlockComp.hpp"
#include "../../PhysicsEngine.hpp"

class WindowEnt : public Ent
{
public:
	WindowEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::Window;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::ShadowDrawableS | EntTypeFlags::HasPhysics;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;
	
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	glm::vec3 GetPosition() const override;
	
	bool NeedsBlurredTextures() const;
	
private:
	void UpdateWaterBlock();
	
	enum class WaterBlockMode
	{
		Auto = 0,
		Never = 1,
		Always = 2
	};
	
	uint32_t m_windowType = 0;
	float m_textureScale = 1;
	WaterBlockMode m_waterBlockMode = WaterBlockMode::Auto;
	AxisAlignedQuadComp m_aaQuad;
	WaterBlockComp m_waterBlockComp;
	
	PhysicsObject m_physicsObject;
};
