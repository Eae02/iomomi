#include "Player.hpp"
#include "Entities/EntInteractable.hpp"
#include "../Graphics/Materials/GravityCornerLightMaterial.hpp"
#include "../Settings.hpp"
#include "Entities/EntTypes/FloorButtonEnt.hpp"

#include <imgui.h>

static float* walkSpeed       = eg::TweakVarFloat("pl_walk_speed",        4.0f,  0.0f);
static float* swimSpeed       = eg::TweakVarFloat("pl_swim_speed",        5.0f,  0.0f);
static float* walkAccelTime   = eg::TweakVarFloat("pl_walk_atime",        0.1f,  0.0f);
static float* swimAccelTime   = eg::TweakVarFloat("pl_swim_atime",        0.4f,  0.0f);
static float* walkDeaccelTime = eg::TweakVarFloat("pl_walk_datime",       0.05f, 0.0f);
static float* swimDrag        = eg::TweakVarFloat("pl_swim_drag",         5.0f,  0.0f);
static float* playerGravity   = eg::TweakVarFloat("pl_gravity",           20,    0.0f);
static float* jumpHeight      = eg::TweakVarFloat("pl_jump_height",       1.1f,  0.0f);
static float* fallGravityRamp = eg::TweakVarFloat("pl_fall_gravity_ramp", 0.1f,  0.0f);
static float* maxYSpeed       = eg::TweakVarFloat("pl_max_yspeed",        300,   0.0f);

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

