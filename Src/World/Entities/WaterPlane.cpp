#include "WaterPlane.hpp"
#include "ECWallMounted.hpp"
#include "ECLiquidPlane.hpp"
#include "../../../Protobuf/Build/WaterPlaneEntity.pb.h"

namespace WaterPlane
{
	eg::EntitySignature EntitySignature = eg::EntitySignature::Create<
	    eg::ECPosition3D, ECWallMounted, ECLiquidPlane, ECEditorVisible>();
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager)
	{
		eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
		
		entity.InitComponent<ECEditorVisible>("Water");
		
		ECLiquidPlane& liquidPlane = entity.GetComponent<ECLiquidPlane>();
		liquidPlane.SetShouldGenerateMesh(true);
		liquidPlane.SetEditorColor(eg::ColorSRGB::FromRGBAHex(0x67BEEA98));
		
		return &entity;
	}
	
	struct Serializer : eg::IEntitySerializer
	{
		std::string_view GetName() const override
		{
			return "WaterPlane";
		}
		
		void Serialize(const eg::Entity& entity, std::ostream& stream) const override
		{
			gravity_pb::WaterPlaneEntity gooPlanePB;
			
			glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
			gooPlanePB.set_posx(pos.x);
			gooPlanePB.set_posy(pos.y);
			gooPlanePB.set_posz(pos.z);
			
			gooPlanePB.SerializeToOstream(&stream);
		}
		
		void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
		{
			eg::Entity& entity = *CreateEntity(entityManager);
			
			gravity_pb::WaterPlaneEntity gooPlanePB;
			gooPlanePB.ParseFromIstream(&stream);
			
			entity.InitComponent<eg::ECPosition3D>(gooPlanePB.posx(), gooPlanePB.posy(), gooPlanePB.posz());
		}
	};
	
	eg::IEntitySerializer* EntitySerializer = new Serializer;
}
