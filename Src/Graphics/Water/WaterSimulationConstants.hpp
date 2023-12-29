#pragma once

static constexpr float ELASTICITY = 0.2f;
static constexpr float IMPACT_COEFFICIENT = 1.0f + ELASTICITY;
static constexpr float MIN_PARTICLE_RADIUS = 0.1f;
static constexpr float MAX_PARTICLE_RADIUS = 0.3f;
static constexpr float INFLUENCE_RADIUS = 0.6f;
static constexpr float CORE_RADIUS = 0.001f;
static constexpr float RADIAL_VISCOSITY_GAIN = 0.25f;
static constexpr float TARGET_NUMBER_DENSITY = 3.5f;
static constexpr float STIFFNESS = 8.0f;
static constexpr float NEAR_TO_FAR = 2.0f;
static constexpr float AMBIENT_DENSITY = 0.5f;

static constexpr float WATER_GRAVITY = 10;

// Maximum number of particles per partition grid cell
static constexpr uint32_t MAX_PER_PART_CELL = 512;

// Maximum number of close particles
static constexpr uint32_t MAX_CLOSE = 512;

static constexpr int CELL_GROUP_SIZE = 4;
