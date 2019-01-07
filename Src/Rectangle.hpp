#pragma once

#include "Utils.hpp"

struct Rectangle
{
	Rectangle() : x(0), y(0), w(0), h(0) { }
	
	Rectangle(glm::vec2 pos, glm::vec2 size)
		: x(pos.x), y(pos.y), w(size.x), h(size.y) { }
	
	Rectangle(glm::vec2 pos, float width, float height)
		: x(pos.x), y(pos.y), w(width), h(height) { }
	
	Rectangle(float _x, float _y, glm::vec2 size)
		: x(_x), y(_y), w(size.x), h(size.y) { }
	
	Rectangle(float _x, float _y, float width, float height)
		: x(_x), y(_y), w(width), h(height) { }
	
	static Rectangle CreateCentered(glm::vec2 pos, float w, float h)
	{ return CreateCentered(pos.x, pos.y, w, h); }
	
	static Rectangle FromMinMax(glm::vec2 min, glm::vec2 max);
	
	static Rectangle CreateCentered(float x, float y, float w, float h);
	
	static Rectangle Mix(const Rectangle& a, const Rectangle& b, float x);
	
	inline float MaxX() const
	{ return x + w; }
	
	inline float MaxY() const
	{ return y + h; }
	
	inline float CenterX() const
	{ return x + (w / 2.0f); }
	
	inline float CenterY() const
	{ return y + (h / 2.0f); }
	
	inline glm::vec2 Center() const
	{ return glm::vec2(CenterX(), CenterY()); }
	
	Rectangle Inflated(float amount) const;
	
	inline glm::vec2 Min() const
	{ return glm::vec2(x, y); }
	
	inline glm::vec2 Max() const
	{ return glm::vec2(x + w, y + h); }
	
	inline glm::vec2 MinXMaxY() const
	{ return glm::vec2(x, y + h); }
	
	inline glm::vec2 MaxXMinY() const
	{ return glm::vec2(x + w, y); }
	
	inline glm::vec2 Size() const
	{ return glm::vec2(w, h); }
	
	std::array<Rectangle, 4> Divide() const;
	
	bool Intersects(const Rectangle& other) const;
	
	bool Contains(float px, float py) const;
	
	bool Contains(glm::vec2 point) const;
	
	bool Contains(const Rectangle& other) const;
	
	inline bool Empty() const
	{ return w == 0 || h == 0; }
	
	inline bool operator==(const Rectangle& other) const
	{
		return FEqual(x, other.x) && FEqual(y, other.y) && FEqual(w, other.w) && FEqual(h, other.h);
	}
	
	inline bool operator!=(const Rectangle& other) const
	{
		return !operator==(other);
	}
	
	float x, y, w, h;
};