void Player::Update(World& world, float dt, bool underwater)
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
	
	if (m_gravityTransitionMode == TransitionMode::None && !eg::IsButtonDown(eg::Button::F8))
	{
		glm::vec2 rotationDelta = glm::vec2(eg::CursorPosDelta()) * -settings.lookSensitivityMS;
		
		rotationDelta -= eg::InputState::Current().RightAnalogValue() * (settings.lookSensitivityGP * dt);
		
		if (settings.lookInvertY)
			rotationDelta.y = -rotationDelta.y;
		
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
	
	const glm::vec3 forward = m_rotation * glm::vec3(0, 0, -1);
	const glm::vec3 right = m_rotation * glm::vec3(1, 0, 0);
	
	const bool moveForward = eg::IsButtonDown(eg::Button::W) || eg::IsButtonDown(eg::Button::CtrlrDPadUp);
	const bool moveBack =    eg::IsButtonDown(eg::Button::S) || eg::IsButtonDown(eg::Button::CtrlrDPadDown);
	const bool moveLeft =    eg::IsButtonDown(eg::Button::A) || eg::IsButtonDown(eg::Button::CtrlrDPadLeft);
	const bool moveRight =   eg::IsButtonDown(eg::Button::D) || eg::IsButtonDown(eg::Button::CtrlrDPadRight);
	const bool moveUp =      eg::IsButtonDown(eg::Button::Space) || eg::IsButtonDown(eg::Button::CtrlrA);
	
	if (underwater)
	{
		m_onGround = false;
		
		glm::vec3 accel;
		if (moveForward)
			accel += forward;
		if (moveBack)
			accel -= forward;
		if (moveLeft)
			accel -= right;
		if (moveRight)
			accel += right;
		if (moveUp)
			accel += up;
		
		if (glm::length2(accel) > 0.01f)
		{
			accel = glm::normalize(accel) * *swimSpeed / *swimAccelTime;
			m_velocity += accel * dt;
		}
		
		//Caps the local velocity to the walking speed
		const float speed = glm::length(m_velocity);
		if (speed > *swimSpeed)
		{
			m_velocity *= *swimSpeed / speed;
		}
		
		m_velocity -= m_velocity * std::min(dt * *swimDrag, 1.0f);
	}
	
	//Constructs the forward and right movement vectors.
	//These, along with up, are the basis vectors for local space.
	const glm::vec3 forwardPlane = glm::normalize(forward - glm::dot(forward, up) * up);
	const glm::vec3 rightPlane = glm::normalize(right - glm::dot(right, up) * up);
	
	//Finds the velocity vector in local space
	float localVelVertical = glm::dot(up, m_velocity);
	glm::vec2 localVelPlane(glm::dot(forwardPlane, m_velocity), glm::dot(rightPlane, m_velocity));
	glm::vec2 localAccPlane(-eg::AxisValue(eg::ControllerAxis::LeftY), eg::AxisValue(eg::ControllerAxis::LeftX));
	
	if (m_gravityTransitionMode == TransitionMode::None && !underwater)
	{
		const float accelAmount = *walkSpeed / *walkAccelTime;
		const float deaccelAmount = *walkSpeed / *walkDeaccelTime;
		
		if (moveForward == moveBack && std::abs(localAccPlane.x) < 1E-4f)
		{
			if (localVelPlane.x < 0)
			{
				localVelPlane.x += dt * deaccelAmount;
				if (localVelPlane.x > 0)
					localVelPlane.x = 0.0f;
			}
			if (localVelPlane.x > 0)
			{
				localVelPlane.x -= dt * deaccelAmount;
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
				localVelPlane.y += dt * deaccelAmount;
				if (localVelPlane.y > 0)
					localVelPlane.y = 0.0f;
			}
			if (localVelPlane.y > 0)
			{
				localVelPlane.y -= dt * deaccelAmount;
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
			localAccPlane *= accelAmount / accelMag;
			
			//Increases the localAccPlane if the player is being accelerated in the opposite direction of it's current
			// velocity. This makes direction changes snappier.
			if (glm::dot(localAccPlane, localVelPlane) < 0.0f)
				localAccPlane *= 4.0f;
			
			localVelPlane += localAccPlane * dt;
		}
		
		//Caps the local velocity to the walking speed
		const float speed = glm::length(localVelPlane);
		if (speed > *walkSpeed)
		{
			localVelPlane *= *walkSpeed / speed;
		}
	}
	
	const float jumpAccel = std::sqrt(2.0f * *jumpHeight * *playerGravity);
	
	//Updates vertical velocity
	if (moveUp && m_gravityTransitionMode == TransitionMode::None && m_onGround)
	{
		localVelVertical = jumpAccel;
		m_onGround = false;
	}
	else if (currentPlatform == nullptr)
	{
		float gravity = *playerGravity;
		gravity *= 1.0f + *fallGravityRamp * glm::clamp(-localVelVertical, 0.0f, 1.0f);
		if (underwater)
			gravity *= 0.1f;
		
		localVelVertical = std::max(localVelVertical - gravity * dt, -*maxYSpeed);
	}
	
	if (m_wasUnderwater && !underwater && localVelVertical > 0)
	{
		localVelVertical = jumpAccel;
	}
	
	//Reconstructs the world velocity vector
	glm::vec3 velocityXZ = localVelPlane.x * forwardPlane + localVelPlane.y * rightPlane;
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
		if (const GravityCorner* corner = world.FindGravityCorner(GetAABB(), move, m_down))
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
		EntInteractable* interactableEntity = nullptr;
		int bestInteractPriority = 0;
		world.entManager.ForEachWithFlag(EntTypeFlags::Interactable, [&] (Ent& entity)
		{
			EntInteractable& interactable = dynamic_cast<EntInteractable&>(entity);
			int thisPriority = interactable.CheckInteraction(*this);
			if (thisPriority > bestInteractPriority)
			{
				interactableEntity = &interactable;
				bestInteractPriority = thisPriority;
			}
		});
		
		if (interactableEntity != nullptr)
		{
			if ((eg::IsButtonDown(eg::Button::E) && !eg::WasButtonDown(eg::Button::E)) ||
				(eg::IsButtonDown(eg::Button::CtrlrX) && !eg::WasButtonDown(eg::Button::CtrlrX)))
			{
				interactableEntity->Interact(*this);
			}
			else
			{
				//TODO: Show help text
			}
		}
	}
	
	//Activates floor buttons which the player is moving into
	world.entManager.ForEachOfType<FloorButtonEnt>([&] (FloorButtonEnt& floorButtonEntity) 
	{
		glm::vec3 toButton = glm::normalize(floorButtonEntity.Pos() - m_position);
		if (glm::dot(toButton, glm::normalize(move)) > 0.1f &&
		    floorButtonEntity.GetAABB().Intersects(GetAABB()))
		{
			floorButtonEntity.Activate();
		}
	});
	/*
	//Checks for force fields
	eg::AABB forceFieldAABB = GetAABB();
	forceFieldAABB.min += forceFieldAABB.Size() * 0.2f;
	forceFieldAABB.max -= forceFieldAABB.Size() * 0.2f;
	std::optional<Dir> forceFieldGravity = ECForceField::CheckIntersection(world.EntityManager(), forceFieldAABB);
	if (m_gravityTransitionMode == TransitionMode::None && forceFieldGravity.has_value() && m_down != *forceFieldGravity)
	{
		m_down = *forceFieldGravity;
		m_gravityTransitionMode = TransitionMode::Fall;
		m_transitionTime = 0;
		m_velocity = glm::vec3(0);
		m_oldEyePosition = m_eyePosition;
		m_oldRotation = m_rotation;
		m_newRotation = GetRotation(m_rotationYaw, m_rotationPitch, m_down);
	}*/
	
	m_onGround = false;
	
	const int downDim = (int)m_down / 2;
	const int downSign = ((int)m_down % 2) ? -1 : 1;
	/*
	//Moves the player with the current platform, if one is set 
	if (currentPlatform != nullptr)
	{
		glm::vec3 platformMove = currentPlatform->GetComponent<ECPlatform>().moveDelta;
		
		//The player's position should be slightly above the platform (PLAYER_PLATFORM_MARGIN)
		// at all times to prevent the player from falling through the platform when it moves up.
		float PLAYER_PLATFORM_MARGIN = (up[downDim] < 0) ? 0.3f : 0.1f;
		
		float correction =
			-m_position[downDim] +
			ECPlatform::GetPosition(*currentPlatform)[downDim] +
			((HEIGHT * 0.5f) + PLAYER_PLATFORM_MARGIN) * up[downDim];
		
		if (correction * up[downDim] < 0)
			correction = 0;
		
		platformMove[downDim] += correction;
		
		ClipAndMove(world, platformMove, true);
		m_onGround = true; //Always on ground if on a platform
	}
	*/
	ClipAndMove(world, move, false);
	
	//Searches for a platform under the player's feet
	const glm::vec3 feetPos = m_position + glm::vec3(DirectionVector(m_down)) * (HEIGHT * 0.5f);
	glm::vec3 platformSearchMax = feetPos + 0.1f;
	glm::vec3 platformSearchMin = feetPos - 0.1f;
	float platformSearchDown1 = feetPos[downDim];
	float platformSearchDown2 = feetPos[downDim] + std::max(move[downDim] * downSign, 0.3f) * downSign;
	platformSearchMax[downDim] = std::max(platformSearchDown1, platformSearchDown2);
	platformSearchMin[downDim] = std::min(platformSearchDown1, platformSearchDown2);
	currentPlatform = nullptr;//ECPlatform::FindPlatform(eg::AABB(platformSearchMin, platformSearchMax), world.EntityManager());
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
	
	m_wasUnderwater = underwater;
}

