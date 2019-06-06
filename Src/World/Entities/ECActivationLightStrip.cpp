#include "ECActivationLightStrip.hpp"
#include "ECActivator.hpp"
#include "ECActivatable.hpp"
#include "ECWallMounted.hpp"
#include "../World.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../Vec3Compare.hpp"

#include <unordered_map>
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
	s_models[MV_Bend] = &eg::GetAsset<eg::Model>("Models/ActivationStripBend.obj");
	s_materials[MV_Bend] = &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml");
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
					meshBatch.AddModelMesh(*s_models[v], i, m_material, instanceData);
				}
				else
				{
					meshBatch.AddModelMesh(*s_models[v], i, *s_materials[v], instanceData.transform);
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
};

struct NodePosHash
{
	size_t operator()(const NodePos& n) const noexcept
	{
		size_t hash = 0;
		eg::HashAppend(hash, (int)n.stepDirection);
		eg::HashAppend(hash, (int)n.wallNormal);
		eg::HashAppend(hash, (int)n.doublePos.x);
		eg::HashAppend(hash, (int)n.doublePos.y);
		eg::HashAppend(hash, (int)n.doublePos.z);
		return hash;
	}
};

struct QueueEntry
{
	int distance;
	int distanceH;
	NodePos pos;
	
	bool operator<(const QueueEntry& other) const
	{
		return other.distanceH < distanceH;
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
		int minDist;
		NodePos prevPos;
	};
	
	std::unordered_map<NodePos, NodeData, NodePosHash> nodeData;
	std::priority_queue<QueueEntry, std::vector<QueueEntry>> pq;
	
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
	
	m_maxTransitionProgress = 0;
	ClearGeneratedMesh();
	
