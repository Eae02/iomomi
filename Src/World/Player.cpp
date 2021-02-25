#include "Player.hpp"
#include "Entities/EntInteractable.hpp"
#include "Entities/EntTypes/PlatformEnt.hpp"
#include "Entities/EntTypes/ForceFieldEnt.hpp"
#include "Entities/EntTypes/GooPlaneEnt.hpp"
#include "Entities/EntTypes/LadderEnt.hpp"
#include "../AudioPlayers.hpp"
#include "../Graphics/Materials/GravityCornerLightMaterial.hpp"
#include "../Settings.hpp"
#include "../Game.hpp"

#include <imgui.h>

static float* walkSpeed         = eg::TweakVarFloat("pl_walk_speed",     4.0f,  0.0f);
static float* swimSpeed         = eg::TweakVarFloat("pl_swim_speed",     5.0f,  0.0f);
static float* noclipSpeed       = eg::TweakVarFloat("pl_noclip_speed",   8.0f,  0.0f);
static float* noclipAccel       = eg::TweakVarFloat("pl_noclip_accel",   80.0f, 0.0f);
static float* walkAccelTime     = eg::TweakVarFloat("pl_walk_atime",     0.1f,  0.0f);
static float* swimAccelTime     = eg::TweakVarFloat("pl_swim_atime",     0.4f,  0.0f);
static float* walkDeaccelTime   = eg::TweakVarFloat("pl_walk_datime",    0.05f, 0.0f);
static float* swimAccel         = eg::TweakVarFloat("pl_swim_accel",     12.0f, 0.0f);
static float* swimDrag          = eg::TweakVarFloat("pl_swim_drag",      3.5f,  0.0f);
static float* swimGravityScale  = eg::TweakVarFloat("pl_swim_gravscale", 0.1f,  0.0f);
static float* ladderClimbSpeed  = eg::TweakVarFloat("pl_ladder_speed",   3.0f,  0.0f);
static float* ladderAccelTime   = eg::TweakVarFloat("pl_ladder_atime",   0.2f,  0.0f);
static float* jumpHeight        = eg::TweakVarFloat("pl_jump_height",    1.1f,  0.0f);
static float* waterJumpHeight   = eg::TweakVarFloat("pl_wjump_height",   1.3f,  0.0f);
static float* eyeRoundPrecision = eg::TweakVarFloat("pl_eye_round_prec", 0.01f, 0);
static float* stepsPerSecond    = eg::TweakVarFloat("pl_steps_per_sec",  2.2f, 0);
static int* noclipActive        = eg::TweakVarInt("noclip", 0, 0, 1);

static float* stepVolume          = eg::TweakVarFloat("vol_pl_step", 0.7f, 0);
static float* gravityCornerVolume = eg::TweakVarFloat("vol_gravity_corner", 1.5f, 0);

static const float* viewBobbingMaxRotation = eg::TweakVarFloat("vb_max_rot", 0.0035f, 0);
static const float* viewBobbingMaxTransY = eg::TweakVarFloat("vb_max_ty", 0.05f, 0);
static const float* viewBobbingCyclesPerSecond = eg::TweakVarFloat("vb_cycles_per_sec", 2.2f, 0);

static constexpr float EYE_OFFSET = Player::EYE_HEIGHT - Player::HEIGHT / 2;

static constexpr int NUM_WALK_SOUNDS = 5;
static const eg::AudioClip* WALK_SOUNDS_L[NUM_WALK_SOUNDS];
static const eg::AudioClip* WALK_SOUNDS_R[NUM_WALK_SOUNDS];
static const eg::AudioClip* gravityCornerSound;

static void OnInit()
{
	char nameBuffer[64];
	for (int i = 0; i < NUM_WALK_SOUNDS; i++)
	{
		snprintf(nameBuffer, sizeof(nameBuffer), "Audio/Walking/%dL.ogg", i + 1);
		WALK_SOUNDS_L[i] = &eg::GetAsset<eg::AudioClip>(nameBuffer);
		snprintf(nameBuffer, sizeof(nameBuffer), "Audio/Walking/%dR.ogg", i + 1);
		WALK_SOUNDS_R[i] = &eg::GetAsset<eg::AudioClip>(nameBuffer);
	}
	
	gravityCornerSound = &eg::GetAsset<eg::AudioClip>("Audio/GravityCorner.ogg");
}
EG_ON_INIT(OnInit)

