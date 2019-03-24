#include "Player.hpp"

#include <imgui.h>

//Constants related to player physics
static constexpr float WALK_SPEED = 4.0f;
static constexpr float ACCEL_TIME = 0.1f;
static constexpr float DEACCEL_TIME = 0.05f;
static constexpr float GRAVITY = 20;
static constexpr float JUMP_HEIGHT = 1.1f;
static constexpr float FALLING_GRAVITY_RAMP = 0.1f;
static constexpr float MAX_VERTICAL_SPEED = 100;

//Constants related to player size
static constexpr float HEIGHT = 1.65f;
static constexpr float WIDTH = 0.8f;
static constexpr float EYE_HEIGHT = HEIGHT * 0.75f;

//Constants which derive from previous
static const float JUMP_ACCEL = std::sqrt(2.0f * JUMP_HEIGHT * GRAVITY);
static const float ACCEL_AMOUNT = WALK_SPEED / ACCEL_TIME;
static constexpr float DEACCEL_AMOUNT = WALK_SPEED / DEACCEL_TIME;
static constexpr float EYE_OFFSET = EYE_HEIGHT - HEIGHT / 2;

Player::Player()
	: m_position(1, 2, 1), m_velocity(0.0f) { }

static const glm::vec3 leftDirs[6] =
{
	{ 0, 1, 0 },
	{ 0, 1, 0 },
	{ 1, 0, 0 },
	{ 1, 0, 0 },
	{ 0, 1, 0 },
	{ 0, 1, 0 }
};

inline glm::quat GetDownCorrection(Dir down)
{
	glm::vec3 up = -DirectionVector(down);
	return glm::quat_cast(glm::mat3(leftDirs[(int)down], up, glm::cross(leftDirs[(int)down], up)));
}

inline glm::quat GetRotation(float yaw, float pitch, Dir down)
{
	return GetDownCorrection(down) *
	       glm::angleAxis(yaw, glm::vec3(0, 1, 0)) *
	       glm::angleAxis(pitch, glm::vec3(1, 0, 0));
}

