#pragma once

#include <span>

struct Vec3Compare
{
	bool operator()(const glm::vec3& a, const glm::vec3& b) const
	{
		return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
	}
};

struct IVec3Compare
{
	bool operator()(const glm::ivec3& a, const glm::ivec3& b) const
	{
		return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
	}
};

struct IVec3Hash
{
	size_t operator()(const glm::ivec3& v) const noexcept
	{
		size_t h = 0;
		eg::HashAppend(h, v.x);
		eg::HashAppend(h, v.y);
		eg::HashAppend(h, v.z);
		return h;
	}
};

template <typename T>
std::span<const char> ToCharSpan(std::span<const T> span)
{
	return std::span<const char>(reinterpret_cast<const char*>(span.data()), span.size_bytes());
}

template <typename T>
std::span<const char> RefToCharSpan(const T& ref)
{
	return std::span<const char>(reinterpret_cast<const char*>(&ref), sizeof(T));
}

// Wrapper that wraps a non-copyable type T so that it can be copied by initializing the copied value using its default
// constructor
template <typename T>
struct ResetOnCopy
{
	ResetOnCopy() = default;
	ResetOnCopy(T _inner) : inner(std::move(_inner)) {}

	ResetOnCopy(const ResetOnCopy& other) {}
	ResetOnCopy(ResetOnCopy&& other) : inner(std::move(other.inner)) {}

	ResetOnCopy& operator=(const ResetOnCopy& other)
	{
		inner = T();
		return *this;
	}

	ResetOnCopy& operator=(ResetOnCopy&& other)
	{
		inner = std::move(other.inner);
		return *this;
	}

	const T* operator->() const { return &inner; }
	T* operator->() { return &inner; }

	T inner;
};
