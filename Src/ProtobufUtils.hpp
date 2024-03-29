#pragma once

#include "../Protobuf/Build/Vec.pb.h"

inline glm::vec3 PBToGLM(const iomomi_pb::Vec3& v)
{
	return glm::vec3(v.x(), v.y(), v.z());
}

inline glm::vec2 PBToGLM(const iomomi_pb::Vec2& v)
{
	return glm::vec2(v.x(), v.y());
}

inline iomomi_pb::Vec3* GLMToPB(glm::vec3 v, google::protobuf::Arena* arena)
{
	iomomi_pb::Vec3* res = google::protobuf::Arena::CreateMessage<iomomi_pb::Vec3>(arena);
	res->set_x(v.x);
	res->set_y(v.y);
	res->set_z(v.z);
	return res;
}

inline iomomi_pb::Vec2* GLMToPB(glm::vec2 v, google::protobuf::Arena* arena)
{
	iomomi_pb::Vec2* res = google::protobuf::Arena::CreateMessage<iomomi_pb::Vec2>(arena);
	res->set_x(v.x);
	res->set_y(v.y);
	return res;
}
