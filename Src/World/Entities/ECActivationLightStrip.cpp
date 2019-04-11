#include "ECActivationLightStrip.hpp"
#include "ECActivator.hpp"
#include "ECActivatable.hpp"
#include "../World.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../Vec3Compare.hpp"
#include "ECWallMounted.hpp"

#include <tuple>

eg::MessageReceiver ECActivationLightStrip::MessageReceiver =
	eg::MessageReceiver::Create<ECActivationLightStrip, DrawMessage, EditorDrawMessage>();

eg::EntitySignature EntitySignature = eg::EntitySignature::Create<ECActivationLightStrip>();

static eg::EntitySignature activatorLightStripSig = eg::EntitySignature::Create<ECActivator, ECActivationLightStrip>();

const eg::Model* ECActivationLightStrip::s_models[MV_Count];
const eg::IMaterial* ECActivationLightStrip::s_materials[MV_Count];

const eg::ColorLin ACTIVATED_COLOR = eg::ColorLin(eg::ColorSRGB::FromHex(0x4bf863)).ScaleRGB(4);
const eg::ColorLin DEACTIVATED_COLOR = eg::ColorLin(eg::ColorSRGB::FromHex(0xf84b5e)).ScaleRGB(4);

ECActivationLightStrip::ECActivationLightStrip()
{
	
}

void ECActivationLightStrip::OnInit()
{
	s_models[MV_Straight] = &eg::GetAsset<eg::Model>("Models/ActivationStripStraight.obj");
	s_materials[MV_Straight] = &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml");
}

void ECActivationLightStrip::OnShutdown()
{
	
}

EG_ON_INIT(ECActivationLightStrip::OnInit)
EG_ON_SHUTDOWN(ECActivationLightStrip::OnShutdown)

void ECActivationLightStrip::Draw(eg::MeshBatch& meshBatch)
{
	if (m_transitionDirection == -1)
	{
		m_material.transitionProgress = m_maxTransitionProgress - m_transitionProgress;
		m_material.color1 = DEACTIVATED_COLOR;
		m_material.color2 = ACTIVATED_COLOR;
	}
	else
	{
		m_material.transitionProgress = m_transitionProgress;
		m_material.color1 = ACTIVATED_COLOR;
		m_material.color2 = DEACTIVATED_COLOR;
	}
	
	for (int v = 0; v < MV_Count; v++)
	{
		for (const LightStripMaterial::InstanceData& instanceData : m_instances[v])
		{
			int lightMaterialIndex = s_models[v]->GetMaterialIndex("Light");
			for (size_t i = 0; i < s_models[v]->NumMeshes(); i++)
			{
				if (s_models[v]->GetMesh(i).materialIndex == lightMaterialIndex)
				{
					meshBatch.Add(*s_models[v], i, m_material, instanceData);
				}
				else
				{
					meshBatch.Add(*s_models[v], i, *s_materials[v], instanceData.transform);
				}
			}
		}
	}
}

void ECActivationLightStrip::HandleMessage(eg::Entity& entity, const DrawMessage& message)
{
	Draw(*message.meshBatch);
}

void ECActivationLightStrip::HandleMessage(eg::Entity& entity, const EditorDrawMessage& message)
{
	Draw(*message.meshBatch);
}

void ECActivationLightStrip::Update(eg::EntityManager& entityManager, float dt)
{
	const float TRANSITION_SPEED = 100;
	const float TRANSITION_MARGIN = 0.5f;
	const float REVERT_MARGIN = 2.0f;
	
	for (eg::Entity& entity : entityManager.GetEntitySet(activatorLightStripSig))
	{
		ECActivationLightStrip& lightStrip = entity.GetComponent<ECActivationLightStrip>();
		
		if (lightStrip.m_transitionDirection == 0 || lightStrip.m_transitionProgress < REVERT_MARGIN ||
		    lightStrip.m_transitionProgress > lightStrip.m_maxTransitionProgress - REVERT_MARGIN)
		{
			lightStrip.m_transitionDirection = (entity.GetComponent<ECActivator>().IsActivated() ? 1 : -1);
		}
		
		lightStrip.m_transitionProgress += lightStrip.m_transitionDirection * dt * TRANSITION_SPEED;
		const float maxTransitionProgress = lightStrip.m_maxTransitionProgress + TRANSITION_MARGIN;
		
		if (lightStrip.m_transitionDirection == 1 && lightStrip.m_transitionProgress > maxTransitionProgress)
		{
			lightStrip.m_transitionProgress = maxTransitionProgress;
			lightStrip.m_transitionDirection = 0;
		}
		if (lightStrip.m_transitionDirection == -1 && lightStrip.m_transitionProgress < -TRANSITION_MARGIN)
		{
			lightStrip.m_transitionProgress = -TRANSITION_MARGIN;
			lightStrip.m_transitionDirection = 0;
		}
	}
}

struct NodeDistance
{
	int directionChanges;
	int distance;
	
	bool operator==(const NodeDistance& other) const
	{
		return directionChanges == other.directionChanges && distance == other.distance;
	}
	