void Player::Update(World& world, float dt)
{
	const glm::vec3 up = -DirectionVector(m_down);
	
	auto TransitionInterpol = [&] { return glm::smoothstep(0.0f, 1.0f, m_transitionTime); };
	
	//Updates the gravity switch transition.
	//If this is playing remaining update logic will be skipped!
	if (m_gravityTransitionMode == TransitionMode::Corner)
	{
		constexpr float TRANSITION_DURATION = 0.2f;
		m_transitionTime += dt / TRANSITION_DURATION;
		
		if (m_transitionTime > 1.0f)
		{
			m_position = m_newPosition;
			m_rotation = m_newRotation;
			m_gravityTransitionMode = TransitionMode::None;
		}
		else
		{
			m_eyePosition = glm::mix(m_oldEyePosition, m_newPosition + up * EYE_OFFSET, TransitionInterpol());
			m_rotation = glm::slerp(m_oldRotation, m_newRotation, TransitionInterpol());
			return;
		}
	}
	
	if (m_gravityTransitionMode == TransitionMode::None)
	{
		const float MOUSE_SENSITIVITY = -0.005f;
		const float GAME_PAD_AXIS_SENSITIVITY = 2.0f;
		
		glm::vec2 rotationDelta = glm::vec2(eg::CursorPosDelta()) * MOUSE_SENSITIVITY;
		rotationDelta -= eg::InputState::Current().RightAnalogValue() * GAME_PAD_AXIS_SENSITIVITY * dt;
		
		//Updates the camera's rotation
		m_rotationYaw += rotationDelta.x;
		m_rotationPitch = glm::clamp(m_rotationPitch + rotationDelta.y, -eg::HALF_PI * 0.95f, eg::HALF_PI * 0.95f);
		
		m_rotation = GetRotation(m_rotationYaw, m_rotationPitch, m_down);
	}
	else if (m_gravityTransitionMode == TransitionMode::Fall)
	{
		constexpr float TRANSITION_DURATION = 0.3f;
		m_transitionTime += dt / TRANSITION_DURATION;
		
		if (m_transitionTime > 1.0f)
		{
			m_rotation = m_newRotation;
			m_gravityTransitionMode = TransitionMode::None;
		}
		else
		{
			m_rotation = glm::slerp(m_oldRotation, m_newRotation, TransitionInterpol());
		}
	}
	
	//Constructs the forward and right movement vectors.
	//These, along with up, are the basis vectors for local space.
	auto GetDirVector = [&] (const glm::vec3& v)
	{
		glm::vec3 v1 = m_rotation * v;
		return glm::normalize(v1 - glm::dot(v1, up) * up);
	};
	const glm::vec3 forward = GetDirVector(glm::vec3(0, 0, -1));
	const glm::vec3 right = GetDirVector(glm::vec3(1, 0, 0));
	
	//Finds the velocity vector in local space
	float localVelVertical = glm::dot(up, m_velocity);
	glm::vec2 localVelPlane(glm::dot(forward, m_velocity), glm::dot(right, m_velocity));
	glm::vec2 localAccPlane(-eg::AxisValue(eg::ControllerAxis::LeftY), eg::AxisValue(eg::ControllerAxis::LeftX));
	
	if (m_gravityTransitionMode == TransitionMode::None)
	{
		const bool moveForward = eg::IsButtonDown(eg::Button::W) || eg::IsButtonDown(eg::Button::CtrlrDPadUp);
		const bool moveBack =    eg::IsButtonDown(eg::Button::S) || eg::IsButtonDown(eg::Button::CtrlrDPadDown);
		const bool moveLeft =    eg::IsButtonDown(eg::Button::A) || eg::IsButtonDown(eg::Button::CtrlrDPadLeft);
		const bool moveRight =   eg::IsButtonDown(eg::Button::D) || eg::IsButtonDown(eg::Button::CtrlrDPadRight);
		
		if (moveForward == moveBack && std::abs(localAccPlane.x) < 1E-4f)
		{
			if (localVelPlane.x < 0)
			{
				localVelPlane.x += dt * DEACCEL_AMOUNT;
				if (localVelPlane.x > 0)
					localVelPlane.x = 0.0f;
			}
			if (localVelPlane.x > 0)
			{
				localVelPlane.x -= dt * DEACCEL_AMOUNT;
				if (localVelPlane.x < 0)
					localVelPlane.x = 0.0f;
			}
		}
		else if (moveForward)
		{
			localAccPlane += glm::vec2(1, 0);
		}
		else if (moveBack)
		{
			localAccPlane -= glm::vec2(1, 0);
		}
		
		if (moveRight == moveLeft && std::abs(localAccPlane.y) < 1E-4f)
		{
			if (localVelPlane.y < 0)
			{
				localVelPlane.y += dt * DEACCEL_AMOUNT;
				if (localVelPlane.y > 0)
					localVelPlane.y = 0.0f;
			}
			if (localVelPlane.y > 0)
			{
				localVelPlane.y -= dt * DEACCEL_AMOUNT;
				if (localVelPlane.y < 0)
					localVelPlane.y = 0.0f;
			}
		}
		else if (moveLeft)
		{
			localAccPlane -= glm::vec2(0, 1);
		}
		else if (moveRight)
		{
			localAccPlane += glm::vec2(0, 1);
		}
		
		const float accelMag = glm::length(localAccPlane);
		if (accelMag > 1E-6f)
		{
			localAccPlane *= ACCEL_AMOUNT / accelMag;
			
			//Increases the localAccPlane if the player is being accelerated in the opposite direction of it's current
			// velocity. This makes direction changes snappier.
			if (glm::dot(localAccPlane, localVelPlane) < 0.0f)
				localAccPlane *= 4.0f;
			
			localVelPlane += localAccPlane * dt;
		}
		
		//Caps the local velocity to the walking speed
		const float speed = glm::length(localVelPlane);
		if (speed > WALK_SPEED)
		{
			localVelPlane *= WALK_SPEED / speed;
		}
	}
	
	//Updates vertical velocity
	if ((eg::IsButtonDown(eg::Button::Space) || eg::IsButtonDown(eg::Button::CtrlrA)) &&
	    m_gravityTransitionMode == TransitionMode::None && m_onGround)
	{
		localVelVertical = JUMP_ACCEL;
	}
	else
	{
		float gravity = GRAVITY;
		gravity *= 1.0f + FALLING_GRAVITY_RAMP * glm::clamp(-localVelVertical, 0.0f, 1.0f);
		
		localVelVertical = std::max(localVelVertical - gravity * dt, -MAX_VERTICAL_SPEED);
	}
	
	//Reconstructs the world velocity vector
	glm::vec3 velocityXZ = localVelPlane.x * forward + localVelPlane.y * right;
	glm::vec3 velocityY = localVelVertical * up;
	
	m_velocity = velocityXZ + velocityY;
	
	glm::vec3 radius(WIDTH / 2, WIDTH / 2, WIDTH / 2);
	radius[(int)m_down / 2] = HEIGHT / 2;
	
	glm::vec3 move = m_velocity * dt;
	
	//Checks for intersections with gravity corners along the move vector
	if (m_gravityTransitionMode == TransitionMode::None && m_onGround)
	{
		ClippingArgs clippingArgs;
		clippingArgs.move = move;
		clippingArgs.aabbMin = m_position - radius;
		clippingArgs.aabbMax = m_position + radius;
		if (const GravityCorner* corner = world.FindGravityCorner(clippingArgs, m_down))
		{
			//A gravity corner was found, this code begins transitioning to another gravity
			
			const Dir newDown = corner->down1 == m_down ? corner->down2 : corner->down1;
			const glm::vec3 newUpVec = -DirectionVector(newDown);
			const glm::quat rotationDiff = glm::angleAxis(eg::HALF_PI, glm::cross(up, newUpVec));
			
			//Finds the new position of the player after the transition. This is done by transforming 
			// the player's position to corner local space, swapping x and y, and transforming back.
			const glm::mat3 cornerRotation = corner->MakeRotationMatrix();
			glm::vec3 cornerL = glm::transpose(cornerRotation) * (m_position - corner->position);
			if (corner->down2 == m_down)
				cornerL.x = std::max(cornerL.x, 1.5f);
			else
				cornerL.y = std::max(cornerL.y, 1.5f);
			std::swap(cornerL.x, cornerL.y);
			m_newPosition = (cornerRotation * cornerL) + corner->position + newUpVec * 0.001f;
			
			//Converts the camera's yaw and pitch to the new gravity
			glm::vec3 f = glm::inverse(GetDownCorrection(newDown)) * (rotationDiff * (m_rotation * glm::vec3(0, 0, -1)));
			glm::vec2 f2 = glm::normalize(glm::vec2(f.x, f.z));
			m_rotationPitch = std::asin(f.y);
			m_rotationYaw = std::atan2(-f2.x, -f2.y);
			
			//Sets up the old and new rotations to be interpolated between in the transition
			m_oldRotation = m_rotation;
			m_newRotation = GetRotation(m_rotationYaw, m_rotationPitch, newDown);
			
			//Rotates the velocity vector and reassigns some other stuff
			m_velocity = rotationDiff * m_velocity;
			m_down = newDown;
			move = { };
			m_oldEyePosition = m_eyePosition;
			m_gravityTransitionMode = TransitionMode::Corner;
			m_transitionTime = 0;
		}
	}
	
	//Checks for gravity switches
	if (m_gravityTransitionMode == TransitionMode::None && m_onGround)
	{
		glm::vec3 feetPos = m_position - up * (HEIGHT / 2);
		eg::AABB searchAABB(feetPos - radius, feetPos + radius);
		if (std::shared_ptr<GravitySwitchEntity> gravitySwitch = world.FindGravitySwitch(searchAABB, m_down))
		{
			if (eg::IsButtonDown(eg::Button::E) || eg::IsButtonDown(eg::Button::CtrlrX))
			{
				Dir newDown = gravitySwitch->Up();
				
				m_rotationYaw += eg::PI;
				m_rotationPitch = -m_rotationPitch;
				m_oldRotation = m_rotation;
				m_newRotation = GetRotation(m_rotationYaw, m_rotationPitch, newDown);
				
				m_newPosition = m_position;// - up * 1.0f;
				m_velocity = glm::vec3(0);
				m_down = newDown;
				move = { };
				m_oldEyePosition = m_eyePosition;
				m_gravityTransitionMode = TransitionMode::Fall;
				m_transitionTime = 0;
			}
		}
	}
	
	//Clipping
	m_onGround = false;
	constexpr int MAX_CLIP_ITERATIONS = 10;
	for (int i = 0; i < MAX_CLIP_ITERATIONS; i++)
	{
		if (glm::length2(move) < 1E-6f)
			break;
		
		ClippingArgs clippingArgs;
		clippingArgs.move = move;
		clippingArgs.aabbMin = m_position - radius;
		clippingArgs.aabbMax = m_position + radius;
		clippingArgs.clipDist = 1;
		world.CalcClipping(clippingArgs);
		
		glm::vec3 clippedMove = move * (clippingArgs.clipDist * 0.99f);
		m_position += clippedMove;
		
		if (clippingArgs.clipDist > 0.99999f)
			break;
		
		move -= clippedMove;
		
		glm::vec3 n = clippingArgs.colPlaneNormal;
		move -= n * glm::dot(move, n);
		
		const float nDotUp = glm::dot(n, up);
		if (std::abs(nDotUp) > 0.9f)
		{
			m_velocity -= up * glm::dot(m_velocity, up);
		}
		if (nDotUp > 0.75f)
		{
			m_onGround = true;
		}
	}
	
	m_eyePosition = m_position + up * EYE_OFFSET;
	if (m_gravityTransitionMode == TransitionMode::Fall)
	{
		m_eyePosition = glm::mix(m_oldEyePosition, m_eyePosition, TransitionInterpol());
	}
}

void Player::GetViewMatrix(glm::mat4& matrixOut, glm::mat4& inverseMatrixOut) const
{
	const glm::mat4 rotationMatrix = glm::mat4_cast(m_rotation);
	matrixOut = glm::transpose(rotationMatrix) * glm::translate(glm::mat4(1.0f), -m_eyePosition);
	inverseMatrixOut = glm::translate(glm::mat4(1.0f), m_eyePosition) * rotationMatrix;
}

void Player::DebugDraw()
{
	ImGui::Text("Position: %.2f, %.2f, %.2f", m_eyePosition.x, m_eyePosition.y, m_eyePosition.z);
	
	const glm::vec3 forward = m_rotation * glm::vec3(0, 0, -1);
	const glm::vec3 absForward = glm::abs(forward);
	int maxDir = 0;
	for (int i = 1; i < 3; i++)
	{
		if (absForward[i] > absForward[maxDir])
			maxDir = i;
	}
	
	const char* dirNames = "XYZ";
	ImGui::Text("Facing: %c%c", forward[maxDir] < 0 ? '-' : '+', dirNames[maxDir]);
}
