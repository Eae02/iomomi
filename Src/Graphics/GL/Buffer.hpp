#pragma once

#include "GL.hpp"
#include "Handle.hpp"
#include "../../Utils.hpp"

namespace gl
{
	enum class BufferUsage
	{
		None = 0,
		MapRead = 1,
		MapWrite = 2,
		Update = 4
	};
	
	BIT_FIELD(BufferUsage)
	
	class Buffer
	{
	public:
		Buffer(uint64_t size, BufferUsage usage, const void* initialData);
		
		void Realloc(uint64_t newSize, const void* newData);
		
		void* Map(uint64_t offset, uint64_t range);
		void Unmap(uint64_t modifiedOffset, uint64_t modifiedRange);
		
		void Update(uint64_t offset, uint64_t range, const void* data);
		
		GLuint Handle() const
		{
			return *m_buffer;
		}
		
		uint64_t Size() const
		{
			return m_size;
		}
		
	private:
		gl::Handle<ResType::Buffer> m_buffer;
		uint64_t m_size;
		GLenum m_storageFlags;
		GLenum m_mapFlags;
		void* m_persistentMapping;
	};
}
