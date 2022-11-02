#include "SlidingWallEnt.hpp"
#include "PlatformEnt.hpp"
#include "../../WorldUpdateArgs.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Graphics/Lighting/PointLightShadowMapper.hpp"
#include "../../../ImGui.hpp"
#include "../../../../Protobuf/Build/SlidingWallEntity.pb.h"

DEF_ENT_TYPE(SlidingWallEnt)

constexpr float HALF_DEPTH = 0.1f;

SlidingWallEnt::SlidingWallEnt()
{
	m_physicsObject.canBePushed = false;
	m_physicsObject.canCarry = true;
	m_physicsObject.owner = this;
	m_physicsObject.mass = 100;
	m_physicsObject.debugColor = 0xcf24cf;
	m_physicsObject.constrainMove = &ConstrainMove;
	
	m_aaQuadComp.upPlane = 1;
	m_aaQuadComp.radius = glm::vec2(1, 1);
}

glm::vec3 SlidingWallEnt::ConstrainMove(const PhysicsObject& object, const glm::vec3& move)
{
	SlidingWallEnt& ent = *std::get<Ent*>(object.owner)->Downcast<SlidingWallEnt>();
	
	const float posSlideTime =
		glm::dot(object.position - ent.m_initialPosition, ent.m_slideOffset) / glm::length2(ent.m_slideOffset);
	const float slideDist = glm::dot(move, ent.m_slideOffset) / glm::length2(ent.m_slideOffset);
	
	return ent.m_slideOffset * glm::clamp(slideDist, -posSlideTime, 1 - posSlideTime);
}

void SlidingWallEnt::RenderSettings()
{
#ifdef EG_HAS_IMGUI
	Ent::RenderSettings();
	
	ImGui::DragFloat3("Slide Offset", &m_slideOffset.x, 0.1f);
	
	ImGui::DragFloat2("Size", &m_aaQuadComp.radius.x, 0.5f);
	
	ImGui::Combo("Forward Plane", &m_aaQuadComp.upPlane, "X\0Y\0Z\0");
	ImGui::Combo("Up Plane", &m_upPlane, "X\0Y\0Z\0");
#endif
}

void SlidingWallEnt::CommonDraw(const EntDrawArgs& args)
{
	glm::mat4 worldMatrix = glm::translate(glm::mat4(1), m_physicsObject.displayPosition) * m_rotationAndScale;
	args.meshBatch->AddModel(
		eg::GetAsset<eg::Model>("Models/SlidingWall.obj"),
		eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml"),
		StaticPropMaterial::InstanceData(worldMatrix));
	
	if (glm::length2(m_slideOffset) > 1E-5f && args.shadowDrawArgs == nullptr)
	{
		const glm::vec3 up = DirectionVector(static_cast<Dir>(m_upPlane * 2));
		float distToTrack = glm::dot(up, m_aabbRadius);
		
		glm::vec3 trackStart = m_initialPosition;
		glm::vec3 trackDir = m_slideOffset;
		if (std::abs(m_slideOffset[m_aaQuadComp.upPlane]) < 0.1f)
		{
			float extraTrackDistance = std::abs(glm::dot(m_slideOffset, m_aabbRadius) / glm::length2(m_slideOffset));
			glm::vec3 extraTrackVector = extraTrackDistance * m_slideOffset;
			trackStart -= extraTrackVector;
			trackDir += extraTrackVector * 2.0f;
		}
		
		PlatformEnt::DrawSliderMesh(*args.meshBatch, *args.frustum, trackStart - up * distToTrack, trackDir, up, 0.75f);
		PlatformEnt::DrawSliderMesh(*args.meshBatch, *args.frustum, trackStart + up * distToTrack, trackDir, -up, 0.75f);
	}
}

