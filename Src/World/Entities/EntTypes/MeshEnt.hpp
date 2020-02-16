#pragma once

#include "../Entity.hpp"

class MeshEnt : public Ent
{
public:
	MeshEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::Mesh;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
private:
	int m_typeIndex = 0;
};
