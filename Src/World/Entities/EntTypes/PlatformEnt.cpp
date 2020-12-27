#include "PlatformEnt.hpp"
#include "../../Entities/EntityManager.hpp"
#include "../../WorldUpdateArgs.hpp"
#include "../../World.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Graphics/Lighting/PointLightShadowMapper.hpp"
#include "../../../Editor/PrimitiveRenderer.hpp"
#include "../../../../Protobuf/Build/PlatformEntity.pb.h"

#include <imgui.h>

static eg::Model* platformModel;
static eg::IMaterial* platformMaterial;

static eg::Model* platformSliderModel;
static eg::IMaterial* platformSliderMaterial;

static void OnInit()
{
	platformModel = &eg::GetAsset<eg::Model>("Models/Platform.obj");
	platformMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Platform.yaml");
	
	platformSliderModel = &eg::GetAsset<eg::Model>("Models/PlatformSlider.obj");
	platformSliderMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/PlatformSlider.yaml");
}

EG_ON_INIT(OnInit)

PlatformEnt::PlatformEnt()
	: m_activatable(&PlatformEnt::GetConnectionPoints)
{
	m_physicsObject.canBePushed = false;
	m_physicsObject.canCarry = true;
	m_physicsObject.owner = this;
	m_physicsObject.debugColor = 0xcf24cf;
	m_physicsObject.constrainMove = &ConstrainMove;
	
	m_aaQuadComp.upPlane = 1;
	m_aaQuadComp.radius = glm::vec2(1, 1);
}

static const glm::mat4 PLATFORM_ROTATION_MATRIX(0, 0, -1, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1);

//Gets the transformation matrix for the platform without any sliding
glm::mat4 PlatformEnt::GetBaseTransform() const 
{
	return glm::translate(glm::mat4(1), m_basePosition) *
	       glm::mat4(GetRotationMatrix(m_forwardDir)) *
	       PLATFORM_ROTATION_MATRIX;
}

std::vector<glm::vec3> PlatformEnt::GetConnectionPoints(const Ent& entity)
{
	const PlatformEnt& platform = static_cast<const PlatformEnt&>(entity);
	const glm::mat4 baseTransform = platform.GetBaseTransform();
	
	const float connectionPoints[] = { 0.0f, 0.5f, 1.0f };
	
	std::vector<glm::vec3> connectionPointsWS(std::size(connectionPoints));
	for (size_t i = 0; i < std::size(connectionPoints); i++)
	{
		const glm::vec2 slideOffset = platform.m_slideOffset * connectionPoints[i];
		connectionPointsWS[i] = baseTransform * glm::vec4(slideOffset.x, slideOffset.y, 0, 1);
	}
	
	return connectionPointsWS;
}

void PlatformEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	if (ImGui::DragFloat2("Slide Offset", &m_slideOffset.x, 0.1f))
	{
		m_editorLaunchTrajectory.clear();
	}
	
	if (ImGui::DragFloat("Slide Time", &m_slideTime))
	{
		m_slideTime = std::max(m_slideTime, 0.0f);
	}
	
	if (ImGui::DragFloat("Launch Speed", &m_launchSpeed, 0.1f, 0.0f, INFINITY))
	{
		m_launchSpeed = std::max(m_launchSpeed, 0.0f);
		m_editorLaunchTrajectory.clear();
	}
}

void PlatformEnt::DrawSliderMesh(eg::MeshBatch& meshBatch, const eg::Frustum& frustum,
	const glm::vec3& start, const glm::vec3& toEnd, const glm::vec3& up, float scale)
{
	const float slideDist = glm::length(toEnd);
	const glm::vec3 normToEnd = toEnd / slideDist;
	
	constexpr float SLIDER_MODEL_LENGTH = 0.2f;
	const int numSliderInstances = (int)std::round(slideDist / (SLIDER_MODEL_LENGTH * scale));
	
	const glm::mat4 commonTransform =
		glm::translate(glm::mat4(1), start) *
		glm::mat4(glm::mat3(glm::cross(up, normToEnd) * scale, up * scale, normToEnd * scale));
	
	for (int i = 1; i <= numSliderInstances; i++)
	{
		const glm::mat4 partTransform = glm::translate(commonTransform, glm::vec3(0, 0, SLIDER_MODEL_LENGTH * (float)i));
		const eg::AABB aabb = platformSliderModel->GetMesh(0).boundingAABB->TransformedBoundingBox(partTransform);
		if (frustum.Intersects(aabb))
		{
			meshBatch.AddModel(*platformSliderModel, *platformSliderMaterial,
			                   StaticPropMaterial::InstanceData(partTransform));
		}
	}
}

