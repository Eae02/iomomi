#include "Player.hpp"
#include "Entities/ECWallMounted.hpp"
#include "Entities/ECFloorButton.hpp"
#include "Entities/ECActivator.hpp"
#include "Entities/ECInteractable.hpp"
#include "../Graphics/Materials/GravityCornerLightMaterial.hpp"
#include "Entities/ECPlatform.hpp"

#include <imgui.h>

//Constants related to player physics
static constexpr float WALK_SPEED = 4.0f;
static constexpr float ACCEL_TIME = 0.1f;
static constexpr float DEACCEL_TIME = 0.05f;
static constexpr float GRAVITY = 20;
static constexpr float JUMP_HEIGHT = 1.1f;
static constexpr float FALLING_GRAVITY_RAMP = 0.1f;
static constexpr float MAX_VERTICAL_SPEED = 300;

//Constants which derive from previous
static const float JUMP_ACCEL = std::sqrt(2.0f * JUMP_HEIGHT * GRAVITY);
static const float ACCEL_AMOUNT = WALK_SPEED / ACCEL_TIME;
static constexpr float DEACCEL_AMOUNT = WALK_SPEED / DEACCEL_TIME;
static constexpr float EYE_OFFSET = Player::EYE_HEIGHT - Player::HEIGHT / 2;

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
	eg::Entity* currentPlatform = m_currentPlatform.Get();
	
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
		m_onGround = false;
	}
	else if (currentPlatform == nullptr)
	{
		float gravity = GRAVITY;
		gravity *= 1.0f + FALLING_GRAVITY_RAMP * glm::clamp(-localVelVertical, 0.0f, 1.0f);
		
		localVelVertical = std::max(localVelVertical - gravity * dt, -MAX_VERTICAL_SPEED);
	}
	
	//Reconstructs the world velocity vector
	glm::vec3 velocityXZ = localVelPlane.x * forward + localVelPlane.y * right;
	glm::vec3 velocityY = localVelVertical * up;
	
	m_velocity = velocityXZ + velocityY;
	
	m_radius = glm::vec3(WIDTH / 2, WIDTH / 2, WIDTH / 2);
	m_radius[(int)m_down / 2] = HEIGHT / 2;
	
	glm::vec3 move = m_velocity * dt;
	
	//Without this, onGround can turn off at high frame rates because
	// the player is not being pushed into the ground hard enough.
	if (m_onGround && currentPlatform == nullptr)
		move -= up * 0.001f;
	
	//Checks for intersections with gravity corners along the move vector
	if (m_gravityTransitionMode == TransitionMode::None && m_onGround && !m_isCarrying)
	{
		ClippingArgs clippingArgs;
		clippingArgs.move = move;
		clippingArgs.aabb = GetAABB();
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
			
			glm::vec3 activatePos = cornerRotation * glm::vec3(0.3f, 0.3f, cornerL.z) + corner->position;
			GravityCornerLightMaterial::instance.Activate(activatePos);
		}
	}
	
	//Checks for interactable entities
	if (m_gravityTransitionMode == TransitionMode::None)
	{
		static eg::EntitySignature interactableSig = eg::EntitySignature::Create<ECInteractable>();
		eg::Entity* interactableEntity = nullptr;
		int bestInteractPriority = 0;
		for (eg::Entity& entity : world.EntityManager().GetEntitySet(interactableSig))
		{
			int thisPriority = entity.GetComponent<ECInteractable>().checkInteraction(entity, *this);
			if (thisPriority > bestInteractPriority)
			{
				interactableEntity = &entity;
				bestInteractPriority = thisPriority;
			}
		}
		
		if (interactableEntity != nullptr)
		{
			ECInteractable& interactable = interactableEntity->GetComponent<ECInteractable>();
			if ((eg::IsButtonDown(eg::Button::E) && !eg::WasButtonDown(eg::Button::E)) ||
				(eg::IsButtonDown(eg::Button::CtrlrX) && !eg::WasButtonDown(eg::Button::CtrlrX)))
			{
				interactable.interact(*interactableEntity, *this);
			}
			else
			{
				//TODO: Show help text
			}
		}
	}
	
	//Activates floor buttons which the player is moving into
	for (eg::Entity& floorButtonEntity : world.EntityManager().GetEntitySet(ECFloorButton::EntitySignature))
	{
		glm::vec3 toButton = glm::normalize(eg::GetEntityPosition(floorButtonEntity) - m_position);
		if (glm::dot(toButton, glm::normalize(move)) > 0.1f &&
		    ECFloorButton::GetAABB(floorButtonEntity).Intersects(GetAABB()))
		{
			floorButtonEntity.HandleMessage(ActivateMessage());
		}
	}
	
	m_onGround = false;
	
	const int downDim = (int)m_down / 2;
	const int downSign = ((int)m_down % 2) ? -1 : 1;
	
	//Moves the player with the current platform, if one is set 
	if (currentPlatform != nullptr)
	{
		glm::vec3 platformMove = currentPlatform->GetComponent<ECPlatform>().moveDelta;
		
		//The player's position should be slightly above the platform (PLAYER_PLATFORM_MARGIN)
		// at all times to prevent the player from falling through the platform when it moves up.
		constexpr float PLAYER_PLATFORM_MARGIN = 0.05f;
		const float fromPlatformY = m_position[downDim] - ECPlatform::GetPosition(*currentPlatform)[downDim];
		platformMove -= std::min(fromPlatformY - (HEIGHT * 0.5f) - PLAYER_PLATFORM_MARGIN, 0.0f) * up;
		
		ClipAndMove(world, platformMove, true);
		m_onGround = true; //Always on ground if on a platform
	}
	
	ClipAndMove(world, move, false);
	
	//Searches for a platform under the player's feet
	const glm::vec3 feetPos = m_position + glm::vec3(DirectionVector(m_down)) * (HEIGHT * 0.5f);
	glm::vec3 platformSearchMax = feetPos + 0.05f;
	glm::vec3 platformSearchMin = feetPos - 0.05f;
	platformSearchMax[downDim] = feetPos[downDim];
	platformSearchMin[downDim] = feetPos[downDim] + std::max(move[downDim] * downSign, 0.2f) * downSign;
	currentPlatform = ECPlatform::FindPlatform(eg::AABB(platformSearchMin, platformSearchMax), world.EntityManager());
	if (currentPlatform == nullptr)
		m_currentPlatform = { };
	else
		m_currentPlatform = *currentPlatform;
	
	//Updates the eye position
	m_eyePosition = m_position + up * EYE_OFFSET;
	if (m_gravityTransitionMode == TransitionMode::Fall)
	{
		m_eyePosition = glm::mix(m_oldEyePosition, m_eyePosition, TransitionInterpol());
	}
}

