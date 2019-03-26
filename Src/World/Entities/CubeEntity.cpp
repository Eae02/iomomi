#include "CubeEntity.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"

static constexpr float MASS = 10.0f;
static constexpr float RADIUS = 0.4f;

static std::unique_ptr<btCollisionShape> s_collisionShape;
static btVector3 s_localInertia;

static eg::Model* s_model;
static eg::IMaterial* s_material;

static void OnInit()
{
	s_collisionShape = std::make_unique<btBoxShape>(btVector3(RADIUS, RADIUS, RADIUS));
	s_collisionShape->calculateLocalInertia(MASS, s_localInertia);
	
	s_model = &eg::GetAsset<eg::Model>("Models/Cube.obj");
	s_material = &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml");
}

EG_ON_INIT(OnInit)

CubeEntity::CubeEntity()
	: m_rigidBody(MASS, &m_motionState, s_collisionShape.get(), s_localInertia)
{
	
}

void CubeEntity::EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal)
{
	SetPosition(wallPosition + glm::vec3(DirectionVector(wallNormal)) * (RADIUS + 0.01f));
}

void CubeEntity::Draw(eg::MeshBatch& meshBatch)
{
	btTransform transform;
	m_motionState.getWorldTransform(transform);
	
	glm::mat4 worldMatrix;
	transform.getOpenGLMatrix(reinterpret_cast<float*>(&worldMatrix));
	worldMatrix *= glm::scale(glm::mat4(1), glm::vec3(RADIUS));
	
	meshBatch.Add(*s_model, *s_material, worldMatrix);
}

void CubeEntity::Update(const Entity::UpdateArgs& args)
{
	if (m_rigidBody.getLinearVelocity().length2() > 1E-4f || m_rigidBody.getAngularVelocity().length2() > 1E-4f)
	{
		btTransform transform;
		m_motionState.getWorldTransform(transform);
		
		args.invalidateShadows(eg::Sphere(bullet::ToGLM(transform.getOrigin()), RADIUS * std::sqrt(3.0f)));
	}
}

void CubeEntity::EditorDraw(bool selected, const Entity::EditorDrawArgs& drawArgs) const
{
	glm::mat4 worldMatrix =
		glm::translate(glm::mat4(1), Position()) *
		glm::scale(glm::mat4(1), glm::vec3(RADIUS));
	
	drawArgs.meshBatch->Add(*s_model, *s_material, worldMatrix);
}

void CubeEntity::Save(YAML::Emitter& emitter) const
{
	Entity::Save(emitter);
}

void CubeEntity::Load(const YAML::Node& node)
{
	Entity::Load(node);
	
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(bullet::FromGLM(Position()));
	m_motionState.setWorldTransform(transform);
	m_rigidBody.setWorldTransform(transform);
	
	m_rigidBody.setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
	m_rigidBody.setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
	m_rigidBody.clearForces();
}

btRigidBody* CubeEntity::GetRigidBody()
{
	return &m_rigidBody;
}
