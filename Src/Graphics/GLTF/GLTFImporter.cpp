#include "GLTFImporter.hpp"
#include "GLTFData.hpp"
#include "../Mesh.hpp"
//#include "../../IOUtils.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

using namespace nlohmann;

template <typename IsValidTp>
inline std::string AddNameSuffix(const std::string& originalName, IsValidTp isValid)
{
	std::string finalName = originalName;
	std::ostringstream nameStream;
	int suffix = 1;
	while (!isValid(finalName))
	{
		suffix++;
		
		nameStream.str("");
		nameStream << originalName << "_" << suffix;
		finalName = nameStream.str();
	}
	
	return finalName;
}

namespace gltf
{
	static glm::mat4 ParseNodeTransform(const json& node)
	{
		auto matrixIt = node.find("matrix");
		if (matrixIt != node.end())
		{
			glm::mat4 transform;
			
			for (int c = 0; c < 4; c++)
			{
				for (int r = 0; r < 4; r++)
				{
					transform[c][r] = (*matrixIt)[c * 4 + r];
				}
			}
			
			return transform;
		}
		
		glm::vec3 scale(1.0f);
		glm::quat rotation;
		glm::vec3 translation;
		
		auto scaleIt = node.find("scale");
		if (scaleIt != node.end())
		{
			scale = glm::vec3((*scaleIt)[0], (*scaleIt)[1], (*scaleIt)[2]);
		}
		
		auto rotationIt = node.find("rotation");
		if (rotationIt != node.end())
		{
			rotation = glm::quat((*rotationIt)[3], (*rotationIt)[0], (*rotationIt)[1], (*rotationIt)[2]);
		}
		
		auto translationIt = node.find("translation");
		if (translationIt != node.end())
		{
			translation = glm::vec3((*translationIt)[0], (*translationIt)[1], (*translationIt)[2]);
		}
		
		return glm::translate(glm::mat4(1), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1), scale);
	}
	
	struct MeshToImport
	{
		long m_meshIndex;
		long m_skinIndex;
		std::string m_name;
		glm::mat4 m_transform;
	};
	
	static void WalkNodeTree(const json& nodesArray, size_t rootIndex, std::vector<MeshToImport>& meshes,
	                         const glm::mat4& transform)
	{
		const json& nodeEl = nodesArray[rootIndex];
		
		auto meshIt = nodeEl.find("mesh");
		
		glm::mat4 nodeTransform = transform * ParseNodeTransform(nodeEl);
		
		if (meshIt != nodeEl.end())
		{
			std::string name;
			
			auto nameIt = nodeEl.find("name");
			if (nameIt != nodeEl.end())
			{
				name = nameIt->get<std::string>();
			}
			
			auto skinIt = nodeEl.find("skin");
			
			MeshToImport mesh;
			mesh.m_meshIndex = meshIt->get<long>();
			mesh.m_skinIndex = skinIt == nodeEl.end() ? -1 : skinIt->get<long>();
			mesh.m_name = std::move(name);
			mesh.m_transform = nodeTransform;
			
			meshes.push_back(std::move(mesh));
		}
		
		auto childrenIt = nodeEl.find("children");
		if (childrenIt != nodeEl.end())
		{
			for (size_t i = 0; i < childrenIt->size(); i++)
			{
				WalkNodeTree(nodesArray, (*childrenIt)[i].get<size_t>(), meshes, nodeTransform);
			}
		}
	}
	
	Model Import(const std::filesystem::path& path)
	{
		std::filesystem::path dirPath = path.parent_path();
		
		std::ifstream fileStream(path);
		json jsonRoot = json::parse(fileStream);
		fileStream.close();
		
		GLTFData data;
		
		//Parses and reads buffers
		const json& buffersArray = jsonRoot.at("buffers");
		for (size_t i = 0; i < buffersArray.size(); i++)
		{
			std::ifstream dataStream(dirPath / buffersArray[i].at("uri").get<std::string>(), std::ios::binary);
			data.AddBuffer(ReadStreamContents(dataStream));
		}
		
		//Parses buffer views
		const json& bufferViewsArray = jsonRoot.at("bufferViews");
		for (size_t i = 0; i < bufferViewsArray.size(); i++)
		{
			BufferView bufferView;
			bufferView.BufferIndex = bufferViewsArray[i].at("buffer");
			bufferView.ByteOffset = bufferViewsArray[i].at("byteOffset");
			
			auto byteStrideIt = bufferViewsArray[i].find("byteStride");
			if (byteStrideIt != bufferViewsArray[i].end())
				bufferView.ByteStride = *byteStrideIt;
			else
				bufferView.ByteStride = 0;
			
			data.AddBufferView(bufferView);
		}
		
		//Parses accessors
		for (const json& accessorEl : jsonRoot.at("accessors"))
		{
			Accessor accessor = { };
			
			const auto& view = data.GetBufferView(accessorEl.at("bufferView").get<long>());
			accessor.BufferIndex = view.BufferIndex;
			accessor.ElementCount = accessorEl.at("count");
			accessor.componentType = static_cast<ComponentType>(accessorEl.at(
				"componentType").get<int>());
			accessor.elementType = GLTFData::ParseElementType(accessorEl.at("type").get<std::string>());
			accessor.ByteOffset = view.ByteOffset;
			
			auto byteOffsetIt = accessorEl.find("byteOffset");
			if (byteOffsetIt != accessorEl.end())
				accessor.ByteOffset += byteOffsetIt->get<int>();
			
			int componentsPerElement = 0;
			switch (accessor.elementType)
			{
			case ElementType::SCALAR:
				componentsPerElement = 1;
				break;
			case ElementType::VEC2:
				componentsPerElement = 2;
				break;
			case ElementType::VEC3:
				componentsPerElement = 3;
				break;
			case ElementType::VEC4:
				componentsPerElement = 4;
				break;
			}
			
			if (view.ByteStride != 0)
			{
				accessor.ByteStride = view.ByteStride;
			}
			else
			{
				switch (accessor.componentType)
				{
				case ComponentType::UInt8:
					accessor.ByteStride = 1 * componentsPerElement;
					break;
				case ComponentType::UInt16:
					accessor.ByteStride = 2 * componentsPerElement;
					break;
				case ComponentType::UInt32:
				case ComponentType::Float:
					accessor.ByteStride = 4 * componentsPerElement;
					break;
				}
			}
			
			data.AddAccessor(accessor);
		}
		
		//Imports materials.
		std::vector<std::string> materialNames;
		auto materialsIt = jsonRoot.find("materials");
		if (materialsIt != jsonRoot.end())
		{
			for (size_t i = 0; i < materialsIt->size(); i++)
			{
				std::string name = "Unnamed Material";
				
				auto nameIt = (*materialsIt)[i].find("name");
				if (nameIt != (*materialsIt)[i].end())
				{
					name = nameIt->get<std::string>();
				}
				
				name = AddNameSuffix(name, [&] (const std::string& cName)
				{
					return !std::any_of(materialNames.begin(), materialNames.end(),
						[&] (const std::string& n) { return cName == n; });
				});
				
				std::cout << "Imported material '" << name << "'. Assigned to index " << i << ".";
				
				materialNames.push_back(std::move(name));
			}
		}
		
		int sceneIndex = 0;
		auto sceneIt = jsonRoot.find("scene");
		if (sceneIt != jsonRoot.end())
			sceneIndex = sceneIt->get<uint32_t>();
		
		const json& sceneNodesArray = jsonRoot.at("scenes").at(sceneIndex).at("nodes");
		const json& nodesArray = jsonRoot.at("nodes");
		const json& meshesArray = jsonRoot.at("meshes");
		
		//Walks the node tree to find meshes to import
		std::vector<MeshToImport> meshesToImport;
		for (const json& sceneNode : sceneNodesArray)
		{
			WalkNodeTree(nodesArray, sceneNode.get<size_t>(), meshesToImport, glm::mat4(1.0f));
		}
		
		//Imports meshes
		std::vector<NamedMesh> meshes;
		for (MeshToImport& meshToImport : meshesToImport)
		{
			const json& meshEl = meshesArray.at(meshToImport.m_meshIndex);
			
			std::string baseName = std::move(meshToImport.m_name);
			
			if (baseName.empty())
			{
				auto nameIt = meshEl.find("name");
				if (nameIt != meshEl.end())
					baseName = nameIt->get<std::string>();
				else
					baseName = path.stem().string();
			}
			
			const json& primitivesArray = meshEl.at("primitives");
			for (size_t i = 0; i < primitivesArray.size(); i++)
			{
				std::string name = baseName;
				if (primitivesArray.size() > 1)
					name += "_" + std::to_string(i);
				
				name = AddNameSuffix(name, [&] (const std::string& cName)
				{
					return !std::any_of(meshes.begin(), meshes.end(), [&] (const NamedMesh& mesh)
					{
						return mesh.Name() == cName;
					});
				});
				
				//meshes.emplace_back(std::move(name), data, primitivesArray.at(i), meshToImport.m_transform);
			}
		}
		
		return Model(std::move(meshes));
	}
}
