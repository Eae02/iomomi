#include "Shader.hpp"
#include "../../Utils.hpp"

namespace gl
{
	GLuint Shader::s_currentProgram = 0;
	
	void Shader::AttachStage(GLenum type, std::string_view source)
	{
		GLuint shader = glCreateShader(type);
		
		//Uploads shader source code.
		auto sourcePtr = reinterpret_cast<const GLchar*>(source.data());
		GLint len = source.size();
		glShaderSource(shader, 1, &sourcePtr, &len);
		
		glCompileShader(shader);
		
		//Checks the shader's compile status.
		GLint compileStatus = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
		if (!compileStatus)
		{
			GLint infoLogLen = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
			
			std::vector<char> infoLog(static_cast<size_t>(infoLogLen) + 1);
			glGetShaderInfoLog(shader, infoLogLen, nullptr, infoLog.data());
			infoLog.back() = '\0';
			
			PANIC("Shader failed to compile:" << infoLog.data());
		}
		
		glAttachShader(*m_program, shader);
	}
	
	void Shader::Link()
	{
		glLinkProgram(*m_program);
		
		int linkStatus = GL_FALSE;
		glGetProgramiv(*m_program, GL_LINK_STATUS, &linkStatus);
		if (!linkStatus)
		{
			int infoLogLen = 0;
			glGetProgramiv(*m_program, GL_INFO_LOG_LENGTH, &infoLogLen);
			
			std::vector<char> infoLog(static_cast<size_t>(infoLogLen) + 1);
			glGetProgramInfoLog(*m_program, infoLogLen, nullptr, infoLog.data());
			infoLog.back() = '\0';
			
			PANIC("Shader program failed to link: " << infoLog.data());
		}
		
		int numUniforms;
		glGetProgramiv(*m_program, GL_ACTIVE_UNIFORMS, &numUniforms);
		m_uniforms.resize(numUniforms);
		
		//Reads uniform names
		GLint sizeOut;
		GLenum typeOut;
		char nameBuffer[1024];
		for (GLuint i = 0; i < (GLuint)numUniforms; i++)
		{
			glGetActiveUniform(*m_program, i, sizeof(nameBuffer), nullptr, &sizeOut, &typeOut, nameBuffer);
			m_uniforms[i].name = nameBuffer;
			m_uniforms[i].location = i;
		}
		
		//Sorts uniforms so that they can be binary searched over later
		std::sort(m_uniforms.begin(), m_uniforms.end(), [&] (const Uniform& a, const Uniform& b)
		{
			return a.name < b.name;
		});
	}
	
	void Shader::Bind() const
	{
		if (s_currentProgram != *m_program)
		{
			glUseProgram(*m_program);
			s_currentProgram = *m_program;
		}
	}
	
	static std::vector<std::string> warnedUniforms;
	
	int Shader::GetUniformLocation(std::string_view name)
	{
		auto it = std::lower_bound(m_uniforms.begin(), m_uniforms.end(), name, [&] (const Uniform& a, std::string_view b)
		{
			return a.name < b;
		});
		
		if (it == m_uniforms.end() || it->name != name)
		{
			if (std::find(warnedUniforms.begin(), warnedUniforms.end(), name) == warnedUniforms.end())
			{
				warnedUniforms.emplace_back(name);
				std::cerr << "Uniform not found \"" << name << "\"" << std::endl;
			}
			return -1;
		}
		
		return it->location;
	}
}
