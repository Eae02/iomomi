#pragma once

class ECDecal
{
public:
	ECDecal();
	
	void HandleMessage(eg::Entity& entity, const struct DrawMessage& message);
	void HandleMessage(eg::Entity& entity, const struct EditorDrawMessage& message);
	void HandleMessage(eg::Entity& entity, const struct EditorRenderImGuiMessage& message);
	
	void SetMaterialByName(std::string_view materialName);
	
	const char* GetMaterialName() const;
	
	float rotation = 0;
	float scale = 1;
	
	static eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	static eg::MessageReceiver MessageReceiver;
	
	static eg::EntitySignature Signature;
	
	static eg::IEntitySerializer* EntitySerializer;
	
private:
	static glm::mat4 GetTransform(const eg::Entity& entity);
	
	void UpdateMaterialPointer();
	
	const class DecalMaterial* m_material = nullptr;
	int m_decalMaterialIndex = 0;
};
