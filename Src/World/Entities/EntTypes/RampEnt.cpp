#include "RampEnt.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Graphics/WallShader.hpp"
#include "../../../../Protobuf/Build/Ramp.pb.h"
#include "../../Collision.hpp"

#include <imgui.h>
#include <glm/glm.hpp>

static const glm::vec3 untransformedPositions[4] = 
{
	{ -1, -1, -1 }, { 1, -1, -1 }, { -1, 1, 1 }, { 1, 1, 1 }
};
static const int indicesNormal[6] = { 0, 1, 2, 1, 3, 2 };
static const int indicesFlipped[6] = { 0, 2, 1, 1, 2, 3 };
static const glm::vec2 uvs[4] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };

struct RampMaterial
{
	const char* name;
	const StaticPropMaterial* material;
	int textureRepeatsV = 1;
	bool twoSided = false;
	
	RampMaterial(const char* _name, const StaticPropMaterial* _material)
		: name(_name), material(_material) { }
};

static DecalMaterial* edgeDecalMaterial;
static std::vector<RampMaterial> rampMaterials;

float* rampDecalSize = eg::TweakVarFloat("ramp_decal_size", 0.4f, 0.0f);

static void Initialize()
{
	edgeDecalMaterial = &eg::GetAsset<DecalMaterial>("Decals/RampEdge.yaml");
	
	rampMaterials.emplace_back(
		"Ramp", &eg::GetAsset<StaticPropMaterial>("Materials/Ramp.yaml")
	);
	rampMaterials.emplace_back(
		"Grating", &eg::GetAsset<StaticPropMaterial>("Materials/Platform.yaml")
	);
	rampMaterials.emplace_back(
		"Grating 2", &eg::GetAsset<StaticPropMaterial>("Materials/Grating2.yaml")
	);
	
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
	if (edgeDecalMaterial == nullptr)
	{
		Initialize();
	}
	
	m_physicsObject.canBePushed = false;
	m_physicsObject.owner = this;
}

void RampEnt::RenderSettings()
{
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
	
	if (ImGui::Checkbox("Edge Decals", &m_hasEdgeDecals))
		m_meshOutOfDate = true;
	
	ImGui::DragFloat("Texture Scale", &m_textureScale, 0.5f, 0.0f, INFINITY);
	ImGui::Checkbox("Stretch Texture V", &m_stretchTextureV);
}

void RampEnt::CommonDraw(const EntDrawArgs& args)
{
	InitializeVertexBuffer();
	
	glm::vec2 textureScale(m_textureScale, m_rampLength * m_textureScale);
	if (m_stretchTextureV)
	{
		float roundPrecision = rampMaterials[m_material].material->TextureScale().x;
		textureScale.y = std::max(std::round(textureScale.y / roundPrecision), 1.0f) * roundPrecision;
	}
	
	eg::MeshBatch::Mesh mesh;
	mesh.firstIndex = 0;
	mesh.firstVertex = 0;
	mesh.numElements = 6;
	mesh.vertexBuffer = m_vertexBuffer;
	args.meshBatch->Add(mesh, *rampMaterials[m_material].material,
		StaticPropMaterial::InstanceData(glm::mat4(1), textureScale));
	
	if (m_hasEdgeDecals)
	{
		for (const DecalMaterial::InstanceData& decalInstance : m_edgeDecalInstances)
		{
			args.meshBatch->Add(DecalMaterial::GetMesh(), *edgeDecalMaterial, decalInstance, 1);
		}
	}
}

