#include "PushButtonEnt.hpp"

#include "../../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../ImGui.hpp"
#include "../../../../Settings.hpp"
#include "../../../Player.hpp"
#include <PushButtonEntity.pb.h>

DEF_ENT_TYPE(PushButtonEnt)

static const eg::Model* pushButtonModel;
static std::vector<const eg::IMaterial*> pushButtonMaterials;

static constexpr float SCALE = 0.4f;

static void OnInit()
{
	pushButtonModel = &eg::GetAsset<eg::Model>("Models/PushButton.obj");
	pushButtonMaterials.resize(
		pushButtonModel->NumMeshes(), &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml")
	);
}

EG_ON_INIT(OnInit)

void PushButtonEnt::RenderSettings()
{
#ifdef EG_HAS_IMGUI
	Ent::RenderSettings();

	ImGui::SliderInt("Rotation", &m_rotation, 0, 3);

	ImGui::Separator();

	ImGui::DragFloat("Activation Delay", &m_activationDelay, 0.1f);
	ImGui::DragFloat("Activation Duration", &m_activationDuration, 0.1f);
#endif
}

glm::mat4 PushButtonEnt::GetTransform() const
{
	return glm::translate(glm::mat4(1), m_position) * glm::mat4(GetRotationMatrix(m_direction)) *
	       glm::rotate(glm::mat4(1), static_cast<float>(m_rotation) * eg::HALF_PI, glm::vec3(0, 1, 0)) *
	       glm::scale(glm::mat4(1), glm::vec3(SCALE));
}

void PushButtonEnt::CommonDraw(const EntDrawArgs& args)
{
	StaticPropMaterial::InstanceData propInstanceData(GetTransform());

	for (size_t m = 0; m < pushButtonModel->NumMeshes(); m++)
	{
		args.meshBatch->AddModelMesh(*pushButtonModel, m, *pushButtonMaterials[m], propInstanceData);
	}
}

void PushButtonEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	m_direction = faceDirection.value_or(m_direction);
}

void PushButtonEnt::Update(const struct WorldUpdateArgs& args)
{
	if (m_timeSincePressed < (m_activationDelay + m_activationDuration))
	{
		if (m_timeSincePressed > m_activationDelay)
			m_activator.Activate();
		m_timeSincePressed += args.dt;
	}

	m_activator.Update(args);
}

void PushButtonEnt::Interact(Player& player)
{
	m_timeSincePressed = 0;
}

int PushButtonEnt::CheckInteraction(const Player& player, const PhysicsEngine& physicsEngine) const
{
	static constexpr float MAX_INTERACT_DIST = 1;
	static constexpr int INTERACT_PRIORITY = 2000;

	const eg::Ray ray(player.EyePosition(), player.Forward());
	const eg::AABB boundingAABB = *pushButtonModel->GetMesh(pushButtonModel->GetMeshIndex("Cube")).boundingAABB;
	const std::optional<float> buttonIntersectDist =
		RayIntersectOrientedBox(ray, OrientedBox::FromAABB(boundingAABB).Transformed(GetTransform()));
	if (buttonIntersectDist.has_value() && *buttonIntersectDist < MAX_INTERACT_DIST)
	{
		auto [intersectObj, intersectDist] = physicsEngine.RayIntersect(ray, RAY_MASK_BLOCK_PICK_UP);

		if (intersectObj == nullptr || intersectDist > *buttonIntersectDist)
		{
			return INTERACT_PRIORITY;
		}
	}

	return 0;
}

std::optional<InteractControlHint> PushButtonEnt::GetInteractControlHint() const
{
	InteractControlHint hint;
	hint.keyBinding = &settings.keyInteract;
	hint.message = "Push";
	return hint;
}

const void* PushButtonEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(ActivatorComp))
		return &m_activator;
	return nullptr;
}

void PushButtonEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::PushButtonEntity buttonPB;

	buttonPB.set_dir(static_cast<iomomi_pb::Dir>(m_direction));
	buttonPB.set_rotation(m_rotation);
	SerializePos(buttonPB, m_position);

	buttonPB.set_activation_delay(m_activationDelay);
	buttonPB.set_activation_duration(m_activationDuration);
	buttonPB.set_allocated_activator(m_activator.SaveProtobuf(nullptr));

	buttonPB.SerializeToOstream(&stream);
}

void PushButtonEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::PushButtonEntity buttonPB;
	buttonPB.ParseFromIstream(&stream);

	m_direction = static_cast<Dir>(buttonPB.dir());
	m_position = DeserializePos(buttonPB);
	m_rotation = buttonPB.rotation();
	m_activator.LoadProtobuf(buttonPB.activator());
	m_activationDelay = buttonPB.activation_delay();
	m_activationDuration = buttonPB.activation_duration();
}
