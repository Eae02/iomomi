#include "Player.hpp"

#include <imgui.h>

const float WALK_SPEED = 4.0f;
const float ACCEL_TIME = 0.1f;
const float DEACCEL_TIME = 0.05f;
const float GRAVITY = 20;
const float JUMP_HEIGHT = 1.1f;
const float FALLING_GRAVITY_RAMP = 0.1f;
const float MAX_VERTICAL_SPEED = 100;

const float JUMP_ACCEL = std::sqrt(2.0f * JUMP_HEIGHT * GRAVITY);
const float ACCEL_AMOUNT = WALK_SPEED / ACCEL_TIME;
const float DEACCEL_AMOUNT = WALK_SPEED / DEACCEL_TIME;

Player::Player()
	: m_position(1, 2, 1), m_velocity(0.0f)
{
	
}

const glm::vec3 leftDirs[6] =
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
	constexpr float EYE_OFFSET = EYE_HEIGHT - HEIGHT / 2;
	
	glm::vec3 up = -DirectionVector(m_down);
	
	if (m_currentCorner != nullptr)
	{
		constexpr float TRANSITION_DURATION = 0.2f;
		m_transitionTime += dt / TRANSITION_DURATION;
		
		if (m_transitionTime > 1.0f)
		{
			m_position = m_newPosition;
			m_rotation = m_newRotation;
			m_currentCorner = nullptr;
		}
		else
		{
			float a = glm::smoothstep(0.0f, 1.0f, m_transitionTime);
			m_eyePosition = glm::mix(m_oldEyePosition, m_newPosition + up * EYE_OFFSET, a);
			m_rotation = glm::slerp(m_oldRotation, m_newRotation, a);
			return;
		}
	}
	
	const float MOUSE_SENSITIVITY = -0.005f;
	
	glm::mat3 oriRotationMatrix(leftDirs[(int)m_down], up, glm::cross(leftDirs[(int)m_down], up));
	
	//Updates the camera's rotation
	m_rotationYaw += eg::CursorDeltaX() * MOUSE_SENSITIVITY;
	m_rotationPitch = glm::clamp(m_rotationPitch + eg::CursorDeltaY() * MOUSE_SENSITIVITY, -eg::HALF_PI * 0.95f, eg::HALF_PI * 0.95f);
	
	m_rotation = GetRotation(m_rotationYaw, m_rotationPitch, m_down);
	
	//Constructs the forward and right movement vectors
	auto GetDirVector = [&] (const glm::vec3& v)
	{
		glm::vec3 v1 = m_rotation * v;
		return glm::normalize(v1 - glm::dot(v1, up) * up);
	};
	const glm::vec3 forward = GetDirVector(glm::vec3(0, 0, -1));
	const glm::vec3 right = GetDirVector(glm::vec3(1, 0, 0));
	
	//Changes the basis for the velocity vector to local space
	float localVelVertical = glm::dot(up, m_velocity);
	glm::vec2 localVelPlane(glm::dot(forward, m_velocity), glm::dot(right, m_velocity));
	glm::vec2 localAccPlane(0.0f);
	
	const bool moveForward = eg::IsButtonDown(eg::Button::W);
	const bool moveBack =    eg::IsButtonDown(eg::Button::S);
	const bool moveLeft =    eg::IsButtonDown(eg::Button::A);
	const bool moveRight =   eg::IsButtonDown(eg::Button::D);
	
	if (moveForward == moveBack)
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
	else //if (moveBack)
	{
		localAccPlane -= glm::vec2(1, 0);
	}
	
	if (moveRight == moveLeft)
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
	else //if (moveRight)
	{
		localAccPlane += glm::vec2(0, 1);
	}
	
	float accelMag = glm::length(localAccPlane);
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
	float speed = glm::length(localVelPlane);
	if (speed > WALK_SPEED)
	{
		localVelPlane *= WALK_SPEED / speed;
	}
	
	//Updates vertical velocity.
	if (eg::IsButtonDown(eg::Button::Space) && m_onGround)
	{
		localVelVertical = JUMP_ACCEL;
	}
	else
	{
		float gravity = GRAVITY;
		gravity *= 1.0f + FALLING_GRAVITY_RAMP * glm::clamp(-localVelVertical, 0.0f, 1.0f);
		
		localVelVertical = std::max(localVelVertical - gravity * dt, -MAX_VERTICAL_SPEED);
	}
	
	//Reconstructs the world velocity vector.
	glm::vec3 velocityXZ = localVelPlane.x * forward + localVelPlane.y * right;
	glm::vec3 velocityY = localVelVertical * up;
	
	m_velocity = velocityXZ + velocityY;
	
	glm::vec3 radius(WIDTH / 2, WIDTH / 2, WIDTH / 2);
	radius[(int)m_down / 2] = HEIGHT / 2;
	
	glm::vec3 move = m_velocity * dt;
	
	if (m_onGround)
	{
		ClippingArgs clippingArgs;
		clippingArgs.move = move;
		clippingArgs.aabbMin = m_position - radius;
		clippingArgs.aabbMax = m_position + radius;
		if (const GravityCorner* corner = world.FindGravityCorner(clippingArgs, m_down))
		{
			const Dir newDown = corner->down1 == m_down ? corner->down2 : corner->down1;
			const glm::vec3 newUpVec = -DirectionVector(newDown);
			
			const glm::mat3 cornerRotation = corner->MakeRotationMatrix();
			glm::vec3 cornerL = glm::transpose(cornerRotation) * (m_position - corner->position);
			if (corner->down2 == m_down)
				cornerL.x = std::max(cornerL.x, 1.5f);
			else
				cornerL.y = std::max(cornerL.y, 1.5f);
			std::swap(cornerL.x, cornerL.y);
			m_newPosition = (cornerRotation * cornerL) + corner->position + newUpVec * 0.001f;
			
			const glm::quat rotationDiff = glm::angleAxis(eg::HALF_PI, glm::cross(up, newUpVec));
			
			glm::vec3 f = glm::inverse(GetDownCorrection(newDown)) * (rotationDiff * (m_rotation * glm::vec3(0, 0, -1)));
			glm::vec2 f2 = glm::normalize(glm::vec2(f.x, f.z));
			
			m_rotationPitch = std::asin(f.y);
			m_rotationYaw = std::atan2(-f2.x, -f2.y);
			
			m_oldRotation = m_rotation;
			m_newRotation = GetRotation(m_rotationYaw, m_rotationPitch, newDown);
			
			m_velocity = rotationDiff * m_velocity;
			m_currentCorner = corner;
			m_down = newDown;
			move = { };
			m_oldEyePosition = m_eyePosition;
			m_transitionTime = 0;
		}
	}
	
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
		
		glm::vec3 clippedMove = move * clippingArgs.clipDist;
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
}

void Player::GetViewMatrix(glm::mat4& matrixOut, glm::mat4& inverseMatrixOut) const
{
	glm::mat4 rotationMatrix = glm::mat4_cast(m_rotation);
	matrixOut = glm::transpose(rotationMatrix) * glm::translate(glm::mat4(1.0f), -m_eyePosition);
	inverseMatrixOut = rotationMatrix * glm::translate(glm::mat4(1.0f), m_eyePosition);
}

void Player::DebugDraw()
{
	ImGui::Text("Position: %.2f, %.2f, %.2f", m_eyePosition.x, m_eyePosition.y, m_eyePosition.z);
	
	glm::vec3 forward = m_rotation * glm::vec3(0, 0, -1);
	glm::vec3 absForward = glm::abs(forward);
	int maxDir = 0;
	for (int i = 1; i < 3; i++)
	{
		if (absForward[i] > absForward[maxDir])
			maxDir = i;
	}
	
	const char* dirNames = "XYZ";
	ImGui::Text("Facing: %c%c", forward[maxDir] < 0 ? '-' : '+', dirNames[maxDir]);
}