	for (size_t i = 1; i < points.size(); i++)
	{
		glm::ivec3 sourcePos = GetDoublePos(points[i - 1].position);
		glm::ivec3 targetPos = GetDoublePos(points[i].position);
		
		auto QueuePush = [&] (int dist, const NodePos& pos)
		{
			glm::ivec3 sDelta = glm::abs(pos.doublePos - targetPos);
			int heuristic = sDelta.x + sDelta.y + sDelta.z;
			pq.push({ dist, dist + heuristic, pos });
		};
		
		nodeData.clear();
		pq = { };
		
		//Inserts the four source nodes into the priority queue and node data map
		for (Dir orthoDir : ORTHO_DIRS[(int)points[i - 1].wallNormal / 2])
		{
			NodePos nodePos;
			nodePos.wallNormal = points[i - 1].wallNormal;
			nodePos.stepDirection = orthoDir;
			nodePos.doublePos = sourcePos;
			
			nodeData.emplace(nodePos, NodeData{ 0 });
			QueuePush(0, nodePos);
		}
		
		//A*
		NodePos lastPos;
		while (!pq.empty())
		{
			QueueEntry curEntry = pq.top();
			pq.pop();
			
			if (curEntry.pos.doublePos == targetPos &&
			    (i == points.size() - 1 || curEntry.pos.wallNormal == points[i].wallNormal))
			{
				lastPos = curEntry.pos;
				break;
			}
			
			if (curEntry.distance != nodeData.at(curEntry.pos).minDist)
				continue;
			
			int nextDist = curEntry.distance + 1;
			
			auto ProcessEdge = [&] (const NodePos& nextPos)
			{
				if (!IsValidPosition(nextPos))
					return;
				
				auto dataIt = nodeData.find(nextPos);
				if (dataIt == nodeData.end())
				{
					nodeData.emplace(nextPos, NodeData { nextDist, curEntry.pos });
					QueuePush(nextDist, nextPos);
				}
				else if (nextDist < dataIt->second.minDist)
				{
					dataIt->second.minDist = nextDist;
					dataIt->second.prevPos = curEntry.pos;
					QueuePush(nextDist, nextPos);
				}
			};
			
			//Processes edges due to changing step direction
			for (Dir orthoDir : ORTHO_DIRS[(int)curEntry.pos.wallNormal / 2])
			{
				if (orthoDir != curEntry.pos.stepDirection)
				{
					NodePos nextPos = curEntry.pos;
					nextPos.stepDirection = orthoDir;
					ProcessEdge(nextPos);
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
					ProcessEdge(nextPos);
				}
			}
			
			//Processes the edge from continuing to move in the step direction
			NodePos nextStepPos = curEntry.pos;
			nextStepPos.doublePos += DirectionVector(curEntry.pos.stepDirection);
			ProcessEdge(nextStepPos);
		}
		
		if (pq.empty())
		{
			eg::Log(eg::LogLevel::Warning, "misc", "Unable to find path for activation light strip!");
			return;
		}
		
		//Returns whether the given node has a previous node
		auto HasPrev = [&] (const NodePos& pos)
		{
			return pos.doublePos != sourcePos || pos.wallNormal != points[i - 1].wallNormal;
		};
		
		//Walks through the found path backwards and constructs the list of mesh instances.
		int accDistance = 0;
		for (NodePos pos = lastPos; HasPrev(pos);)
		{
			NodePos prevPos = nodeData.at(pos).prevPos;
			
			LightStripMaterial::InstanceData* instanceData = nullptr;
			
			NodePos prev4[4];
			prev4[0] = pos;
			prev4[1] = prevPos;
			int numPrev = 2;
			while (HasPrev(prev4[numPrev - 1]) && numPrev < 4)
			{
				prev4[numPrev] = nodeData.at(prev4[numPrev - 1]).prevPos;
				numPrev++;
			}
			
			glm::vec3 rotationY = DirectionVector(pos.wallNormal);
			glm::vec3 rotationZ = DirectionVector(pos.stepDirection);
			glm::vec3 rotationX, translation;
			
			//If a bend mesh is to be inserted, the wall normal for the previous 4 nodes must all be the same and the
			// step direction must be the same for p0 and p1 and also the same for p2 and p3 (but different from p0 and p1).
			if (numPrev == 4 && prev4[0].wallNormal == prev4[1].wallNormal &&
			    prev4[1].wallNormal == prev4[2].wallNormal && prev4[2].wallNormal == prev4[3].wallNormal &&
			    prev4[0].stepDirection == prev4[1].stepDirection && prev4[2].stepDirection == prev4[3].stepDirection && 
			    prev4[1].stepDirection != prev4[2].stepDirection)
			{
				rotationX = -DirectionVector(prev4[2].stepDirection);
				
				//If rotationZ cross rotationX isn't aligned with rotationY, the winding will be wrong.
				// So swap rotationX and rotationZ in this case.
				if (glm::dot(glm::cross(rotationZ, rotationX), rotationY) < 0)
					std::swap(rotationX, rotationZ);
				
				translation = glm::vec3(prevPos.doublePos) / 2.0f;
				instanceData = &m_instances[MV_Bend].emplace_back();
				pos = prev4[2];
			}
			else if (pos.stepDirection == prevPos.stepDirection && pos.wallNormal == prevPos.wallNormal)
			{
				rotationX = glm::cross(rotationY, rotationZ);
				translation = glm::vec3(pos.doublePos + prevPos.doublePos) / 4.0f;
				instanceData = &m_instances[MV_Straight].emplace_back();
			}
			
			if (instanceData != nullptr)
			{
				const float SCALE = 0.25f;
				glm::mat3 rotationScaleMatrix(rotationX * SCALE, rotationY * SCALE, rotationZ * SCALE);
				instanceData->transform = glm::translate(glm::mat4(1), translation) * glm::mat4(rotationScaleMatrix);
				instanceData->lightProgress = accDistance;
				accDistance++;
			}
			
			pos = nodeData.at(pos).prevPos;
		}
		
		for (int v = 0; v < MV_Count; v++)
		{
			for (LightStripMaterial::InstanceData& instanceData : m_instances[v])
				instanceData.lightProgress = m_maxTransitionProgress + accDistance - instanceData.lightProgress;
		}
		m_maxTransitionProgress += accDistance + 1;
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
	points[1].position = connectionPoints[std::min<size_t>(activator->targetConnectionIndex, connectionPoints.size() - 1)];
	
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
