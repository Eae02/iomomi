#pragma once

#include "../Entity.hpp"
#include "../Components/AxisAlignedQuadComp.hpp"
#include "../Components/RigidBodyComp.hpp"

class WindowEnt : public Ent
{
public:
	WindowEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::Window;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::DisableClone;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void Draw(const EntDrawArgs& args) override;
	
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
private:
	void Draw(eg::MeshBatch& meshBatch, eg::MeshBatchOrdered& transparentMeshBatch) const;
	
	const eg::IMaterial* m_material;
	float m_textureScale = 1;
	AxisAlignedQuadComp m_aaQuad;
	RigidBodyComp m_rigidBodyComp;
	
	std::unique_ptr<btBoxShape> m_bulletShape;
};
