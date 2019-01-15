#include "Player.hpp"

const float WALK_SPEED = 4.0f;
const float ACCEL_TIME = 0.1f;
const float DEACCEL_TIME = 0.05f;
const float GRAVITY = 20;
const float JUMP_HEIGHT = 1.0f;
const float FALLING_GRAVITY_RAMP = 0.1f;
const float MAX_VERTICAL_SPEED = 100;

const float JUMP_ACCEL = std::sqrt(2.0f * JUMP_HEIGHT * GRAVITY);
const float ACCEL_AMOUNT = WALK_SPEED / ACCEL_TIME;
const float DEACCEL_AMOUNT = WALK_SPEED / DEACCEL_TIME;

Player::Player()
	: m_position(1, 2, 1), m_velocity(0.0f)
{
	
}

void Player::Update(World& world, float dt)
{
	const float MOUSE_SENSITIVITY = -0.005f;
	
	//Updates the camera's rotation
	glm::vec2 mouseDelta = glm::vec2(eg::CursorPos() - eg::PrevCursorPos()) * MOUSE_SENSITIVITY;
	m_rotationYaw += mouseDelta.x;
	m_rotationPitch = glm::clamp(m_rotationPitch + mouseDelta.y, -eg::HALF_PI * 0.95f, eg::HALF_PI * 0.95f);
	
	m_rotationMatrix = glm::rotate(glm::mat4(1.0f), m_rotationYaw, glm::vec3(0, 1, 0)) *
	                   glm::rotate(glm::mat4(1.0f), m_rotationPitch, glm::vec3(1, 0, 0));
	
	glm::vec3 up = { 0, 1, 0 }; // gravityModes[m_gravityMode].up;
	
	//Constructs the forward and right movement vectors
	auto GetDirVector = [&] (const glm::vec3& v)
	{
		glm::vec3 v1 = m_rotationMatrix * v;
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
	
	const glm::vec3 radius(WIDTH / 2, HEIGHT / 2, WIDTH / 2);
	
	glm::vec3 move = m_velocity * dt;
	
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
		
		if (glm::dot(n, up) > 0.75f)
		{
			m_onGround = true;
		}
	}
	
	if (m_onGround)
		m_velocity -= up * glm::dot(m_velocity, up);
}

void Player::GetViewMatrix(glm::mat4& matrixOut, glm::mat4& inverseMatrixOut) const
{
	glm::vec3 eyePos = m_position + glm::vec3(0, EYE_HEIGHT - HEIGHT / 2, 0);
	matrixOut = glm::mat4(glm::transpose(m_rotationMatrix)) * glm::translate(glm::mat4(1.0f), -eyePos);
	inverseMatrixOut = glm::mat4(m_rotationMatrix) * glm::translate(glm::mat4(1.0f), eyePos);
}