void PlatformEnt::DrawSliderMesh(eg::MeshBatch& meshBatch, const eg::Frustum& frustum) const
{
	const glm::vec3 localFwd(m_slideOffset.y, 0, -m_slideOffset.x);
	const glm::mat3 rotationMatrix = GetRotationMatrix(m_forwardDir);
	DrawSliderMesh(meshBatch, frustum, m_basePosition, rotationMatrix * localFwd, rotationMatrix[1], 0.5f);
}

void PlatformEnt::GameDraw(const EntGameDrawArgs& args)
{
	if (args.shadowDrawArgs == nullptr)
	{
		DrawSliderMesh(*args.meshBatch, *args.frustum);
	}
	
	const glm::mat4 transform = glm::translate(glm::mat4(1), m_physicsObject.position) * GetBaseTransform();
	const eg::AABB aabb = platformModel->GetMesh(0).boundingAABB->TransformedBoundingBox(transform);
	if (args.frustum->Intersects(aabb))
	{
		args.meshBatch->AddModel(*platformModel, *platformMaterial, StaticPropMaterial::InstanceData(transform));
	}
}

static constexpr float LAUNCH_TIME = 0.8f;

void PlatformEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	DrawSliderMesh(*args.meshBatch, *args.frustum);
	args.meshBatch->AddModel(*platformModel, *platformMaterial, StaticPropMaterial::InstanceData(GetBaseTransform()));
	
	if (m_launchSpeed > 0)
	{
		if (m_editorLaunchTrajectory.empty())
		{
			ComputeLaunchVelocity();
			
			glm::vec3 startPos = glm::mix(m_basePosition, FinalPosition(), LAUNCH_TIME) + glm::vec3(DirectionVector(m_forwardDir));
			m_editorLaunchTrajectory.push_back(startPos);
			auto posAt = [&] (float dst)
			{
				return glm::vec3(0, -0.5f * GRAVITY_MAG * dst * dst, 0) + m_launchVelocityToSet * dst + startPos;
			};
			
			constexpr float STEP_SIZE = 0.05f;
			for (float dst = STEP_SIZE; dst < 50; dst += STEP_SIZE)
			{
				glm::vec3 pos = posAt(dst);
				if (!args.world->IsAir(glm::floor(pos)))
				{
					float lo = dst - STEP_SIZE;
					float hi = dst;
					for (int i = 0; i < 20; i++)
					{
						float mid = (lo + hi) / 2;
						pos = posAt(mid);
						if (args.world->IsAir(glm::floor(pos)))
							lo = mid;
						else
							hi = mid;
					}
					m_editorLaunchTrajectory.push_back(pos);
					break;
				}
				m_editorLaunchTrajectory.push_back(pos);
			}
		}
		
		static const eg::ColorSRGB color = eg::ColorSRGB::FromRGBAHex(0xde6e1faa);
		
		for (size_t i = 1; i < m_editorLaunchTrajectory.size(); i++)
		{
			args.primitiveRenderer->AddLine(m_editorLaunchTrajectory[i - 1], m_editorLaunchTrajectory[i], color, 0.05f);
		}
	}
}

glm::vec3 PlatformEnt::ConstrainMove(const PhysicsObject& object, const glm::vec3& move)
{
	PlatformEnt& ent = *(PlatformEnt*)std::get<Ent*>(object.owner);
	glm::vec3 moveDir = ent.FinalPosition() - ent.m_basePosition;
	
	float posSlideTime = glm::dot(object.position, moveDir) / glm::length2(moveDir);
	float slideDist = glm::dot(move, moveDir) / glm::length2(moveDir);
	
	return moveDir * glm::clamp(slideDist, -posSlideTime, 1 - posSlideTime);
}