void Player::ClipAndMove(const World& world, glm::vec3 move, bool skipPlatforms)
{
	glm::vec3 fromEllipsoidSpace = m_radius;
	glm::vec3 toEllipsoidSpace = 1.0f / m_radius;
	glm::vec3 velocityES = m_velocity * toEllipsoidSpace;
	glm::vec3 moveES = move * toEllipsoidSpace;
	
	const glm::vec3 up = -DirectionVector(m_down);
	constexpr int MAX_CLIP_ITERATIONS = 3;
	for (int i = 0; i < MAX_CLIP_ITERATIONS; i++)
	{
		if (glm::length2(moveES) < 1E-6f)
			break;
		
		ClippingArgs clippingArgs;
		clippingArgs.move = moveES * fromEllipsoidSpace;
		clippingArgs.ellipsoid = eg::CollisionEllipsoid(m_position, m_radius);
		world.CalcClipping(clippingArgs, m_down);
		
		glm::vec3 positionES = m_position * toEllipsoidSpace;
		glm::vec3 newPosition = positionES;
		
		glm::vec3 dstPosition = positionES + moveES;
		
		if (!clippingArgs.collisionInfo.collisionFound)
		{
			m_position = dstPosition * fromEllipsoidSpace;
			break;
		}
		
		const float veryCloseDist = 0.001f;
		float moveDist = clippingArgs.collisionInfo.distance - veryCloseDist;
		if (moveDist >= 0.0f)
		{
			newPosition = positionES + moveES * moveDist;
			
			clippingArgs.collisionInfo.positionES -= veryCloseDist * glm::normalize(moveES);
		}
		
		const eg::Plane slidePlane(clippingArgs.collisionInfo.positionES - newPosition, clippingArgs.collisionInfo.positionES);
		
		if (glm::dot(slidePlane.GetNormal(), up) > 0.75f)
		{
			m_onGround = true;
		}
		
		velocityES -= glm::dot(velocityES, slidePlane.GetNormal()) * slidePlane.GetNormal();
		
		glm::vec3 newDstPosition = dstPosition - slidePlane.GetDistanceToPoint(dstPosition) * slidePlane.GetNormal();
		
		m_position = newPosition * fromEllipsoidSpace;
		moveES = newDstPosition - clippingArgs.collisionInfo.positionES;
	}
	
	m_velocity = velocityES * fromEllipsoidSpace;
}

void Player::FlipDown()
{
	Dir newDown = OppositeDir(m_down);
	
	m_rotationYaw = -m_rotationYaw + eg::PI;
	m_rotationPitch = -m_rotationPitch;
	m_oldRotation = m_rotation;
	m_newRotation = GetRotation(m_rotationYaw, m_rotationPitch, newDown);
	
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
	
	ImGui::Text("Platform: %p", (void*)m_currentPlatform.Get());
}

void Player::Reset()
{
	m_position = glm::vec3(0.0f);
	m_velocity = glm::vec3(0.0f);
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
