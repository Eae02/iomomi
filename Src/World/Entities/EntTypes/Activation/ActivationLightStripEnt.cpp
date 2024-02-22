#include "ActivationLightStripEnt.hpp"

#include <tuple>
#include <unordered_map>

#include "../../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../Graphics/RenderSettings.hpp"
#include "../../../World.hpp"
#include "../../Components/ActivatableComp.hpp"
#include "../../Components/ActivatorComp.hpp"

DEF_ENT_TYPE(ActivationLightStripEnt)

const eg::Model* ActivationLightStripEnt::s_models[MV_Count];
const eg::IMaterial* ActivationLightStripEnt::s_materials[MV_Count];

const eg::ColorLin ActivationLightStripEnt::ACTIVATED_COLOR =
	eg::ColorLin(eg::ColorSRGB::FromHex(0x4bf863)).ScaleRGB(4);
const eg::ColorLin ActivationLightStripEnt::DEACTIVATED_COLOR =
	eg::ColorLin(eg::ColorSRGB::FromHex(0xf84b5e)).ScaleRGB(4);

void ActivationLightStripEnt::OnInit()
{
	s_models[MV_Straight] = &eg::GetAsset<eg::Model>("Models/ActivationStrip/ActivationStripStraight.obj");
	s_materials[MV_Straight] = &eg::GetAsset<StaticPropMaterial>("Materials/ActivationStrip.yaml");
	s_models[MV_Bend] = &eg::GetAsset<eg::Model>("Models/ActivationStrip/ActivationStripBend.obj");
	s_materials[MV_Bend] = &eg::GetAsset<StaticPropMaterial>("Materials/ActivationStrip.yaml");
	s_models[MV_Corner] = &eg::GetAsset<eg::Model>("Models/ActivationStrip/ActivationStripCorner.obj");
	s_materials[MV_Corner] = &eg::GetAsset<StaticPropMaterial>("Materials/ActivationStrip.yaml");
}

EG_ON_INIT(ActivationLightStripEnt::OnInit)

void ActivationLightStripEnt::RenderSettings()
{
	Ent::RenderSettings();
}

const float MODEL_SCALE = 0.25f;

void ActivationLightStripEnt::CommonDraw(const EntDrawArgs& args)
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
				eg::AABB aabb = s_models[v]->GetMesh(i).boundingAABB->TransformedBoundingBox(instanceData.transform);
				if (!args.frustum->Intersects(aabb))
					continue;

				if (s_models[v]->GetMesh(i).materialIndex == lightMaterialIndex)
				{
					args.meshBatch->AddModelMesh(*s_models[v], i, m_material, instanceData);
				}
				else
				{
					args.meshBatch->AddModelMesh(
						*s_models[v], i, *s_materials[v], StaticPropMaterial::InstanceData(instanceData.transform));
				}
			}
		}
	}
}

std::vector<ActivationLightStripEnt::WayPoint> ActivationLightStripEnt::GetWayPointsWithStartAndEnd() const
{
	std::shared_ptr<Ent> activatorEnt = m_activator.lock();
	std::shared_ptr<Ent> activatableEnt = m_activatable.lock();
	const ActivatorComp* activator = activatorEnt->GetComponent<ActivatorComp>();

	std::vector<WayPoint> points(activator->waypoints.size() + 2);
	points[0].position = activatorEnt->GetPosition();
	points[0].wallNormal = activatorEnt->GetFacingDirection();
	std::copy(activator->waypoints.begin(), activator->waypoints.end(), points.begin() + 1);

	std::vector<glm::vec3> connectionPoints =
		activatableEnt->GetComponent<ActivatableComp>()->GetConnectionPoints(*activatableEnt);

	const size_t lastPointIndex = std::min<size_t>(activator->targetConnectionIndex, connectionPoints.size() - 1);
	points.back().position = connectionPoints[lastPointIndex];

	return points;
}

void ActivationLightStripEnt::SetGenerateResult(GenerateResult& result)
{
	m_instances.swap(result.instances);
	m_path.swap(result.path);
	m_maxTransitionProgress = result.maxTransitionProgress;
}