void Player::ClipAndMove(const World& world, glm::vec3 move, bool skipPlatforms)
{
	const glm::vec3 up = -DirectionVector(m_down);
	constexpr int MAX_CLIP_ITERATIONS = 10;
	for (int i = 0; i < MAX_CLIP_ITERATIONS; i++)
	{
		if (glm::length2(move) < 1E-6f)
			break;
		
		ClippingArgs clippingArgs;
		clippingArgs.move = move;
		clippingArgs.aabb = GetAABB();
		clippingArgs.clipDist = 1;
		clippingArgs.skipPlatforms = skipPlatforms;
		CalcWorldClipping(world, clippingArgs);
		
		glm::vec3 clippedMove = move * clippingArgs.clipDist;
		m_position += clippedMove;
		
		const float nDotUp = glm::dot(clippingArgs.colPlaneNormal, up);
		if (std::abs(nDotUp) > 0.9f)
		{
			m_velocity -= up * glm::dot(m_velocity, up);
		}
		if (nDotUp > 0.75f)
		{
			m_onGround = true;
		}
		
		if (clippingArgs.clipDist > 0.9999f)
			break;
		
		move -= clippedMove;
		
		move -= clippingArgs.colPlaneNormal * glm::dot(move, clippingArgs.colPlaneNormal);
	}
	
	m_position += CalcWorldCollisionCorrection(world, GetAABB());
}

void Player::FlipDown()
{
	Dir newDown = OppositeDir(m_down);
	
	m_rotationYaw = -m_rotationYaw + eg::PI;
	m_rotationPitch = -m_rotationPitch;
	m_oldRotation = m_rotation;
	m_newRotation = GetRotation(m_rotationYaw, m_rotationPitch, newDown);
	
	m_newPosition = m_position;
	m_velocity = glm::vec3(0);
	m_down = newDown;
	m_oldEyePosition = m_eyePosition;
	m_gravityTransitionMode = TransitionMode::Fall;
	m_transitionTime = 0;
}

void Player::GetViewMatrix(glm::mat4& matrixOut, glm::mat4& inverseMatrixOut) const
{
	const glm::mat4 rotationMatrix = glm::mat4_cast(m_rotation);
	matrixOut = glm::transpose(rotationMatrix) * glm::translate(glm::mat4(1.0f), -m_eyePosition);
	inverseMatrixOut = glm::translate(glm::mat4(1.0f), m_eyePosition) * rotationMatrix;
}

glm::vec3 Player::Forward() const
{
	return m_rotation * glm::vec3(0, 0, -1);
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

void Player::Reset()
{
	m_down = Dir::NegY;
	m_onGround = false;
	m_isCarrying = false;
	m_gravityTransitionMode = TransitionMode::None;
	m_currentPlatform = { };
}

glm::vec3 Player::FeetPosition() const
{
	return m_position + glm::vec3(DirectionVector(m_down)) * (HEIGHT * 0.5f * 0.95f);
}
