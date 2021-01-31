#pragma once

class EntEditorWallRotate
{
public:
	virtual float GetWallRotationAlignment() const { return 0; }
	virtual float GetWallRotation() const = 0;
	virtual void SetWallRotation(float rotation) = 0;
};