void ActivationLightStripEnt::Update(const WorldUpdateArgs& args)
{
	const float TRANSITION_SPEED = 100;
	const float TRANSITION_MARGIN = 0.5f;
	const float REVERT_MARGIN = 2.0f;

	std::shared_ptr<Ent> activatorEnt = m_activator.lock();
	if (activatorEnt == nullptr || m_activatable.lock() == nullptr)
	{
		args.world->entManager.RemoveEntity(*this);
		return;
	}

	const ActivatorComp* activator = activatorEnt->GetComponent<ActivatorComp>();

#ifdef __EMSCRIPTEN__
	if (regenerate)
	{
		GenerateResult result = Generate(args.world->voxels, GetWayPointsWithStartAndEnd());
		SetGenerateResult(result);
		regenerate = false;
	}
#else
	if (m_generationFuture.valid() && m_generationFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		GenerateResult result = m_generationFuture.get();
		SetGenerateResult(result);
		m_generationFuture = {};
	}
	if (!m_generationFuture.valid() && regenerate)
	{
		m_generationFuture = std::async(
			std::launch::async, [points = GetWayPointsWithStartAndEnd(), voxels = args.world->voxels,
		                         self = std::dynamic_pointer_cast<ActivationLightStripEnt>(shared_from_this())]()
			{ return self->Generate(voxels, points); });
		regenerate = false;
	}
#endif

	bool activated = activator && activator->IsActivated();

	if (m_transitionDirection == 0 || m_transitionProgress < REVERT_MARGIN ||
	    m_transitionProgress > m_maxTransitionProgress - REVERT_MARGIN)
	{
		m_transitionDirection = (activated ? 1 : -1);
	}

	m_transitionProgress += static_cast<float>(m_transitionDirection) * args.dt * TRANSITION_SPEED;
	const float maxTransitionProgress = m_maxTransitionProgress + TRANSITION_MARGIN;

	if (m_transitionDirection == 1 && m_transitionProgress > maxTransitionProgress)
	{
		m_transitionProgress = maxTransitionProgress;
		m_transitionDirection = 0;
	}
	if (m_transitionDirection == -1 && m_transitionProgress < -TRANSITION_MARGIN)
	{
		m_transitionProgress = -TRANSITION_MARGIN;
		m_transitionDirection = 0;
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

	bool operator!=(const NodePos& other) const { return !operator==(other); }
};

struct NodePosHash
{
	size_t operator()(const NodePos& n) const noexcept
	{
		size_t hash = 0;
		eg::HashAppend(hash, static_cast<int>(n.stepDirection));
		eg::HashAppend(hash, static_cast<int>(n.wallNormal));
		eg::HashAppend(hash, static_cast<int>(n.doublePos.x));
		eg::HashAppend(hash, static_cast<int>(n.doublePos.y));
		eg::HashAppend(hash, static_cast<int>(n.doublePos.z));
		return hash;
	}
};

struct QueueEntry
{
	int distance;
	int distanceH;
	NodePos pos;

	bool operator<(const QueueEntry& other) const { return other.distanceH < distanceH; }
};

inline glm::ivec3 GetDoublePos(const glm::vec3& pos)
{
	return glm::ivec3(glm::round(pos * 2.0f));
}

static const Dir ORTHO_DIRS[3][4] = {
	{ Dir::PosY, Dir::NegY, Dir::PosZ, Dir::NegZ },
	{ Dir::PosX, Dir::NegX, Dir::PosZ, Dir::NegZ },
	{ Dir::PosY, Dir::NegY, Dir::PosX, Dir::NegX },
};

ActivationLightStripEnt::GenerateResult ActivationLightStripEnt::Generate(
	const VoxelBuffer& voxels, std::span<const WayPoint> points)
{
	struct NodeData
	{
		int minDist;
		NodePos prevPos;
	};

	std::unordered_map<NodePos, NodeData, NodePosHash> nodeData;
	std::priority_queue<QueueEntry, std::vector<QueueEntry>> pq;

	auto IsValidPosition = [&](const NodePos& pos)
	{
		if (pos.doublePos[static_cast<int>(pos.wallNormal) / 2] % 2)
			return false;

		glm::ivec3 dirN = DirectionVector(pos.wallNormal);
		glm::ivec3 dirU = glm::abs(DirectionVector(pos.stepDirection));
		glm::ivec3 dirV;
		if ((static_cast<int>(pos.stepDirection) / 2 + 1) % 3 != static_cast<int>(pos.wallNormal) / 2)
			dirV = glm::abs(DirectionVector(static_cast<Dir>((static_cast<int>(pos.stepDirection) + 2) % 6)));
		else
			dirV = glm::abs(DirectionVector(static_cast<Dir>((static_cast<int>(pos.stepDirection) + 4) % 6)));

		bool isAir[2][2][2];
		for (int du = 0; du < 2; du++)
		{
			for (int dv = 0; dv < 2; dv++)
			{
				for (int dn = 0; dn < 2; dn++)
				{
					const glm::ivec3 actPos =
						glm::floor(glm::vec3(pos.doublePos - dirU * du - dirV * dv + dirN * (dn * 2 - 1)) / 2.0f);
					isAir[du][dv][dn] = voxels.IsAir(actPos);
				}
			}
		}

		return ((isAir[0][0][1] && isAir[0][1][1]) || (isAir[1][0][1] && isAir[1][1][1])) &&
		       ((!isAir[0][0][0] && !isAir[0][1][0]) || (!isAir[1][0][0] && !isAir[1][1][0]));
	};

	GenerateResult result;

	std::vector<NodePos> path;
	std::vector<size_t> nodesBeforeWayPoint(points.size());
	nodesBeforeWayPoint[0] = 0;

	NodePos nextStart;
	nextStart.doublePos = GetDoublePos(points[0].position);
	nextStart.wallNormal = points[0].wallNormal;

	for (size_t i = 1; i < points.size(); i++)
	{
		glm::ivec3 sourcePos = nextStart.doublePos;
		glm::ivec3 targetPos = GetDoublePos(points[i].position);

		auto QueuePush = [&](int dist, const NodePos& pos)
		{
			glm::ivec3 sDelta = glm::abs(pos.doublePos - targetPos);
			int heuristic = sDelta.x + sDelta.y + sDelta.z;
			pq.push({ dist, dist + heuristic, pos });
		};

		nodeData.clear();
		pq = {};

		if (i == 1)
		{
			// Inserts the four source nodes into the priority queue and node data map
			for (Dir orthoDir : ORTHO_DIRS[(int)points[i - 1].wallNormal / 2])
			{
				NodePos nodePos = nextStart;
				nodePos.stepDirection = orthoDir;
				nodeData.emplace(nodePos, NodeData{ 0 });
				QueuePush(0, nodePos);
			}
		}
		else
		{
			nodeData.emplace(nextStart, NodeData{ 0 });
			QueuePush(0, nextStart);
		}

		// A*
		NodePos lastPos;
		while (!pq.empty())
		{
			QueueEntry curEntry = pq.top();
			pq.pop();

			if (curEntry.pos.doublePos == targetPos &&
			    (i + 1 == points.size() || curEntry.pos.wallNormal == points[i].wallNormal))
			{
				lastPos = curEntry.pos;
				break;
			}

			if (curEntry.distance != nodeData.at(curEntry.pos).minDist)
				continue;

			int nextDist = curEntry.distance + 1;

			auto ProcessEdge = [&](const NodePos& nextPos)
			{
				if (!IsValidPosition(nextPos))
					return;

				auto dataIt = nodeData.find(nextPos);
				if (dataIt == nodeData.end())
				{
					nodeData.emplace(nextPos, NodeData{ nextDist, curEntry.pos });
					QueuePush(nextDist, nextPos);
				}
				else if (nextDist < dataIt->second.minDist)
				{
					dataIt->second.minDist = nextDist;
					dataIt->second.prevPos = curEntry.pos;
					QueuePush(nextDist, nextPos);
				}
			};

			// Processes edges due to changing step direction
			for (Dir orthoDir : ORTHO_DIRS[static_cast<int>(curEntry.pos.wallNormal) / 2])
			{
				if (orthoDir != curEntry.pos.stepDirection)
				{
					NodePos nextPos = curEntry.pos;
					nextPos.stepDirection = orthoDir;
					ProcessEdge(nextPos);
				}
			}

			// Processes edges due to switching to the normal's direction
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

			// Processes the edge from continuing to move in the step direction
			NodePos nextStepPos = curEntry.pos;
			nextStepPos.doublePos += DirectionVector(curEntry.pos.stepDirection);
			ProcessEdge(nextStepPos);
		}

		if (pq.empty())
		{
			eg::Log(eg::LogLevel::Warning, "misc", "Unable to find path for activation light strip!");
			return {};
		}

		// Returns whether the given node has a previous node
		auto HasPrev = [&](const NodePos& pos)
		{ return pos.doublePos != sourcePos || pos.wallNormal != points[i - 1].wallNormal; };

		// Adds the shortest path to the path list
		size_t prevPathLen = path.size();
		for (NodePos pos = lastPos; HasPrev(pos); pos = nodeData.at(pos).prevPos)
		{
			path.push_back(pos);
		}
		std::reverse(path.begin() + prevPathLen, path.end());

		nodesBeforeWayPoint[i] = path.size();
		nextStart = lastPos;
	}

	result.path.resize(path.size());
	for (size_t i = 0; i < path.size(); i++)
	{
		auto wpi = std::lower_bound(nodesBeforeWayPoint.begin(), nodesBeforeWayPoint.end(), i);
		result.path[i].position = glm::vec3(path[i].doublePos) / 2.0f;
		result.path[i].wallNormal = path[i].wallNormal;
		result.path[i].prevWayPoint = static_cast<int>(wpi - nodesBeforeWayPoint.begin()) - 1;
	}

	// Builds the path mesh
	float accDistance = 0;
	for (int i = (int)path.size() - 1; i >= 1; i--)
	{
		LightStripMaterial::InstanceData* instanceData = nullptr;

		NodePos prev4[4];
		prev4[0] = path[i];
		prev4[1] = path[i - 1];
		int numPrev = 2;
		while (i - numPrev >= 0 && numPrev < 4)
		{
			prev4[numPrev] = path[i - numPrev];
			numPrev++;
		}

		glm::vec3 rotationY = DirectionVector(path[i].wallNormal);
		glm::vec3 rotationZ = DirectionVector(path[i].stepDirection);
		glm::vec3 rotationX, translation;

		// If a bend mesh is to be inserted, the wall normal for the previous 4 nodes must all be the same and the
		//  step direction must be the same for p0 and p1 and also the same for p2 and p3 (but different from p0 and
		//  p1).
		if (numPrev == 4 && prev4[0].wallNormal == prev4[1].wallNormal && prev4[1].wallNormal == prev4[2].wallNormal &&
		    prev4[2].wallNormal == prev4[3].wallNormal && prev4[0].stepDirection == prev4[1].stepDirection &&
		    prev4[2].stepDirection == prev4[3].stepDirection && prev4[1].stepDirection != prev4[2].stepDirection)
		{
			rotationX = -DirectionVector(prev4[2].stepDirection);

			// If rotationZ cross rotationX isn't aligned with rotationY, the winding will be wrong.
			//  So swap rotationX and rotationZ in this case.
			if (glm::dot(glm::cross(rotationZ, rotationX), rotationY) < 0)
				std::swap(rotationX, rotationZ);

			translation = glm::vec3(path[i - 1].doublePos) / 2.0f;
			instanceData = &result.instances[MV_Bend].emplace_back();
			i -= 2;
		}
		// If the step direction and normals match, insert a straight segment.
		else if (path[i].stepDirection == path[i - 1].stepDirection && path[i].wallNormal == path[i - 1].wallNormal)
		{
			rotationX = glm::cross(rotationY, rotationZ);
			translation = glm::vec3(path[i].doublePos + path[i - 1].doublePos) / 4.0f;
			instanceData = &result.instances[MV_Straight].emplace_back();
		}
		else if (path[i - 1].stepDirection == path[i].wallNormal)
		{
			rotationX = glm::cross(rotationY, rotationZ);
			translation = glm::vec3(path[i].doublePos) / 2.0f;
			instanceData = &result.instances[MV_Corner].emplace_back();
		}

		if (instanceData != nullptr)
		{
			glm::mat3 rotationScaleMatrix(rotationX * MODEL_SCALE, rotationY * MODEL_SCALE, rotationZ * MODEL_SCALE);
			instanceData->transform = glm::translate(glm::mat4(1), translation) * glm::mat4(rotationScaleMatrix);
			instanceData->lightProgress = accDistance;
			accDistance++;
		}
	}

	result.maxTransitionProgress = 0;
	for (int v = 0; v < MV_Count; v++)
	{
		for (LightStripMaterial::InstanceData& instanceData : result.instances[v])
		{
			instanceData.lightProgress = result.maxTransitionProgress + accDistance - instanceData.lightProgress;
		}
	}
	result.maxTransitionProgress += accDistance + 1;

	return result;
}

void ActivationLightStripEnt::GenerateForActivator(World& world, Ent& activatorEntity)
{
	ActivatorComp* activator = activatorEntity.GetComponentMut<ActivatorComp>();
	if (activator == nullptr)
		return;

	Ent* activatableEntity = ActivatableComp::FindByName(world.entManager, activator->activatableName);
	if (activatableEntity == nullptr)
		return;

	// Fetches the light strip entity for this activator or creates a new one
	std::shared_ptr<ActivationLightStripEnt> lightStripEnt = activator->lightStripEntity.lock();
	if (lightStripEnt == nullptr)
	{
		lightStripEnt = Ent::Create<ActivationLightStripEnt>();
		lightStripEnt->PreventSerialize();
		world.entManager.AddEntity(lightStripEnt);
		activator->lightStripEntity = lightStripEnt;
	}

	lightStripEnt->m_activator = activatorEntity.shared_from_this();
	lightStripEnt->m_activatable = activatableEntity->shared_from_this();
	lightStripEnt->regenerate = true;
}

void ActivationLightStripEnt::GenerateAll(World& world)
{
	world.entManager.ForEach([&](Ent& entity) { GenerateForActivator(world, entity); });
}

void ActivationLightStripEnt::Serialize(std::ostream& stream) const {}
void ActivationLightStripEnt::Deserialize(std::istream& stream) {}

glm::vec3 ActivationLightStripEnt::GetPosition() const
{
	return glm::vec3(0);
}
