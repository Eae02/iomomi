#pragma once

#include "../Entity.hpp"
#include "../Components/LiquidPlaneComp.hpp"

class WaterPlaneEnt : public Ent
{
public:
	WaterPlaneEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::WaterPlane;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::EditorWallMove;
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	LiquidPlaneComp m_liquidPlane;
};

template <>
std::shared_ptr<Ent> CloneEntity<WaterPlaneEnt>(const Ent& entity);
