#include "Buffer.hpp"

namespace gl
{
	Buffer::Buffer(uint64_t size, BufferUsage usage, const void* initialData)
	{
		m_mapFlags = 0;
		m_persistentMapping = nullptr;
		
		m_storageFlags = 0;
		if (HasFlag(usage, BufferUsage::MapWrite) || (HasFlag(usage, BufferUsage::Update) && HasVersion45))
		{
			m_storageFlags |= GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
			m_mapFlags |= GL_MAP_WRITE_BIT;
			if (HasVersion45)
				m_mapFlags |= GL_MAP_FLUSH_EXPLICIT_BIT;
		}
		if (HasFlag(usage, BufferUsage::MapRead))
		{
			m_storageFlags |= GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT;
			m_mapFlags |= GL_MAP_READ_BIT;
		}
		
		Realloc(size, initialData);
	}
	
	void Buffer::Realloc(uint64_t newSize, const void* newData)
	{
		if (newSize == 0)
			return;
		m_size = newSize;
		
		if (HasVersion45)
		{
			glCreateBuffers(1, m_buffer.GenPtr());
			glNamedBufferStorage(*m_buffer, newSize, newData, m_storageFlags);
			m_persistentMapping = glMapNamedBufferRange(*m_buffer, 0, newSize, m_mapFlags | GL_MAP_PERSISTENT_BIT);
		}
		else
		{
			if (m_buffer.IsNull())
				glGenBuffers(1, m_buffer.GenPtr());
			glBindBuffer(GL_ARRAY_BUFFER, *m_buffer);
			glBufferData(GL_ARRAY_BUFFER, newSize, newData, m_mapFlags ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		}
	}
	
	void* Buffer::Map(uint64_t offset, uint64_t range)
	{
		if (HasVersion45)
		{
			return static_cast<char*>(m_persistentMapping) + offset;
		}
		
		glBindBuffer(GL_ARRAY_BUFFER, *m_buffer);
		return glMapBufferRange(GL_ARRAY_BUFFER, offset, range, m_mapFlags | GL_MAP_UNSYNCHRONIZED_BIT);
	}
	
	void Buffer::Unmap(uint64_t modifiedOffset, uint64_t modifiedRange)
	{
		if (HasVersion45)
		{
			if (modifiedRange > 0)
			{
				glFlushMappedNamedBufferRange(*m_buffer, modifiedOffset, modifiedRange);
			}
		}
		else
		{
			glBindBuffer(GL_ARRAY_BUFFER, *m_buffer);
			glUnmapBuffer(GL_ARRAY_BUFFER);
		}
	}
	
	void Buffer::Update(uint64_t offset, uint64_t range, const void* data)
	{
		if (!HasVersion45)
		{
			glBindBuffer(GL_ARRAY_BUFFER, *m_buffer);
			glBufferSubData(GL_ARRAY_BUFFER, offset, range, data);
		}
		else
		{
			std::memcpy(reinterpret_cast<char*>(m_persistentMapping) + offset, data, range);
			glFlushMappedNamedBufferRange(*m_buffer, offset, range);
		}
	}
}
