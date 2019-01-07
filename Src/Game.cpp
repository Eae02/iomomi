#include "Game.hpp"
#include "ImGuiInterface.hpp"
#include "Graphics/Graphics.hpp"

#include <SDL2/SDL.h>

bool shouldClose = false;

void RunGame(SDL_Window* window)
{
	ImGuiInterface imguiInterface;
	InputState inputState;
	
	uint32_t vFrameIndex = 0;
	GLsync fences[NUM_VIRTUAL_FRAMES] = { };
	
	int drawableWidth = -1;
	int drawableHeight = -1;
	
	while (!shouldClose)
	{
		inputState.NewFrame();
		
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				shouldClose = true;
				break;
			case SDL_KEYUP:
			case SDL_KEYDOWN:
				if (!event.key.repeat)
				{
					imguiInterface.OnKeyStateChanged(event.key.keysym.scancode, event.key.state);
					inputState.KeyEvent(event.key.keysym.scancode, event.key.state);
				}
				break;
			case SDL_MOUSEMOTION:
				inputState.MouseMotionEvent(event.motion);
				break;
			case SDL_MOUSEWHEEL:
				inputState.MouseWheelEvent(event.wheel);
				break;
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
			{
				auto button = (MouseButtons)event.button.button;
				inputState.MouseButtonEvent(button, event.button.state);
				imguiInterface.OnMouseButtonStateChanged(button, event.button.state);
				break;
			}
			}
		}
		
		int newDrawableWidth, newDrawableHeight;
		SDL_GL_GetDrawableSize(window, &newDrawableWidth, &newDrawableHeight);
		if (newDrawableWidth != drawableWidth || newDrawableHeight != drawableHeight)
		{
			//camera.SetScreenSize(newDrawableWidth, newDrawableHeight);
			//renderer.SetScreenSize(newDrawableWidth, newDrawableHeight);
			
			drawableWidth = newDrawableWidth;
			drawableHeight = newDrawableHeight;
			glViewport(0, 0, newDrawableWidth, newDrawableHeight);
		}
		
		if (fences[vFrameIndex])
		{
			glClientWaitSync(fences[vFrameIndex], GL_SYNC_FLUSH_COMMANDS_BIT, UINT64_MAX);
			glDeleteSync(fences[vFrameIndex]);
		}
		
		imguiInterface.NewFrame(inputState, drawableWidth, drawableHeight);
		
		ImGui::ShowDemoWindow();
		
		const float clearValue[] = { 0.6f, 0.1f, 0.1f, 1.0f };
		glClearBufferfv(GL_COLOR, 0, clearValue);
		
		imguiInterface.EndFrame(vFrameIndex);
		
		fences[vFrameIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		SDL_GL_SwapWindow(window);
		vFrameIndex = (vFrameIndex + 1) % 3;
	}
}
