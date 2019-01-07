#include <SDL2/SDL.h>
#include <iostream>

#include "Game.hpp"
#include "Graphics/GL/GL.hpp"

int main()
{
	if (SDL_Init(SDL_INIT_VIDEO))
	{
		std::cerr << SDL_GetError() << std::endl;
		return 1;
	}
	
	int contextFlags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
#ifndef NDEBUG
	contextFlags |= SDL_GL_CONTEXT_DEBUG_FLAG;
#endif
	
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	
	SDL_Window* window = SDL_CreateWindow("Gravity", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		1200, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
	
	if (window == nullptr)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error creating window", SDL_GetError(), nullptr);
		return 1;
	}
	
	SDL_GLContext glContext = SDL_GL_CreateContext(window);
	if (glContext == nullptr)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error creating OpenGL context", SDL_GetError(), nullptr);
		return 1;
	}
	
	gl3wInit();
	
	gl::Init();
	
	RunGame(window);
	
	SDL_GL_DeleteContext(glContext);
	
	SDL_DestroyWindow(window);
	
	SDL_Quit();
}