	bool operator!=(const NodeDistance& other) const
	{
		return !operator==(other);
	}
	
	bool operator<(const NodeDistance& other) const
	{
		return std::make_tuple(distance, directionChanges) <
		       std::make_tuple(other.distance, other.directionChanges);
	}
};

struct NodePos
{
	Dir wallNormal;
	Dir stepDirection;
	glm::ivec3 doublePos;
	
	bool operator==(const NodePos& other) const
	{
		return wallNormal == other.wallNormal && stepDirection == other.stepDirection && doublePos == other.doublePos;
	}
	
	bool operator!=(const NodePos& other) const
	{
		return !operator==(other);
	}
	
	bool operator<(const NodePos& other) const
	{
		return std::make_tuple((int)wallNormal, (int)stepDirection, doublePos.x, doublePos.y, doublePos.z) <
		       std::make_tuple((int)other.wallNormal, (int)other.stepDirection, other.doublePos.x, other.doublePos.y, other.doublePos.z);
	}
};

struct QueueEntry
{
	NodeDistance distance;
	NodePos pos;
	
	bool operator<(const QueueEntry& other) const
	{
		return other.distance < distance;
	}
};

inline glm::ivec3 GetDoublePos(const glm::vec3& pos)
{
	return glm::ivec3(glm::round(pos * 2.0f));
}

static const Dir ORTHO_DIRS[3][4] =
{
	{ Dir::PosY, Dir::NegY, Dir::PosZ, Dir::NegZ },
	{ Dir::PosX, Dir::NegX, Dir::PosZ, Dir::NegZ },
	{ Dir::PosY, Dir::NegY, Dir::PosX, Dir::NegX },
};

