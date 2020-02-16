#include "GravitySwitchEnt.hpp"
#include "../../Player.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Graphics/Materials/GravitySwitchMaterial.hpp"
#include "../../../../Protobuf/Build/GravitySwitchEntity.pb.h"

static eg::Model* s_model;
static eg::IMaterial* s_material;
static int s_centerMaterialIndex;

static void OnInit()
{
	s_model = &eg::GetAsset<eg::Model>("Models/GravitySwitch.obj");
	s_material = &eg::GetAsset<StaticPropMaterial>("Materials/GravitySwitch.yaml");
	s_centerMaterialIndex = s_model->GetMaterialIndex("Center");
}

EG_ON_INIT(OnInit)

static inline void Draw(const Ent& entity, eg::MeshBatch& meshBatch)
{
	for (size_t m = 0; m < s_model->NumMeshes(); m++)
	{
		const eg::IMaterial* material = s_material;
		if (s_model->GetMesh(m).materialIndex == s_centerMaterialIndex)
			material = &GravitySwitchMaterial::instance;
		
		meshBatch.AddModelMesh(*s_model, m, *material,
			StaticPropMaterial::InstanceData(entity.GetTransform(GravitySwitchEnt::SCALE)));
	}
}

void GravitySwitchEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	::Draw(*this, *args.meshBatch);
}

void GravitySwitchEnt::GameDraw(const EntGameDrawArgs& args)
{
	::Draw(*this, *args.meshBatch);
	
	volLightMaterial.rotationMatrix = GetRotationMatrix();
	volLightMaterial.switchPosition = m_position;
	args.transparentMeshBatch->AddNoData(GravitySwitchVolLightMaterial::GetMesh(), volLightMaterial,
		DepthDrawOrder(volLightMaterial.switchPosition));
}

void GravitySwitchEnt::Interact(Player& player)
{
	player.FlipDown();
}

int GravitySwitchEnt::CheckInteraction(const Player& player) const
{
	bool canInteract = player.CurrentDown() == OppositeDir(m_direction) && player.OnGround() &&
		GetAABB().Contains(player.FeetPosition()) && !player.m_isCarrying;
	
	constexpr int INTERACT_PRIORITY = 1;
	return canInteract ? INTERACT_PRIORITY : 0;
}

std::string_view GravitySwitchEnt::GetInteractDescription() const
{
	return "Flip Gravity";
}

void GravitySwitchEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::GravitySwitchEntity switchPB;
	
	switchPB.set_dir((gravity_pb::Dir)m_direction);
	SerializePos(switchPB);
	
	switchPB.SerializeToOstream(&stream);
}

void GravitySwitchEnt::Deserialize(std::istream& stream)
{
	gravity_pb::GravitySwitchEntity switchPB;
	switchPB.ParseFromIstream(&stream);
	
	DeserializePos(switchPB);
	m_direction = (Dir)switchPB.dir();
}