void RampEnt::InitializeVertexBuffer()
{
	if (!m_meshOutOfDate && m_vertexBuffer.handle)
		return;
	m_meshOutOfDate = false;
	
	if (!m_vertexBuffer.handle)
	{
		m_vertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer | eg::BufferFlags::Update,
			sizeof(eg::StdVertex) * 6, nullptr);
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
	
	eg::StdVertex vertices[6] = { };
	const int* indices = m_flipped ? indicesFlipped : indicesNormal;
	for (int i = 0; i < 6; i++)
	{
		*reinterpret_cast<glm::vec3*>(vertices[i].position) = transformedPos[indices[i]];
		for (int j = 0; j < 3; j++)
		{
			vertices[i].normal[j] = eg::FloatToSNorm(normal[j]);
			vertices[i].tangent[j] = eg::FloatToSNorm(aDir[j]);
		}
		vertices[i].texCoord[0] = uvs[indices[i]].x * m_size.x;
		vertices[i].texCoord[1] = uvs[indices[i]].y; //will be scaled in the shader
	}
	
	eg::DC.UpdateBuffer(m_vertexBuffer, 0, sizeof(vertices), vertices);
	m_vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	
	// ** Generates decal instances for the edge of the ramp **
	m_edgeDecalInstances.clear();
	if (m_hasEdgeDecals)
	{
		const int numDecals = std::max((int)std::round(m_rampLength / *rampDecalSize), 1);
		const float decalWorldSize = m_rampLength / numDecals;
		
		const glm::mat4 decalRotationMatrix = glm::mat4(glm::mat3(
			xDir * decalWorldSize * 0.5f,
			normal * decalWorldSize * 0.5f,
			aDir * decalWorldSize * 0.5f
		));
		
		const glm::mat4 decalTransforms[] = 
		{
			glm::translate({}, transformedPos[0]) * decalRotationMatrix,
			glm::translate({}, transformedPos[1]) * decalRotationMatrix * glm::scale({}, glm::vec3(-1, 1, 1))
		};
		
		for (int y = 1; y < numDecals * 2; y++)
		{
			for (int x = 0; x < 2; x++)
			{
				const glm::mat4 matrix = decalTransforms[x] * glm::translate(glm::mat4(1), glm::vec3(1, 0, y));
				m_edgeDecalInstances.emplace_back(matrix, glm::vec2(1, 0), glm::vec2(0, 1));
			}
		}
	}
}

glm::mat4 RampEnt::GetTransformationMatrix() const
{
	glm::vec3 size = 0.5f * m_size;
	
	glm::mat4 rotationMatrix;
	if (m_rotation < 4)
	{
		rotationMatrix = glm::rotate(glm::mat4(1), (float)m_rotation * eg::HALF_PI, glm::vec3(0, 1, 0));
	}
	else if (m_rotation == 4)
	{
		rotationMatrix = glm::rotate(glm::mat4(1), -eg::HALF_PI, glm::vec3(0, 0, 1));
	}
	else if (m_rotation == 5)
	{
		rotationMatrix = glm::rotate(glm::mat4(1), eg::HALF_PI, glm::vec3(0, 0, 1));
	}
	
	return glm::translate(glm::mat4(1), m_position) * rotationMatrix * glm::scale(glm::mat4(1), size);
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
	gravity_pb::RampEntity rampPB;
	
	SerializePos(rampPB, m_position);
	
	rampPB.set_yaw(m_rotation);
	rampPB.set_flipped(m_flipped);
	rampPB.set_scalex(m_size.x);
	rampPB.set_scaley(m_size.y);
	rampPB.set_scalez(m_size.z);
	rampPB.set_texture_scale(m_textureScale);
	rampPB.set_stretch_texture_v(m_stretchTextureV);
	rampPB.set_no_edge_decals(!m_hasEdgeDecals);
	rampPB.set_material(eg::HashFNV1a32(rampMaterials[m_material].name));
	
	rampPB.SerializeToOstream(&stream);
}

void RampEnt::Deserialize(std::istream& stream)
{
	gravity_pb::RampEntity rampPB;
	rampPB.ParseFromIstream(&stream);
	
	m_rotation = rampPB.yaw();
	m_flipped = rampPB.flipped();
	m_size.x = rampPB.scalex();
	m_size.y = rampPB.scaley();
	m_size.z = rampPB.scalez();
	m_position = DeserializePos(rampPB);
	m_textureScale = rampPB.texture_scale();
	m_stretchTextureV = rampPB.stretch_texture_v();
	m_hasEdgeDecals = !rampPB.no_edge_decals();
	
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
	
	m_collisionMesh = eg::CollisionMesh::CreateV3(vertices,
		eg::Span<const int>(m_flipped ? indicesFlipped : indicesNormal, std::size(indicesNormal)));
	m_collisionMesh.FlipWinding();
	m_physicsObject.shape = &m_collisionMesh;
}

void RampEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
}

void RampEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	m_meshOutOfDate = true;
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
	return clone;
}