void ECActivationLightStrip::Generate(const World& world, eg::Span<const PathPoint> points)
{
	struct NodeData
	{
		NodeDistance minDist;
		NodePos prevPos;
	};
	
	std::map<NodePos, NodeData> nodeData;
	std::priority_queue<QueueEntry, std::vector<QueueEntry>> pq;
	
	const NodeDistance initialDistance = { 0, 0 };
	
	auto IsValidPosition = [&] (const NodePos& pos)
	{
		if (pos.doublePos[(int)pos.wallNormal / 2] % 2)
			return false;
		
		glm::ivec3 dirN = DirectionVector(pos.wallNormal);
		glm::ivec3 dirU = glm::abs(DirectionVector(pos.stepDirection));
		glm::ivec3 dirV;
		if (((int)pos.stepDirection / 2 + 1) % 3 != (int)pos.wallNormal / 2)
			dirV = glm::abs(DirectionVector((Dir)(((int)pos.stepDirection + 2) % 6)));
		else
			dirV = glm::abs(DirectionVector((Dir)(((int)pos.stepDirection + 4) % 6)));
		
		bool isAir[2][2][2];
		for (int du = 0; du < 2; du++)
		{
			for (int dv = 0; dv < 2; dv++)
			{
				for (int dn = 0; dn < 2; dn++)
				{
					glm::ivec3 actPos = glm::floor(glm::vec3(pos.doublePos - dirU * du - dirV * dv + dirN * (dn * 2 - 1)) / 2.0f);
					isAir[du][dv][dn] = world.IsAir(actPos);
				}
			}
		}
		
		return
			((isAir[0][0][1] && isAir[0][1][1]) || (isAir[1][0][1] && isAir[1][1][1])) &&
			((!isAir[0][0][0] && !isAir[0][1][0]) || (!isAir[1][0][0] && !isAir[1][1][0]));
	};
	
	for (size_t i = 1; i < points.size(); i++)
	{
		glm::ivec3 sourcePos = GetDoublePos(points[i - 1].position);
		glm::ivec3 targetPos = GetDoublePos(points[i].position);
		
		nodeData.clear();
		
		//Inserts the four source nodes into the priority queue and node data map
		for (Dir orthoDir : ORTHO_DIRS[(int)points[i - 1].wallNormal / 2])
		{
			NodePos nodePos;
			nodePos.wallNormal = points[i - 1].wallNormal;
			nodePos.stepDirection = orthoDir;
			nodePos.doublePos = sourcePos;
			
			nodeData.emplace(nodePos, NodeData{ initialDistance });
			pq.push({ initialDistance, nodePos });
		}
		
		//Dijkstra's algorithm
		NodePos lastPos;
		while (!pq.empty())
		{
			QueueEntry curEntry = pq.top();
			pq.pop();
			
			if (curEntry.pos.doublePos == targetPos && curEntry.pos.wallNormal == points[i].wallNormal)
			{
				lastPos = curEntry.pos;
				break;
			}
			
			if (curEntry.distance != nodeData.at(curEntry.pos).minDist)
				continue;
			
			auto ProcessEdge = [&] (const NodePos& nextPos, int deltaDirChange, int deltaDist)
			{
				if (!IsValidPosition(nextPos))
					return;
				
				NodeDistance nextDist = curEntry.distance;
				nextDist.directionChanges += deltaDirChange;
				nextDist.distance += deltaDist;
				
				auto dataIt = nodeData.find(nextPos);
				if (dataIt == nodeData.end())
				{
					nodeData.emplace(nextPos, NodeData { nextDist, curEntry.pos });
					pq.push({ nextDist, nextPos });
				}
				else if (nextDist < dataIt->second.minDist)
				{
					dataIt->second.minDist = nextDist;
					dataIt->second.prevPos = curEntry.pos;
					pq.push({ nextDist, nextPos });
				}
			};
			
			//Processes edges due to changing step direction
			for (Dir orthoDir : ORTHO_DIRS[(int)curEntry.pos.wallNormal / 2])
			{
				if (orthoDir != curEntry.pos.stepDirection)
				{
					NodePos nextPos = curEntry.pos;
					nextPos.stepDirection = orthoDir;
					ProcessEdge(nextPos, 1, 0);
				}
			}
			
			//Processes edges due to switching to the normal's direction
			for (int n = 0; n < 2; n++)
			{
				for (int s = 0; s < 2; s++)
				{
					NodePos nextPos;
					nextPos.doublePos = curEntry.pos.doublePos;
					nextPos.wallNormal = n ? OppositeDir(curEntry.pos.stepDirection) : curEntry.pos.stepDirection;
					nextPos.stepDirection = s ? OppositeDir(curEntry.pos.wallNormal) : curEntry.pos.wallNormal;
					ProcessEdge(nextPos, 1, 0);
				}
			}
			
			//Processes the edge from continuing to move in the step direction
			NodePos nextStepPos = curEntry.pos;
			nextStepPos.doublePos += DirectionVector(curEntry.pos.stepDirection);
			ProcessEdge(nextStepPos, 0, 1);
		}
		
		ClearGeneratedMesh();
		if (pq.empty())
		{
			eg::Log(eg::LogLevel::Warning, "misc", "Unable to find path for activation light strip!");
			return;
		}
		
		//Reconstructs the path
		int accDistance = 0;
		for (NodePos pos = lastPos; pos.doublePos != sourcePos || pos.wallNormal != points[i - 1].wallNormal;)
		{
			NodePos prevPos = nodeData.at(pos).prevPos;
			
			LightStripMaterial::InstanceData* instanceData = nullptr;
			
			if (pos.stepDirection == prevPos.stepDirection && pos.wallNormal == prevPos.wallNormal)
			{
				glm::vec3 rotationY = DirectionVector(pos.wallNormal);
				glm::vec3 rotationZ = DirectionVector(pos.stepDirection);
				glm::vec3 rotationX = glm::cross(rotationY, rotationZ);
				
				const float SCALE = 0.25f;
				glm::mat3 rotationScaleMatrix(rotationX * SCALE, rotationY * SCALE, rotationZ * SCALE);
				
				glm::vec3 midPos = glm::vec3(pos.doublePos + prevPos.doublePos) / 4.0f;
				
				instanceData = &m_instances[MV_Straight].emplace_back();
				instanceData->transform = glm::translate(glm::mat4(1), midPos) * glm::mat4(rotationScaleMatrix);
			}
			
			if (instanceData != nullptr)
			{
				instanceData->lightProgress = accDistance;
			}
			
			accDistance++;
			pos = prevPos;
		}
		
		m_maxTransitionProgress = accDistance + 1;
		for (int v = 0; v < MV_Count; v++)
		{
			for (LightStripMaterial::InstanceData& instanceData : m_instances[v])
				instanceData.lightProgress = accDistance - instanceData.lightProgress;
		}
	}
}

void ECActivationLightStrip::GenerateForActivator(const World& world, eg::Entity& activatorEntity)
{
	ECActivationLightStrip* lightStrip = activatorEntity.FindComponent<ECActivationLightStrip>();
	if (lightStrip == nullptr)
		return;
	
	const ECActivator* activator = activatorEntity.FindComponent<ECActivator>();
	if (activator == nullptr)
		return;
	
	eg::Entity* activatableEntity = ECActivatable::FindByName(world.EntityManager(), activator->activatableName);
	if (activatableEntity == nullptr)
		return;
	
	std::vector<glm::vec3> connectionPoints = ECActivatable::GetConnectionPoints(*activatableEntity);
	
	PathPoint points[2];
	points[0].position = eg::GetEntityPosition(activatorEntity);
	points[0].wallNormal = activatorEntity.GetComponent<ECWallMounted>().wallUp;
	points[1].position = connectionPoints[std::min<size_t>(activator->sourceIndex, connectionPoints.size() - 1)];
	points[1].wallNormal = activatableEntity->GetComponent<ECWallMounted>().wallUp;
	
	lightStrip->Generate(world, points);
}

void ECActivationLightStrip::GenerateAll(const World& world)
{
	for (eg::Entity& entity : world.EntityManager().GetEntitySet(activatorLightStripSig))
	{
		GenerateForActivator(world, entity);
	}
}

void ECActivationLightStrip::ClearGeneratedMesh()
{
	for (int v = 0; v < MV_Count; v++)
	{
		m_instances[v].clear();
	}
}
