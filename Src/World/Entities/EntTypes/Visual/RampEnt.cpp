#include "RampEnt.hpp"

#include <glm/glm.hpp>

#include "../../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../Graphics/WallShader.hpp"
#include "../../../../ImGui.hpp"
#include "../../../Collision.hpp"
#include <Ramp.pb.h>

DEF_ENT_TYPE(RampEnt)

static const glm::vec3 untransformedPositions[4] = { { -1, -1, -1 }, { 1, -1, -1 }, { -1, 1, 1 }, { 1, 1, 1 } };
static const uint32_t indicesNormal[6] = { 0, 1, 2, 1, 3, 2 };
static const uint32_t indicesFlipped[6] = { 0, 2, 1, 1, 2, 3 };
static const glm::vec2 uvs[4] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };

struct RampMaterial
{
	const char* name;
	const StaticPropMaterial* material;
	int textureRepeatsV = 1;
	bool twoSided = false;

	RampMaterial(const char* _name, const StaticPropMaterial* _material) : name(_name), material(_material) {}
};

static std::vector<RampMaterial> rampMaterials;

// Cannot be called by EG_ON_INIT because wallMaterials need to be initialized before
static void Initialize()
{
	rampMaterials.emplace_back("Ramp", &eg::GetAsset<StaticPropMaterial>("Materials/Ramp.yaml"));
	rampMaterials.emplace_back("Grating", &eg::GetAsset<StaticPropMaterial>("Materials/Platform.yaml"));
	rampMaterials.emplace_back("Grating 2", &eg::GetAsset<StaticPropMaterial>("Materials/Grating2.yaml"));

	for (uint32_t i = 1; i < MAX_WALL_MATERIALS; i++)
	{
		if (wallMaterials[i].initialized)
		{
			rampMaterials.emplace_back(wallMaterials[i].name, &StaticPropMaterial::GetFromWallMaterial(i));
		}
	}
}

RampEnt::RampEnt()
{
	m_physicsObject.canBePushed = false;
	m_physicsObject.owner = this;

	if (rampMaterials.empty())
	{
		Initialize();
	}
}

