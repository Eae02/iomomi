#pragma once

#include "Handle.hpp"
#include "../../Utils.hpp"
#include "../../Span.hpp"

namespace gl
{
	class Shader
	{
	public:
		Shader()
			: m_program(glCreateProgram()) { }
		
		void AttachStage(GLenum type, std::string_view source);
		
		void Link();
		
		void Bind() const;
		
		int GetUniformLocation(std::string_view name);
		
		template <typename T>
		void SetUniform(std::string_view name, const T& value)
		{
			int location = GetUniformLocation(name);
			if (location != -1)
				StSetUniform<T>(*m_program, location, Span<const T>(&value, 1));
		}
		
		template <typename T>
		void SetUniformArray(std::string_view name, Span<const T> value)
		{
			int location = GetUniformLocation(name);
			if (location != -1)
				StSetUniform<T>(*m_program, location, value);
		}
		
		GLuint Program() const
		{
			return *m_program;
		}
		
	private:
		static GLuint s_currentProgram;
		
		template <typename T>
		static inline void StSetUniform(GLuint program, int location, Span<const T> v);
		
		Handle<ResType::Program> m_program;
		
		struct Uniform
		{
			std::string name;
			int location;
		};
		
		std::vector<Uniform> m_uniforms;
	};
	
#define DEF_SCALAR_SET_UNIFORM(T, fn) \
	template <> inline void Shader::StSetUniform(GLuint program, int location, Span<const T> v) \
	{ glProgramUniform ## fn(program, location, v.size(), reinterpret_cast<const T*>(v.data())); }
#define DEF_VEC_SET_UNIFORM(T, fn) \
	template <> inline void Shader::StSetUniform(GLuint program, int location, Span<const T> v) \
	{ glProgramUniform ## fn(program, location, v.size(), &v[0].x); }
#define DEF_MAT_SET_UNIFORM(T, fn) \
	template <> inline void Shader::StSetUniform(GLuint program, int location, Span<const T> v) \
	{ glProgramUniformMatrix ## fn(program, location, v.size(), GL_FALSE, reinterpret_cast<const float*>(v.data())); }
	
	DEF_SCALAR_SET_UNIFORM(float, 1fv)
	DEF_VEC_SET_UNIFORM(glm::vec2, 2fv)
	DEF_VEC_SET_UNIFORM(glm::vec3, 3fv)
	DEF_VEC_SET_UNIFORM(glm::vec4, 4fv)
	DEF_SCALAR_SET_UNIFORM(int, 1iv)
	DEF_VEC_SET_UNIFORM(glm::ivec2, 2iv)
	DEF_VEC_SET_UNIFORM(glm::ivec3, 3iv)
	DEF_VEC_SET_UNIFORM(glm::ivec4, 4iv)
	DEF_SCALAR_SET_UNIFORM(unsigned, 1uiv)
	DEF_VEC_SET_UNIFORM(glm::uvec2, 2uiv)
	DEF_VEC_SET_UNIFORM(glm::uvec3, 3uiv)
	DEF_VEC_SET_UNIFORM(glm::uvec4, 4uiv)
	DEF_MAT_SET_UNIFORM(glm::mat2, 2fv)
	DEF_MAT_SET_UNIFORM(glm::mat3, 3fv)
	DEF_MAT_SET_UNIFORM(glm::mat4, 4fv)

#undef DEF_SCALAR_SET_UNIFORM
#undef DEF_VEC_SET_UNIFORM
#undef DEF_MAT_SET_UNIFORM
}
