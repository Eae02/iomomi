#pragma once

struct ScrollPanel
{
public:
	float scroll = 0;
	float velocity = 0;
	float contentHeight = 0;
	float marginX = 5;
	float marginY = 5;
	float dragGrabMarginX = 4;
	eg::Rectangle screenRectangle;
	
	bool isDragging = false;
	
	void Update(float dt, bool canInteract = true);
	void Draw(eg::SpriteBatch& spriteBatch) const;
	
	void SetScroll(float s);
	
private:
	float m_maxScroll = 0;
	float m_barBrightness = 1;
	
	eg::Rectangle m_barActiveRectangle;
	eg::Rectangle m_barAreaRectangle;
};