void PlatformEnt::Update(const WorldUpdateArgs& args)
{
	if (args.plShadowMapper)
	{
		if (!m_prevShadowInvalidatePos || glm::distance2(m_physicsObject.position, *m_prevShadowInvalidatePos) > 0.001f)
		{
			m_prevShadowInvalidatePos = m_physicsObject.position;
			eg::AABB aabb = std::get<eg::AABB>(m_physicsObject.shape);
			aabb.min += m_physicsObject.position;
			aabb.max += m_physicsObject.position;
			args.plShadowMapper->Invalidate(eg::Sphere::CreateEnclosing(aabb));
		}
	}
}

void PlatformEnt::ComputeLaunchVelocity()
{
	glm::vec3 moveDir = glm::normalize(FinalPosition() - m_basePosition);
	m_launchVelocityToSet = m_launchSpeed * moveDir;
}

const void* PlatformEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(ActivatableComp))
		return &m_activatable;
	return Ent::GetComponent(type);
}

void PlatformEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::PlatformEntity platformPB;
	
	platformPB.set_dir((gravity_pb::Dir)m_forwardDir);
	SerializePos(platformPB, m_basePosition);
	
	platformPB.set_slide_offset_x(m_slideOffset.x);
	platformPB.set_slide_offset_y(m_slideOffset.y);
	platformPB.set_slide_time(m_slideTime);
	platformPB.set_launch_speed(m_launchSpeed);
	
	platformPB.set_name(m_activatable.m_name);
	
	platformPB.SerializeToOstream(&stream);
}

void PlatformEnt::Deserialize(std::istream& stream)
{
	gravity_pb::PlatformEntity platformPB;
	platformPB.ParseFromIstream(&stream);
	
	m_basePosition = DeserializePos(platformPB);
	m_forwardDir = (Dir)platformPB.dir();
	
	m_slideOffset = glm::vec2(platformPB.slide_offset_x(), platformPB.slide_offset_y());
	m_slideTime = platformPB.slide_time();
	m_launchSpeed = platformPB.launch_speed();
	m_physicsObject.shape = eg::AABB(glm::vec3(-0.99, -0.1f, -1.99), glm::vec3(0.99, 0, -0.01)).TransformedBoundingBox(GetBaseTransform());
	
	ComputeLaunchVelocity();
	
	if (platformPB.name() != 0)
		m_activatable.m_name = platformPB.name();
}

void PlatformEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	glm::vec3 moveDir = FinalPosition() - m_basePosition;
	float slideProgress = glm::clamp(glm::dot(m_physicsObject.position, moveDir) / glm::length2(moveDir), 0.0f, 1.0f);
	float oldSlideProgress = slideProgress;
	
	//Updates the slide progress
	if (m_activatable.m_enabledConnections)
	{
		m_physicsObject.canBePushed = false;
		m_physicsObject.constrainMove = nullptr;
		
		const bool activated = m_activatable.AllSourcesActive();
		const float slideDelta = dt / m_slideTime;
		if (activated)
			slideProgress = std::min(slideProgress + slideDelta, 1.0f);
		else
			slideProgress = std::max(slideProgress - slideDelta, 0.0f);
		
		glm::vec3 wantedPosition = moveDir * slideProgress;
		//m_physicsObject.position = wantedPosition;
		m_physicsObject.move = wantedPosition - m_physicsObject.position;
		
		if (oldSlideProgress < LAUNCH_TIME && slideProgress >= LAUNCH_TIME)
			m_launchVelocity = m_launchVelocityToSet;
		else
			m_launchVelocity = { };
		
		if (!m_physicsObject.childObjects.empty() && glm::length2(m_launchVelocity) > 1E-3f)
		{
			for (PhysicsObject* object : m_physicsObject.childObjects)
			{
				object->velocity = m_launchVelocity;
				object->position += glm::normalize(m_launchVelocity) * 0.5f;
			}
		}
	}
	else
	{
		m_physicsObject.canBePushed = true;
	}
	
	physicsEngine.RegisterObject(&m_physicsObject);
}

glm::vec3 PlatformEnt::FinalPosition() const
{
	return GetBaseTransform() * glm::vec4(m_slideOffset.x, m_slideOffset.y, 0, 1);
}

void PlatformEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_editorLaunchTrajectory.clear();
	m_basePosition = newPosition;
	if (faceDirection)
		m_forwardDir = *faceDirection;
}