void RampEnt::RenderSettings()
{
#ifdef EG_HAS_IMGUI
	Ent::RenderSettings();

	if (ImGui::DragFloat3("Size", &m_size.x))
		m_meshOutOfDate = true;
	if (ImGui::SliderInt("Rotation", &m_rotation, 0, 5))
		m_meshOutOfDate = true;
	if (ImGui::Button("Flip Normal"))
	{
		m_flipped = !m_flipped;
		m_meshOutOfDate = true;
	}

	ImGui::Separator();

	if (ImGui::BeginCombo("Material", rampMaterials[m_material].name))
	{
		for (uint32_t material = 0; material < rampMaterials.size(); material++)
		{
			if (ImGui::Selectable(rampMaterials[material].name, m_material == material))
			{
				m_material = material;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::DragFloat("Texture Scale", &m_textureScale, 0.5f, 0.0f, INFINITY);
	if (ImGui::SliderInt("Texture Rotation", &m_textureRotation, 0, 4))
		m_meshOutOfDate = true;
	ImGui::Checkbox("Stretch Texture V", &m_stretchTextureV);
#endif
}

void RampEnt::CommonDraw(const EntDrawArgs& args)
{
	InitializeVertexBuffer();

	glm::vec2 textureScale(m_textureScale * m_size.x, m_rampLength * m_textureScale);
	if (m_stretchTextureV)
	{
		float roundPrecision = rampMaterials[m_material].material->TextureScale()[m_textureRotation % 2];
		textureScale.y = std::max(std::round(textureScale.y * roundPrecision), 1.0f) / roundPrecision;
	}
	if (m_textureRotation % 2)
		std::swap(textureScale.x, textureScale.y);

	const eg::MeshBatch::Mesh mesh = {
		.buffersDescriptor = m_meshBuffersDescriptor.get(),
		.numElements = 6,
	};
	args.meshBatch->Add(
		mesh, *rampMaterials[m_material].material, StaticPropMaterial::InstanceData(glm::mat4(1), textureScale)
	);
}

void RampEnt::InitializeVertexBuffer()
{
	if (!m_meshOutOfDate && m_vertexBuffer.handle)
		return;
	m_meshOutOfDate = false;

	if (!m_vertexBuffer.handle)
	{
		m_vertexBuffer =
			eg::Buffer(eg::BufferFlags::VertexBuffer | eg::BufferFlags::Update, sizeof(VertexSoaPXNT<6>), nullptr);

		m_meshBuffersDescriptor = std::make_unique<eg::MeshBuffersDescriptor>();
		m_meshBuffersDescriptor->vertexBuffer = m_vertexBuffer;
		SetVertexStreamOffsetsSoaPXNT(m_meshBuffersDescriptor->vertexStreamOffsets, 6);
	}

	const glm::mat4 transformationMatrix = GetTransformationMatrix();
	const std::array<glm::vec3, 4> transformedPos = GetTransformedVertices(transformationMatrix);

	const glm::vec3 xDir = glm::normalize(transformedPos[1] - transformedPos[0]);
	glm::vec3 aDir = transformedPos[2] - transformedPos[0];
	m_rampLength = glm::length(aDir);
	aDir /= m_rampLength;

	glm::vec3 normal = glm::normalize(glm::cross(aDir, xDir));
	if (m_flipped)
		normal = -normal;

	VertexSoaPXNT<6> vertices;
	const uint32_t* indices = m_flipped ? indicesFlipped : indicesNormal;
	for (int i = 0; i < 6; i++)
	{
		vertices.positions[i] = transformedPos[indices[i]];
		vertices.normalsAndTangents[i][0] = glm::packSnorm4x8(glm::vec4(normal, 0));
		vertices.normalsAndTangents[i][1] = glm::packSnorm4x8(glm::vec4(aDir, 0));

		glm::vec2 texCoord = uvs[indices[i]];
		for (int r = 0; r < m_textureRotation; r++)
			texCoord = glm::vec2(texCoord.y, -texCoord.x);
		vertices.textureCoordinates[i] = texCoord;
	}

	m_vertexBuffer.DCUpdateData(0, sizeof(vertices), &vertices);
	m_vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
}

std::pair<glm::vec3, float> RampEnt::GetAngleAxisRotation() const
{
	switch (m_rotation)
	{
	case 0:
	case 1:
	case 2:
	case 3:
		return { glm::vec3(0, 1, 0), static_cast<float>(m_rotation) * eg::HALF_PI };
	case 4:
		return { glm::vec3(0, 0, 1), -eg::HALF_PI };
	case 5:
		return { glm::vec3(0, 0, 1), eg::HALF_PI };
	default:
		EG_UNREACHABLE
	}
}

glm::mat4 RampEnt::GetTransformationMatrix() const
{
	auto [rotationAxis, rotationAngle] = GetAngleAxisRotation();

	return glm::translate(glm::mat4(1), m_position) * glm::rotate(glm::mat4(1), rotationAngle, rotationAxis) *
	       glm::scale(glm::mat4(1), 0.5f * m_size);
}

std::array<glm::vec3, 4> RampEnt::GetTransformedVertices(const glm::mat4& matrix) const
{
	std::array<glm::vec3, 4> transformedPos;
	for (int i = 0; i < 4; i++)
	{
		transformedPos[i] = matrix * glm::vec4(untransformedPositions[i], 1.0f);
	}
	return transformedPos;
}

void RampEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::RampEntity rampPB;

	SerializePos(rampPB, m_position);

	rampPB.set_yaw(m_rotation);
	rampPB.set_flipped(m_flipped);
	rampPB.set_scalex(m_size.x);
	rampPB.set_scaley(m_size.y);
	rampPB.set_scalez(m_size.z);
	rampPB.set_texture_scale(m_textureScale);
	rampPB.set_stretch_texture_v(m_stretchTextureV);
	rampPB.set_material(eg::HashFNV1a32(rampMaterials[m_material].name));
	rampPB.set_texture_rotation(m_textureRotation);

	rampPB.SerializeToOstream(&stream);
}

void RampEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::RampEntity rampPB;
	rampPB.ParseFromIstream(&stream);

	m_rotation = rampPB.yaw();
	m_flipped = rampPB.flipped();
	m_size.x = rampPB.scalex();
	m_size.y = rampPB.scaley();
	m_size.z = rampPB.scalez();
	m_position = DeserializePos(rampPB);
	m_textureScale = rampPB.texture_scale();
	m_stretchTextureV = rampPB.stretch_texture_v();
	m_textureRotation = rampPB.texture_rotation();

	m_material = 0;
	for (uint32_t i = 0; i < rampMaterials.size(); i++)
	{
		if (eg::HashFNV1a32(rampMaterials[i].name) == rampPB.material())
		{
			m_material = i;
			break;
		}
	}

	if (m_textureScale < 1E-3f)
		m_textureScale = 1;

	m_meshOutOfDate = true;
	std::array<glm::vec3, 4> vertices = GetTransformedVertices(GetTransformationMatrix());

	m_collisionMesh = eg::CollisionMesh(
		vertices, std::span<const uint32_t>(m_flipped ? indicesFlipped : indicesNormal, std::size(indicesNormal))
	);
	m_collisionMesh.FlipWinding();
	m_physicsObject.shape = &m_collisionMesh;
}

void RampEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
}

void RampEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	m_meshOutOfDate = true;
}

std::optional<eg::ColorSRGB> RampEnt::EdGetBoxColor(bool selected) const
{
	if (!selected)
		return {};
	return eg::ColorSRGB::FromHex(0x1a8cc9).ScaleAlpha(0.5f);
}

glm::vec3 RampEnt::EdGetSize() const
{
	return m_size * 0.5f;
}

void RampEnt::EdResized(const glm::vec3& newSize)
{
	m_size = newSize * 2.0f;
}

glm::quat RampEnt::EdGetRotation() const
{
	auto [rotationAxis, rotationAngle] = GetAngleAxisRotation();
	return glm::angleAxis(rotationAngle, rotationAxis);
}

template <>
std::shared_ptr<Ent> CloneEntity<RampEnt>(const Ent& entity)
{
	const RampEnt& src = static_cast<const RampEnt&>(entity);
	std::shared_ptr<RampEnt> clone = Ent::Create<RampEnt>();

	clone->m_rotation = src.m_rotation;
	clone->m_flipped = src.m_flipped;
	clone->m_size = src.m_size;
	clone->m_position = src.m_position;
	clone->m_stretchTextureV = src.m_stretchTextureV;
	clone->m_textureScale = src.m_textureScale;
	clone->m_textureRotation = src.m_textureRotation;
	clone->m_material = src.m_material;

	return clone;
}
