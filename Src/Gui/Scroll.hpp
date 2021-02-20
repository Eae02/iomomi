#pragma once

struct ScrollPanel
{
	float scroll = 0;
	float contentHeight = 0;
	float marginX = 5;
	float marginY = 5;
	eg::Rectangle screenRectangle;
	
	float velocity = 0;
	
	void Update(float dt, bool canInteract = true);
	void Draw(eg::SpriteBatch& spriteBatch) const;
};