Player::Player()
{
	m_physicsObject.velocity = { 0, 0, 0 };
	m_physicsObject.position = { 1, 2, 1 };
	m_physicsObject.canBePushed = false;
	m_physicsObject.friction = 0;
	m_physicsObject.owner = this;
}

bool Player::CanPickUpCube() const
{
	return *noclipActive || m_onGround;
}

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

inline float GetJumpAccel(float height)
{
	return std::sqrt(2.0f * height * GRAVITY_MAG);
}

void Player::Update(World& world, PhysicsEngine& physicsEngine, float dt, bool underwater)
{
	const glm::vec3 up = -DirectionVector(m_down);
	
	glm::vec2 rotationAnalogValue = eg::InputState::Current().RightAnalogValue();
	glm::vec2 movementAnalogValue = eg::InputState::Current().LeftAnalogValue();
	if (settings.flipJoysticks)
	{
		std::swap(rotationAnalogValue, movementAnalogValue);
	}
	
	auto TransitionInterpol = [&]
	{ return glm::smoothstep(0.0f, 1.0f, m_transitionTime); };
	
	//Updates the gravity switch transition.
	//If this is playing remaining update logic will be skipped!
	if (m_gravityTransitionMode == TransitionMode::Corner)
	{
		constexpr float TRANSITION_DURATION = 0.2f;
		m_transitionTime += dt / TRANSITION_DURATION;
		
		if (m_transitionTime > 1.0f)
		{
			m_physicsObject.position = m_newPosition;
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
	
	if (eg::DevMode() && eg::IsButtonDown(eg::Button::F6) && !eg::WasButtonDown(eg::Button::F6))
	{
		*noclipActive = 1 - *noclipActive;
	}
	
	if (m_gravityTransitionMode == TransitionMode::None && !eg::IsButtonDown(eg::Button::F8))
	{
		glm::vec2 rotationDelta = glm::vec2(eg::CursorPosDelta()) * -settings.lookSensitivityMS;
		
		rotationDelta -= rotationAnalogValue * (settings.lookSensitivityGP * dt);
		
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
	
	const bool moveForward = settings.keyMoveF.IsDown();
	const bool moveBack    = settings.keyMoveB.IsDown();
	const bool moveLeft    = settings.keyMoveL.IsDown();
	const bool moveRight   = settings.keyMoveR.IsDown();
	const bool moveUp      = settings.keyJump.IsDown();
	
	if (m_leftWaterTime > 0)
	{
		m_leftWaterTime -= dt;
	}
	else if (underwater || *noclipActive)
	{
		m_onGround = false;
		
		glm::vec3 accel;
		if (moveForward)
			accel += forward;
		if (moveBack)
			accel += -forward;
		if (moveLeft)
			accel += -right;
		if (moveRight)
			accel += right;
		if (moveUp)
			accel += up;
		
		if (glm::length2(accel) > 0.01f)
		{
			if (underwater)
			{
				accel += up * *swimGravityScale * GRAVITY_MAG / *swimAccel;
			}
			
			accel = glm::normalize(accel) * (*noclipActive ? *noclipAccel : *swimAccel);
			m_physicsObject.velocity += accel * dt;
		}
		
		if (*noclipActive)
		{
			const float speed = glm::length(m_physicsObject.velocity);
			if (speed > *noclipSpeed)
				m_physicsObject.velocity *= *noclipSpeed / speed;
		}
		
		m_physicsObject.velocity -= m_physicsObject.velocity * std::min(dt * *swimDrag, 1.0f);
	}
	
	//Constructs the forward and right movement vectors.
	//These, along with up, are the basis vectors for local space.
	const glm::vec3 forwardPlane = glm::normalize(forward - glm::dot(forward, up) * up);
	const glm::vec3 rightPlane = glm::normalize(right - glm::dot(right, up) * up);
	
	//Finds the velocity vector in local space
	float localVelVertical = glm::dot(up, m_physicsObject.velocity);
	glm::vec2 localVelPlane(glm::dot(forwardPlane, m_physicsObject.velocity), glm::dot(rightPlane, m_physicsObject.velocity));
	glm::vec2 localAccPlane(-movementAnalogValue.y, movementAnalogValue.x);
	
	if (m_planeMovementDisabledTimer > 0)
	{
		m_planeMovementDisabledTimer -= dt;
	}
	else if (m_gravityTransitionMode == TransitionMode::None && !underwater && !*noclipActive)
	{
		if (glm::length(localVelPlane) > *walkSpeed * 1.1f && !m_onGround)
		{
			localVelPlane *= 1.0f - std::min(dt * 0.3f, 0.1f);
		}
		else
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
	}
	
	glm::vec3 accXZNorm = localAccPlane.x * forwardPlane + localAccPlane.y * rightPlane;
	float accXZLen = glm::length(accXZNorm);
	if (accXZLen > 0.001f)
		accXZNorm /= accXZLen;
	else
		accXZNorm = glm::vec3(0);
	
	glm::vec3 velocityXZ = localVelPlane.x * forwardPlane + localVelPlane.y * rightPlane;
	
	m_radius = glm::vec3(WIDTH / 2, WIDTH / 2, WIDTH / 2);
	if (!underwater && !m_wasUnderwater)
		m_radius[(int)m_down / 2] = HEIGHT / 2;
	m_physicsObject.shape = eg::AABB(-m_radius, m_radius);
	PhysicsObject* floorObject = physicsEngine.FindFloorObject(m_physicsObject, -up);
	
	//Checks for intersections with ladders
	const LadderEnt* ladder = nullptr;
	if (m_gravityTransitionMode == TransitionMode::None && !m_isCarrying && !underwater)
	{
		eg::AABB selfAABB = GetAABB();
		if (m_wasClimbingLadder)
		{
			glm::vec3 aabbExtension = up * -0.2f;
			selfAABB.min = glm::min(selfAABB.min, selfAABB.min + aabbExtension);
			selfAABB.max = glm::max(selfAABB.max, selfAABB.max + aabbExtension);
		}
		
		world.entManager.ForEachOfType<LadderEnt>([&] (const LadderEnt& ladderCandidate)
		{
			if (std::abs(glm::dot(up, ladderCandidate.GetDownVector())) > 0.5f &&
				glm::dot(accXZNorm, -glm::vec3(DirectionVector(ladderCandidate.GetFacingDirection()))) > 0.5f &&
				ladderCandidate.GetAABB().Intersects(selfAABB))
			{
				ladder = &ladderCandidate;
				velocityXZ = glm::vec3(0);
			}
		});
	}
	
	//Vertical movement
	if (!*noclipActive)
	{
		const bool standingOnCube = floorObject &&
			std::holds_alternative<Ent*>(floorObject->owner) &&
		    std::get<Ent*>(floorObject->owner)->TypeID() == EntTypeID::Cube;
		
		if (m_wasUnderwater && !underwater && (localVelVertical > 0 || moveUp))
		{
			glm::vec3 xzShift = forwardPlane * 0.1f;
			eg::AABB aabbNeedsIntersect = GetAABB();
			aabbNeedsIntersect.min += xzShift;
			aabbNeedsIntersect.max += xzShift;
			
			eg::AABB aabbNoIntersect = aabbNeedsIntersect;
			aabbNoIntersect.min += up * *waterJumpHeight;
			aabbNoIntersect.max += up * *waterJumpHeight;
			
			if (physicsEngine.CheckCollision(aabbNeedsIntersect, RAY_MASK_CLIMB, &m_physicsObject) &&
			    !physicsEngine.CheckCollision(aabbNoIntersect, RAY_MASK_CLIMB, &m_physicsObject))
			{
				localVelVertical = GetJumpAccel(*waterJumpHeight);
				m_leftWaterTime = 0.5f;
			}
		}
		
		if (ladder != nullptr)
		{
			m_onGround = false;
			m_onGroundLinger = 0;
			
			if (localVelVertical < 0)
				localVelVertical = 0;
			localVelVertical = std::min(localVelVertical + *ladderClimbSpeed / *ladderAccelTime * dt, *ladderClimbSpeed);
		}
		else
		{
			//Applies gravity
			localVelVertical -= ((underwater ? *swimGravityScale : 1.0f) * GRAVITY_MAG) * dt;
			
			//Updates vertical velocity
			if (moveUp && m_gravityTransitionMode == TransitionMode::None && m_onGround)
			{
				localVelVertical = GetJumpAccel(*jumpHeight * (standingOnCube ? 0.7f : 1.0f));
				m_onGroundRingBufferSize = std::min(m_onGroundRingBufferSize, 1U);
				m_onGround = false;
				m_onGroundLinger = 0;
				m_physicsObject.position += up * 0.01f;
			}
		}
	}
	
	m_wasClimbingLadder = ladder != nullptr;
	
	//Reconstructs the world velocity vector
	glm::vec3 velocityY = localVelVertical * up;
	m_physicsObject.velocity = velocityXZ + velocityY;
	glm::vec3 moveXZ = velocityXZ * dt;
	glm::vec3 moveY = velocityY * dt;
	
	//Checks for intersections with gravity corners along the move (XZ) vector
	if (m_gravityTransitionMode == TransitionMode::None && m_onGround && !m_isCarrying)
	{
		if (const GravityCorner* corner = world.FindGravityCorner(GetAABB(), moveXZ, m_down))
		{
			//A gravity corner was found, this code begins transitioning to another gravity
			
			const Dir newDown = corner->down1 == m_down ? corner->down2 : corner->down1;
			const glm::vec3 newUpVec = -DirectionVector(newDown);
			const glm::quat rotationDiff = glm::angleAxis(eg::HALF_PI, glm::cross(up, newUpVec));
			
			//Finds the new position of the player after the transition. This is done by transforming 
			// the player's position to corner local space, swapping x and y, and transforming back.
			const glm::mat3 cornerRotation = corner->MakeRotationMatrix();
			glm::vec3 cornerL = glm::transpose(cornerRotation) * (Position() - corner->position);
			if (corner->down2 == m_down)
				cornerL.x = std::max(cornerL.x, 1.5f);
			else
				cornerL.y = std::max(cornerL.y, 1.5f);
			std::swap(cornerL.x, cornerL.y);
			m_newPosition = (cornerRotation * cornerL) + corner->position + newUpVec * 0.001f;
			
			//Converts the camera's yaw and pitch to the new gravity
			glm::vec3 f =
				glm::inverse(GetDownCorrection(newDown)) * (rotationDiff * (m_rotation * glm::vec3(0, 0, -1)));
			glm::vec2 f2 = glm::normalize(glm::vec2(f.x, f.z));
			m_rotationPitch = std::asin(f.y);
			m_rotationYaw = std::atan2(-f2.x, -f2.y);
			
			//Sets up the old and new rotations to be interpolated between in the transition
			m_oldRotation = m_rotation;
			m_newRotation = GetRotation(m_rotationYaw, m_rotationPitch, newDown);
			
			//Rotates the velocity vector and reassigns some other stuff
			m_physicsObject.velocity = rotationDiff * m_physicsObject.velocity;
			m_down = newDown;
			moveXZ = moveY = { };
			m_oldEyePosition = m_eyePosition;
			m_gravityTransitionMode = TransitionMode::Corner;
			m_transitionTime = 0;
			
			glm::vec3 activatePos = cornerRotation * glm::vec3(0.3f, 0.3f, cornerL.z) + corner->position;
			GravityCornerLightMaterial::instance.Activate(activatePos);
			
			eg::AudioLocationParameters locationParams = {};
			locationParams.position = corner->position;
			AudioPlayers::gameSFXPlayer.Play(*gravityCornerSound, *gravityCornerVolume, 1, &locationParams);
		}
	}
	
	//Checks for interactable entities
	interactControlHint = {};
	if (m_gravityTransitionMode == TransitionMode::None)
	{
		EntInteractable* interactableEntity = nullptr;
		int bestInteractPriority = 0;
		world.entManager.ForEachWithFlag(EntTypeFlags::Interactable, [&](Ent& entity)
		{
			EntInteractable& interactable = dynamic_cast<EntInteractable&>(entity);
			int thisPriority = interactable.CheckInteraction(*this, physicsEngine);
			if (thisPriority > bestInteractPriority)
			{
				interactableEntity = &interactable;
				bestInteractPriority = thisPriority;
			}
		});
		
		if (interactableEntity != nullptr)
		{
			if (settings.keyInteract.IsDown() && !settings.keyInteract.WasDown())
			{
				interactableEntity->Interact(*this);
			}
			else
			{
				interactControlHint = interactableEntity->GetInteractControlHint();
			}
		}
	}
	
	//Checks for force fields
	eg::AABB forceFieldAABB = GetAABB();
	forceFieldAABB.min += forceFieldAABB.Size() * 0.2f;
	forceFieldAABB.max -= forceFieldAABB.Size() * 0.2f;
	std::optional<Dir> forceFieldGravity = ForceFieldEnt::CheckIntersection(world.entManager, forceFieldAABB);
	if (m_gravityTransitionMode == TransitionMode::None && forceFieldGravity.has_value() &&
		m_down != *forceFieldGravity && !*noclipActive)
	{
		if (*forceFieldGravity == OppositeDir(m_down))
		{
			FlipDown();
		}
		else
		{
			m_down = *forceFieldGravity;
			m_gravityTransitionMode = TransitionMode::Fall;
			m_transitionTime = 0;
			m_oldPosition = Position();
			m_oldEyePosition = m_eyePosition;
			m_oldRotation = m_rotation;
			m_newRotation = GetRotation(m_rotationYaw, m_rotationPitch, m_down);
		}
	}
	
	m_physicsObject.actualMove = {};
	
	//Moves the player with the current platform, if there is one below
	glm::vec3 launchVelocity(0.0f);
	if (floorObject != nullptr && std::holds_alternative<Ent*>(floorObject->owner) && !*noclipActive)
	{
		Ent& floorEntity = *std::get<Ent*>(floorObject->owner);
		physicsEngine.ApplyMovement(*floorObject, dt);
		m_physicsObject.move = floorObject->actualMove;
		physicsEngine.ApplyMovement(m_physicsObject, dt);
		
		if (floorEntity.TypeID() == EntTypeID::Platform)
		{
			launchVelocity = ((PlatformEnt&)floorEntity).LaunchVelocity();
		}
	}
	
	if (*noclipActive)
	{
		m_physicsObject.position += m_physicsObject.velocity  * dt;
	}
	else
	{
		//Applies movement in the XZ plane
		m_physicsObject.shape = eg::AABB(-m_radius + up * 0.05f, m_radius + up * 0.05f);
		m_physicsObject.move = moveXZ;
		physicsEngine.ApplyMovement(m_physicsObject, dt);
		
		//Applies movement vertically
		m_onGround = false;
		m_physicsObject.shape = eg::AABB(-m_radius, m_radius);
		m_physicsObject.move = moveY;
		m_physicsObject.slideDim = glm::abs(up);
		physicsEngine.ApplyMovement(m_physicsObject, dt, [&] (const PhysicsObject&, const glm::vec3& correction)
		{
			if (glm::dot(correction, up) > 0)
				m_onGround = true;
		});
		m_physicsObject.slideDim = glm::vec3(1);
		
		if (glm::length2(launchVelocity) > 0.001f)
		{
			m_physicsObject.velocity = launchVelocity;
			m_onGround = false;
			m_onGroundLinger = 0;
			m_planeMovementDisabledTimer = 0.5f;
		}
	}
	
	if (m_onGround)
	{
		m_onGroundLinger = 0.1f;
	}
	else
	{
		m_onGroundLinger -= dt;
		if (m_onGroundLinger > 0)
			m_onGround = true;
		else
			m_onGroundLinger = 0;
	}
	
	if (!*noclipActive)
	{
		bool isInGoo = false;
		eg::Sphere gooPlaneTestSphere(Position(), 0.1f);
		
		world.entManager.ForEachOfType<GooPlaneEnt>([&] (const GooPlaneEnt& gooPlane)
		{
			if (gooPlane.IsUnderwater(gooPlaneTestSphere))
				isInGoo = true;
		});
		
		if (isInGoo && m_onGroundRingBufferSize != 0)
		{
			Reset();
			uint32_t ringBufferPos = (m_onGroundRingBufferFront + m_onGroundRingBuffer.size() - m_onGroundRingBufferSize) % m_onGroundRingBuffer.size();
			auto [prevPos, prevRot, prevDown] = m_onGroundRingBuffer[ringBufferPos];
			m_physicsObject.position = prevPos;
			m_rotation = prevRot;
			m_down = prevDown;
			return;
		}
	}
	
	if (m_onGround)
	{
		if (!m_wasOnGround)
		{
			m_onGroundRingBufferSize = 0;
			m_onGroundRingBufferFront = 0;
			m_onGroundPushDelay = 0;
		}
		
		m_onGroundPushDelay -= dt;
		while (m_onGroundPushDelay <= 0)
		{
			m_onGroundRingBuffer[m_onGroundRingBufferFront] = { Position(), m_rotation, m_down };
			
			m_onGroundRingBufferFront++;
			m_onGroundRingBufferFront %= m_onGroundRingBuffer.size();
			if (m_onGroundRingBufferSize < m_onGroundRingBuffer.size())
				m_onGroundRingBufferSize++;
			
			m_onGroundPushDelay += 0.2f / m_onGroundRingBuffer.size();
		}
	}
	
	if (underwater)
		m_eyeOffsetFade = std::max(m_eyeOffsetFade - dt, 0.0f);
	else
		m_eyeOffsetFade = std::min(m_eyeOffsetFade + dt, 1.0f);
	
	//Plays footstep sounds
	const float distancePerStepSound = *walkSpeed / *stepsPerSecond;
	if (m_onGround && !underwater)
	{
		glm::vec3 planeMove = m_physicsObject.actualMove - up * glm::dot(m_physicsObject.actualMove, up);
		float distanceMoved = glm::length(planeMove);
		if (distanceMoved < 1E-3f)
		{
			m_stepSoundRemDistance = distancePerStepSound * 0.25f;
			m_nextStepSoundRightIndex = -1;
		}
		else
		{
			m_stepSoundRemDistance -= distanceMoved;
			if (m_stepSoundRemDistance < 0)
			{
				const eg::AudioClip* clipToPlay;
				if (m_nextStepSoundRightIndex != -1)
				{
					clipToPlay = WALK_SOUNDS_R[m_nextStepSoundRightIndex];
					m_nextStepSoundRightIndex = -1;
				}
				else
				{
					m_nextStepSoundRightIndex = std::uniform_int_distribution<int>(0, NUM_WALK_SOUNDS - 1)(globalRNG);
					clipToPlay = WALK_SOUNDS_L[m_nextStepSoundRightIndex];
				}
				
				std::uniform_real_distribution<float> pitchDist(0.7f, 1.0f);
				std::uniform_real_distribution<float> volumeDist(*stepVolume * 0.8f, *stepVolume * 1.2f);
				
				eg::AudioLocationParameters locationParameters = {};
				locationParameters.position = FeetPosition();
				locationParameters.direction = up;
				AudioPlayers::gameSFXPlayer.Play(*clipToPlay, volumeDist(globalRNG), pitchDist(globalRNG),
				                                 &locationParameters);
				
				m_stepSoundRemDistance += distancePerStepSound;
				if (m_stepSoundRemDistance < 0)
					m_stepSoundRemDistance = distancePerStepSound;
			}
		}
	}
	else
	{
		m_stepSoundRemDistance = distancePerStepSound * 0.25f;
		m_nextStepSoundRightIndex = -1;
	}
	
	//Updates the eye position
	m_eyePosition = Position() + up * EYE_OFFSET * m_eyeOffsetFade;
	int downDim = (int)m_down / 2;
	m_eyePosition[downDim] = std::round(m_eyePosition[downDim] / *eyeRoundPrecision) * *eyeRoundPrecision;
	if (m_gravityTransitionMode == TransitionMode::Fall)
	{
		m_eyePosition = glm::mix(m_oldEyePosition + Position() - m_oldPosition, m_eyePosition, TransitionInterpol());
	}
	
	m_wasUnderwater = underwater;
	m_wasOnGround = m_onGround;
	
	float targetVBIntensity = (m_onGround && !underwater) ? std::min(glm::length(velocityXZ) / *walkSpeed, 1.0f) : 0.0f;
	m_viewBobbingIntensity = eg::AnimateTo(m_viewBobbingIntensity, targetVBIntensity, dt * 10.0f);
	if (m_viewBobbingIntensity < 0.0001f)
		m_viewBobbingTime = 0;
	else
	{
		m_viewBobbingTime += dt * eg::PI * *viewBobbingCyclesPerSecond * m_viewBobbingIntensity;
	}
}

void Player::FlipDown()
{
	Dir newDown = OppositeDir(m_down);
	
	m_rotationYaw = -m_rotationYaw + eg::PI;
	m_rotationPitch = -m_rotationPitch;
	m_oldRotation = m_rotation;
	m_newRotation = GetRotation(m_rotationYaw, m_rotationPitch, newDown);
	
	m_down = newDown;
	m_oldPosition = Position();
	m_oldEyePosition = m_eyePosition;
	m_gravityTransitionMode = TransitionMode::Fall;
	m_transitionTime = 0;
	m_physicsObject.velocity = glm::vec3(0);
}

static const float VIEW_BOBBING_TY_LEVEL_MULTIPLIERS[] = {
	/* Off    */ 0.0f,
	/* Low    */ 0.5f,
	/* Normal */ 1.0f
};

static const float VIEW_BOBBING_RZ_LEVEL_MULTIPLIERS[] = {
	/* Off    */ 0.0f,
	/* Low    */ 0.0f,
	/* Normal */ 1.0f
};

void Player::GetViewMatrix(glm::mat4& matrixOut, glm::mat4& inverseMatrixOut) const
{
	const float viewBobbingRZ = std::sin(m_viewBobbingTime) * *viewBobbingMaxRotation *
		m_viewBobbingIntensity * VIEW_BOBBING_RZ_LEVEL_MULTIPLIERS[(int)settings.viewBobbingLevel];
	const float viewBobbingTY = std::sin(m_viewBobbingTime * 2) * *viewBobbingMaxTransY *
		m_viewBobbingIntensity * VIEW_BOBBING_TY_LEVEL_MULTIPLIERS[(int)settings.viewBobbingLevel];
	
	const glm::mat4 rotationMatrix = glm::mat4_cast(m_rotation);
	
	matrixOut =
		glm::rotate(glm::mat4(1), viewBobbingRZ, glm::vec3(0, 0, 1)) *
		glm::translate(glm::mat4(1), glm::vec3(0, -viewBobbingTY, 0)) *
		glm::transpose(rotationMatrix) *
		glm::translate(glm::mat4(1.0f), -m_eyePosition);
	inverseMatrixOut =
		glm::translate(glm::mat4(1.0f), m_eyePosition) *
		rotationMatrix *
		glm::translate(glm::mat4(1), glm::vec3(0, viewBobbingTY, 0)) *
		glm::rotate(glm::mat4(1), -viewBobbingRZ, glm::vec3(0, 0, 1));
}

glm::vec3 Player::Forward() const
{
	return m_rotation * glm::vec3(0, 0, -1);
}

void Player::DrawDebugOverlay()
{
	ImGui::Text("Pos: %.2f, %.2f, %.2f", Position().x, Position().y, Position().z);
	ImGui::Text("Vel: %.2f, %.2f, %.2f, Speed: %.2f)",
			 m_physicsObject.velocity.x, m_physicsObject.velocity.y, m_physicsObject.velocity.z,
			 glm::length(m_physicsObject.velocity));
	ImGui::Text("Eye Pos: %.2f, %.2f, %.2f", m_eyePosition.x, m_eyePosition.y, m_eyePosition.z);
	
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
	
	ImGui::Text("On ground: %d", (int)m_onGround);
	ImGui::Text("On ladder: %d", (int)m_wasClimbingLadder);
}

void Player::Reset()
{
	m_physicsObject.velocity = glm::vec3(0.0f);
	m_onGround = false;
	m_wasOnGround = false;
	m_wasUnderwater = false;
	m_down = Dir::NegY;
	m_isCarrying = false;
	m_leftWaterTime = 0;
	m_eyeOffsetFade = 1;
	m_viewBobbingIntensity = 1;
	m_gravityTransitionMode = TransitionMode::None;
	m_nextStepSoundRightIndex = -1;
	m_stepSoundRemDistance = 0;
}

glm::vec3 Player::FeetPosition() const
{
	return m_physicsObject.position + glm::vec3(DirectionVector(m_down)) * (HEIGHT * 0.45f);
}

eg::AABB Player::GetAABB() const
{
	return eg::AABB(m_physicsObject.position - m_radius, m_physicsObject.position + m_radius);
}
