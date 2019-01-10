#pragma once

#include <EGame/EG.hpp>

namespace gltf
{
	enum class ElementType
	{
		SCALAR,
		VEC2,
		VEC3,
		VEC4
	};
	
	enum class ComponentType
	{
		UInt8 = 5121,
		UInt16 = 5123,
		UInt32 = 5125,
		Float = 5126
	};
	
	struct BufferView
	{
		int BufferIndex;
		int ByteOffset;
		int ByteStride;
	};
	
	struct Accessor
	{
		int BufferIndex;
		int ByteOffset;
		int ByteStride;
		ComponentType componentType;
		int ElementCount;
		ElementType elementType;
	};
	
	class GLTFData
	{
	public:
		inline void AddBuffer(std::vector<char> buffer)
		{
			m_buffers.push_back(std::move(buffer));
		}
		
		inline void AddBufferView(const BufferView& view)
		{
			m_bufferViews.push_back(view);
		}
		
		inline void AddAccessor(const Accessor& accessor)
		{
			m_accessors.push_back(accessor);
		}
		
		inline const BufferView& GetBufferView(long index) const
		{
			if (index < 0 || static_cast<size_t>(index) >= m_bufferViews.size())
				EG_PANIC("Buffer view index out of range.");
			return m_bufferViews[index];
		}
		
		inline const Accessor& GetAccessor(long index) const
		{
			if (index < 0 || static_cast<size_t>(index) >= m_accessors.size())
				EG_PANIC("Accessor index out of range.");
			return m_accessors[index];
		}
		
		inline bool CheckAccessor(long index, ElementType elementType, ComponentType componentType) const
		{
			return index >= 0 && static_cast<size_t>(index) < m_accessors.size() &&
			       m_accessors[index].elementType == elementType &&
			       m_accessors[index].componentType == componentType;
		}
		
		inline const char* GetAccessorData(long index) const
		{
			return m_buffers[GetAccessor(index).BufferIndex].data();
		}
		
		inline const char* GetAccessorData(const Accessor& accessor) const
		{
			return m_buffers[accessor.BufferIndex].data() + accessor.ByteOffset;
		}
		
		static ElementType ParseElementType(std::string_view name);
	
	private:
		std::vector<std::vector<char>> m_buffers;
		std::vector<BufferView> m_bufferViews;
		std::vector<Accessor> m_accessors;
	};
}