void SlidingWallEnt::Update(const WorldUpdateArgs& args)
{
	if (args.mode != WorldMode::Game)
	{
		m_physicsObject.displayPosition = m_initialPosition;
		UpdateShape();
	}
	else if (glm::distance(m_lastShadowUpdatePosition, m_physicsObject.displayPosition) > 1E-3f)
	{
		m_lastShadowUpdatePosition = m_physicsObject.displayPosition;
		eg::Sphere sphere = eg::Sphere::CreateEnclosing(std::get<eg::AABB>(m_physicsObject.shape));
		sphere.position += m_physicsObject.displayPosition;
		if (args.plShadowMapper)
		{
			args.plShadowMapper->Invalidate(sphere);
		}
	}
	
	m_physicsObject.gravity = DirectionVector(m_currentDown) * 2;
	m_waterBlockComp.InitFromAAQuadComponent(m_aaQuadComp, m_physicsObject.position);
}

const void* SlidingWallEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(AxisAlignedQuadComp))
		return &m_aaQuadComp;
	if (type == typeid(WaterBlockComp))
		return &m_waterBlockComp;
	return nullptr;
}

void SlidingWallEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
}

void SlidingWallEnt::Spawned(bool isEditor)
{
	UpdateShape();
	m_lastShadowUpdatePosition = m_initialPosition;
}

void SlidingWallEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_initialPosition = m_physicsObject.position = newPosition;
	UpdateWaterBlockComponent();
	m_waterBlockComp.editorVersion++;
}

void SlidingWallEnt::UpdateShape()
{
	auto [tangent, bitangent] = m_aaQuadComp.GetTangents(0);
	tangent *= 0.5f;
	bitangent *= 0.5f;
	m_aabbRadius = tangent + bitangent + m_aaQuadComp.GetNormal() * HALF_DEPTH;
	m_physicsObject.shape = eg::AABB(m_aabbRadius * -0.99f, m_aabbRadius * 0.99f);
	m_rotationAndScale = glm::mat3(tangent, bitangent, -m_aaQuadComp.GetNormal());
}

void SlidingWallEnt::UpdateWaterBlockComponent()
{
	std::fill_n(m_waterBlockComp.blockedGravities, 6, !m_neverBlockWater);
	m_waterBlockComp.InitFromAAQuadComponent(m_aaQuadComp, m_physicsObject.position);
}

bool SlidingWallEnt::SetGravity(Dir newGravity)
{
	m_currentDown = newGravity;
	m_physicsObject.velocity = glm::vec3(0);
	return true;
}

glm::vec3 SlidingWallEnt::GetPosition() const
{
	return m_physicsObject.position;
}

void SlidingWallEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::SlidingWallEntity entityPB;
	
	entityPB.set_forward_plane(m_aaQuadComp.upPlane);
	entityPB.set_up_plane(m_upPlane);
	entityPB.set_sizex(m_aaQuadComp.radius.x);
	entityPB.set_sizey(m_aaQuadComp.radius.y);
	SerializePos(entityPB, m_physicsObject.position);
	entityPB.set_slide_offset_x(m_slideOffset.x);
	entityPB.set_slide_offset_y(m_slideOffset.y);
	entityPB.set_slide_offset_z(m_slideOffset.z);
	entityPB.set_never_block_water(m_neverBlockWater);
	
	entityPB.SerializeToOstream(&stream);
}

void SlidingWallEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::SlidingWallEntity entityPB;
	entityPB.ParseFromIstream(&stream);
	
	m_initialPosition = m_physicsObject.position = DeserializePos(entityPB);
	m_neverBlockWater = entityPB.never_block_water();
	m_aaQuadComp.upPlane = entityPB.forward_plane();
	m_upPlane = entityPB.up_plane();
	m_aaQuadComp.radius = glm::vec2(entityPB.sizex(), entityPB.sizey());
	m_slideOffset.x = entityPB.slide_offset_x();
	m_slideOffset.y = entityPB.slide_offset_y();
	m_slideOffset.z = entityPB.slide_offset_z();
	
	UpdateWaterBlockComponent();
}
