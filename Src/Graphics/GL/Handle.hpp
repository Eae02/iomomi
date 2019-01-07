#pragma once

namespace gl
{
	enum class ResType
	{
		Texture,
		Buffer,
		VertexArray,
		Shader,
		Program,
		Framebuffer
	};
	
	namespace detail
	{
		template <ResType res>
		inline void Delete(GLuint handle);
		
		template<> inline void Delete<ResType::Texture>(GLuint handle)
		{
			glDeleteTextures(1, &handle);
		}
		
		template<> inline void Delete<ResType::Buffer>(GLuint handle)
		{
			glDeleteBuffers(1, &handle);
		}
		
		template<> inline void Delete<ResType::VertexArray>(GLuint handle)
		{
			glDeleteVertexArrays(1, &handle);
		}
		
		template<> inline void Delete<ResType::Shader>(GLuint handle)
		{
			glDeleteShader(handle);
		}
		
		template<> inline void Delete<ResType::Program>(GLuint handle)
		{
			glDeleteProgram(handle);
		}
		
		template<> inline void Delete<ResType::Framebuffer>(GLuint handle)
		{
			glDeleteFramebuffers(1, &handle);
		}
	}
	
	template <ResType R>
	class Handle
	{
	public:
		Handle()
			: m_null(true), m_handle(0) { }
		Handle(GLuint _handle)
			: m_null(false), m_handle(_handle) { }
		
		~Handle() noexcept
		{
			Reset();
		}
		
		Handle(Handle&& other) noexcept
			: m_null(other.m_null), m_handle(other.m_handle)
		{
			other.m_null = true;
			other.m_handle = 0;
		}
		
		Handle& operator=(Handle&& other) noexcept
		{
			this->~Handle();
			m_null = other.m_null;
			m_handle = other.m_handle;
			other.m_null = true;
			other.m_handle = 0;
			return *this;
		}
		
		/**
		 * Deletes the current resource and returns a pointer to be passed to glGen_.
		 * @return A pointer to be passed to glGen_
		 */
		inline GLuint* GenPtr()
		{
			if (!m_null)
				detail::Delete<R>(m_handle);
			else
				m_null = false;
			return &m_handle;
		}
		
		bool operator==(const Handle& other) const
		{ return m_null == other.m_null && m_handle == other.m_handle; }
		bool operator!=(const Handle& other) const
		{ return !operator==(other); }
		GLuint operator*() const
		{ return m_handle; }
		
		bool IsNull() const noexcept
		{
			return m_null;
		}
		
		void Reset() noexcept
		{
			if (!m_null)
			{
				detail::Delete<R>(m_handle);
				m_null = true;
				m_handle = 0;
			}
		}
		
	private:
		bool m_null;
		GLuint m_handle;
	};
}
