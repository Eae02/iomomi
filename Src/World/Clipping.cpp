#include "Clipping.hpp"
#include "World.hpp"
#include "Entities/Messages.hpp"

void CalcPolygonClipping(ClippingArgs& args, eg::Span<const glm::vec3> vertices)
{
	EG_ASSERT(vertices.size() >= 3);
	eg::Plane plane(vertices[0], vertices[1], vertices[2]);
	
	glm::vec3 aabbVertices[] =
	{
		glm::vec3(args.aabb.min.x, args.aabb.min.y, args.aabb.min.z),
		glm::vec3(args.aabb.min.x, args.aabb.min.y, args.aabb.max.z),
		glm::vec3(args.aabb.min.x, args.aabb.max.y, args.aabb.min.z),
		glm::vec3(args.aabb.min.x, args.aabb.max.y, args.aabb.max.z),
		glm::vec3(args.aabb.max.x, args.aabb.min.y, args.aabb.min.z),
		glm::vec3(args.aabb.max.x, args.aabb.min.y, args.aabb.max.z),
		glm::vec3(args.aabb.max.x, args.aabb.max.y, args.aabb.min.z),
		glm::vec3(args.aabb.max.x, args.aabb.max.y, args.aabb.max.z),
	};
	
	glm::vec3 centerAABB = (args.aabb.min + args.aabb.max) / 2.0f;
	
	float div = glm::dot(args.move, plane.GetNormal());
	if (std::abs(div) < 1E-6f)
		return;
	if (div > 0)
	{
		plane.FlipNormal();
		div = -div;
	}
	
	for (const glm::vec3& v : aabbVertices)
	{
		if (glm::dot(v - centerAABB, args.move) < 0)
			continue;
		
		float a = (plane.GetDistance() - glm::dot(v, plane.GetNormal())) / div;
		if (a >= -1E-4f && a < args.clipDist)
		{
			glm::vec3 iPos = plane.GetClosestPointOnPlane(v + a * args.move);
			bool inPolygon = false;
			for (size_t i = 2; i < vertices.size(); i++)
			{
				if (eg::TriangleContainsPoint(vertices[0], vertices[i - 1], vertices[i], iPos))
				{
					inPolygon = true;
					break;
				}
			}
			
			if (inPolygon)
			{
				args.clipDist = std::max(a, 0.0f);
				args.colPlaneNormal = plane.GetNormal();
			}
		}
	}
}

void CalcWorldClipping(const World& world, ClippingArgs& args)
{
	const glm::vec3 endMin = args.aabb.min + args.move;
	const glm::vec3 endMax = args.aabb.max + args.move;
	
	constexpr float EP = 0.0001f;
	
	//Clipping against voxels
	for (int axis = 0; axis < 3; axis++)
	{
		if (std::abs(args.move[axis]) < 1E-6f)
			continue;
		
		const bool negativeMove = args.move[axis] < 0;
		
		int vOffset;
		float beginA, endA;
		if (negativeMove)
		{
			beginA = args.aabb.min[axis];
			endA = endMin[axis];
			vOffset = -1;
		}
		else
		{
			beginA = args.aabb.max[axis];
			endA = endMax[axis];
			vOffset = 0;
		}
		
		const int minI = (int)std::floor(std::min(beginA, endA) - EP) - 1;
		const int maxI = (int)std::ceil(std::max(beginA, endA) + EP) + 1;
		
		const int axisB = (axis + 1) % 3;
		const int axisC = (axis + 2) % 3;
		
		glm::ivec3 normal(0);
		normal[axis] = negativeMove ? 1 : -1;
		const Dir collisionSide = (Dir)(axis * 2 + (negativeMove ? 0 : 1));
		
		for (int v = minI; v <= maxI; v++)
		{
			const float t = (v - beginA) / args.move[axis];
			if (t > args.clipDist || t < -EP)
				continue;
			
			const int minB = (int)std::floor(glm::mix(args.aabb.min[axisB], endMin[axisB], t));
			const int maxB = (int)std::ceil(glm::mix(args.aabb.max[axisB], endMax[axisB], t));
			const int minC = (int)std::floor(glm::mix(args.aabb.min[axisC], endMin[axisC], t));
			const int maxC = (int)std::ceil(glm::mix(args.aabb.max[axisC], endMax[axisC], t));
			for (int vb = minB; vb < maxB; vb++)
			{
				for (int vc = minC; vc < maxC; vc++)
				{
					glm::ivec3 coord;
					coord[axis] = v + vOffset;
					coord[axisB] = vb;
					coord[axisC] = vc;
					
					if (world.HasCollision(coord, collisionSide))
					{
						args.colPlaneNormal = normal;
						args.clipDist = std::max(t, 0.0f);
						break;
					}
				}
			}
		}
	}
	
	CalculateCollisionMessage message;
	message.clippingArgs = &args;
	world.EntityManager().SendMessageToAll(message);
}

float CalcCollisionCorrection(const eg::AABB& aabb, const eg::Plane& plane, float maxC)
{
	float minD = 0;
	float maxD = 0;
	for (int n = 0; n < 8; n++)
	{
		float d = plane.GetDistanceToPoint(aabb.NthVertex(n));
		minD = std::min(minD, d);
		maxD = std::max(maxD, d);
	}
	float c = -minD;
	return (c > maxD || c > maxC) ? 0 : c;
}

glm::vec3 CalcWorldCollisionCorrection(const World& world, const eg::AABB& aabb, float maxC)
{
	glm::ivec3 searchMin(glm::floor(aabb.min));
	glm::ivec3 searchMax(glm::ceil(aabb.max));
	
	for (int x = searchMin.x; x < searchMax.x; x++)
	{
		for (int y = searchMin.y; y < searchMax.y; y++)
		{
			for (int z = searchMin.z; z < searchMax.z; z++)
			{
				glm::ivec3 pos(x, y, z);
				if (world.IsAir(pos))
					continue;
				
				for (int s = 0; s < 6; s++)
				{
					glm::ivec3 toNeighbor = DirectionVector((Dir)s);
					if (!world.HasCollision(pos + toNeighbor, OppositeDir((Dir)s)))
						continue;
					
					eg::Plane plane(toNeighbor, (pos * 2 + 1 + toNeighbor) / 2);
					float correction = CalcCollisionCorrection(aabb, plane, maxC);
					if (correction > 0)
					{
						return plane.GetNormal() * correction;
					}
				}
			}
		}
	}
	return glm::vec3(0.0f);
}
