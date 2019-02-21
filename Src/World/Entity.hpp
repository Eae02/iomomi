#pragma once

class Entity
{
public:
	virtual ~Entity() = default;
	
	const glm::vec3& Position() const
	{
		return m_position;
	}
	
	void SetPosition(const glm::vec3& position)
	{
		m_position = position;
	}
	
	struct EditorInteractArgs
	{
		const class World* world;
		eg::Ray viewRay;
	};
	
	virtual bool EditorInteract(const EditorInteractArgs& args);
	
private:
	glm::vec3 m_position;
};
