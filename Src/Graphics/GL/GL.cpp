#include "GL.hpp"
#include "../../Utils.hpp"

namespace gl
{
#ifndef NDEBUG
	enum class GLVendor
	{
		Unknown,
		Nvidia,
		IntelOpenSource
	};
	
	static GLVendor glVendor;
	
	void OpenGLMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
	                           const GLchar* message, const void* userData)
	{
		if (glVendor == GLVendor::IntelOpenSource)
		{
			if (id == 17 || id == 14) //Clearing integer framebuffer attachments.
				return;
		}
		if (glVendor == GLVendor::Nvidia)
		{
			if (id == 131186) //Buffer performance warning
				return;
		}
		
		std::string_view messageView(message, static_cast<size_t>(length));
		
		//Some vendors include a newline at the end of the message. This removes the newline if present.
		if (messageView.back() == '\n')
		{
			messageView = messageView.substr(0, messageView.size() - 1);
		}
		
		std::cout << "gl " << id << messageView << std::endl;
		
		if (severity == GL_DEBUG_SEVERITY_HIGH || type == GL_DEBUG_TYPE_ERROR)
		{
			DEBUG_BREAK
			std::abort();
		}
	}
	
	static void InitDebug()
	{
		std::string_view vendorName = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
		if (vendorName == "Intel Open Source Technology Center")
			glVendor = GLVendor::IntelOpenSource;
		else if (vendorName == "NVIDIA Corporation")
			glVendor = GLVendor::Nvidia;
		else
			glVendor = GLVendor::Unknown;
		
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(OpenGLMessageCallback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
	}
#else
	static void InitDebug() { }
#endif
	
	bool HasVersion45 = false;
	
	void Init()
	{
		InitDebug();
		
		int verMajor, verMinor;
		glGetIntegerv(GL_MAJOR_VERSION, &verMajor);
		glGetIntegerv(GL_MINOR_VERSION, &verMinor);
		
		HasVersion45 = verMajor > 4 || (verMajor == 4 && verMinor >= 5);
	}
}
