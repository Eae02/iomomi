#ifndef WATER_CONSTANTS_H
#define WATER_CONSTANTS_H

#ifdef __cplusplus
#define CONST constexpr
#define UINT uint32_t
#else
#define CONST const
#define UINT uint
#endif

CONST UINT W_CELLS_PER_CHUNK = 16;
CONST UINT W_NUM_CELL_CHUNKS = 64;

CONST UINT W_MAX_OCT_GROUPS_PER_CELL = 4;

CONST float W_ELASTICITY = 0.1f;
CONST float W_MIN_PARTICLE_RADIUS = 0.15f;
CONST float W_MAX_PARTICLE_RADIUS = 0.3f;
CONST float W_INFLUENCE_RADIUS = 0.4f;
CONST float W_DISTANCE_EPSILON = 0.001f;
CONST float W_RADIAL_VISCOSITY_GAIN = 0.4f;
CONST float W_TARGET_DENSITY = 4.0f;
CONST float W_STIFFNESS = 7.5f;
CONST float W_NEAR_TO_FAR = 1.5f;
CONST float W_AMBIENT_DENSITY = 0.5f;

CONST float W_PARTICLE_RENDER_RADIUS = 0.12f;
CONST float W_PARTICLE_COLLIDE_RADIUS = 0.1f;

CONST float W_UNDERWATER_DEPTH = 0.5f;

CONST float W_GRAVITY = 10;

CONST UINT W_COMMON_WG_SIZE = 128;
CONST UINT W_OCT_GROUPS_PREFIX_SUM_WG_SIZE = 256;

CONST UINT W_MAX_GLOW_TIME = (1 << 13) - 1;

#undef CONST
#undef UINT

#endif