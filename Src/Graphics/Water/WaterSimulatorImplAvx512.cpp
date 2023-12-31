#ifdef IOMOMI_ENABLE_WATER
#include "WaterSimulatorImpl.hpp"

#include <immintrin.h>

static_assert(MAX_CLOSE % alignof(__m512) == 0);

class WaterSimulatorImplAvx512 : public WaterSimulatorImpl
{
public:
	explicit WaterSimulatorImplAvx512(const ConstructorArgs& args) : WaterSimulatorImpl(args, alignof(__m512))
	{
		std::uniform_real_distribution<float> offsetDist(-CORE_RADIUS / 2, CORE_RADIUS);
		for (size_t i = 0; i < std::size(m_randomOffsets); i++)
		{
			for (size_t j = 0; j < 8; j++)
			{
				m_randomOffsets[i][j] = offsetDist(m_threadRngs[0]);
			}
		}
	}

	void Stage2_ComputeNumberDensity(SimulateStageArgs args) final override;
	void Stage3_Acceleration(SimulateStageArgs args) final override;

private:
	__m512 m_randomOffsets[16];
};

std::unique_ptr<WaterSimulatorImpl> CreateWaterSimulatorImplAvx512(const WaterSimulatorImpl::ConstructorArgs& args)
{
	return std::make_unique<WaterSimulatorImplAvx512>(args);
}

constexpr uint32_t PROC_PER_SIMD_ITERATION = 16;

inline float SumFloatx16(__m512 v)
{
	float ans = 0;
	for (uint32_t i = 0; i < 16; i++)
		ans += v[i];
	return ans;
}

void WaterSimulatorImplAvx512::Stage2_ComputeNumberDensity(SimulateStageArgs args)
{
	for (uint32_t a = args.loIdx; a < args.hiIdx; a++)
	{
		__m512 densityX = _mm512_set1_ps(0);
		__m512 densityY = _mm512_set1_ps(0);

		uint32_t numClose = GetCloseParticlesCount(a);
		if (numClose == 0)
			continue;

		__m512* closeParticlesDist = reinterpret_cast<__m512*>(GetCloseParticlesDistPtr(a));

		uint32_t numSimdIterations = (numClose + PROC_PER_SIMD_ITERATION - 1) / PROC_PER_SIMD_ITERATION;

		// Fills additional distances in the last simd iteration with INFLUENCE_RADIUS (no effect)
		for (uint32_t i = numClose; i < numSimdIterations * PROC_PER_SIMD_ITERATION; i++)
		{
			GetCloseParticlesDistPtr(a)[i] = INFLUENCE_RADIUS;
			GetCloseParticlesIdxPtr(a)[i] = GetCloseParticlesIdxPtr(a)[numClose - 1];
		}

		for (uint32_t bi = 0; bi < numSimdIterations; bi++)
		{
			__m512 dv = _mm512_mul_ps(closeParticlesDist[bi], _mm512_set1_ps(1.0f / INFLUENCE_RADIUS));
			__m512 q = _mm512_max_ps(_mm512_sub_ps(_mm512_set1_ps(1), dv), _mm512_set1_ps(0));
			__m512 q2 = _mm512_mul_ps(q, q);
			__m512 q3 = _mm512_mul_ps(q2, q);
			__m512 q4 = _mm512_mul_ps(q2, q2);

			densityX = _mm512_add_ps(densityX, q3);
			densityY = _mm512_add_ps(densityY, q4);
		}

		m_particleDensityX[a] = 1.0f + SumFloatx16(densityX);
		m_particleDensityY[a] = 1.0f + SumFloatx16(densityY);
	}
}

void WaterSimulatorImplAvx512::Stage3_Acceleration(SimulateStageArgs args)
{
	uint32_t randomOffsetIdx = m_threadRngs[args.threadIndex]();

	for (uint32_t a = args.loIdx; a < args.hiIdx; a++)
	{
		const float aDensityX = m_particleDensityX[a];
		const float nearDensA = m_particleDensityY[a];
		const float relativeDensity = (aDensityX - AMBIENT_DENSITY) / aDensityX;

		__m512 accelX = _mm512_setzero_ps();
		__m512 accelY = _mm512_setzero_ps();
		__m512 accelZ = _mm512_setzero_ps();

		const float aDensityXSub2TargetDensity = aDensityX - 2 * TARGET_NUMBER_DENSITY;

		glm::vec3 apos = m_particlesPos1[a];

		uint32_t numClose = GetCloseParticlesCount(a);
		__m256i* closeParticlesIdx = reinterpret_cast<__m256i*>(GetCloseParticlesIdxPtr(a));
		__m512* closeParticlesDist = reinterpret_cast<__m512*>(GetCloseParticlesDistPtr(a));

		uint32_t numSimdIterations = (numClose + PROC_PER_SIMD_ITERATION - 1) / PROC_PER_SIMD_ITERATION;

		for (uint32_t bi = 0; bi < numSimdIterations; bi++)
		{
			__m512i indices = _mm512_cvtepu16_epi32(closeParticlesIdx[bi]);

			__m512 bposX = _mm512_i32gather_ps(indices, m_particlesPos1.x, sizeof(float));
			__m512 bposY = _mm512_i32gather_ps(indices, m_particlesPos1.y, sizeof(float));
			__m512 bposZ = _mm512_i32gather_ps(indices, m_particlesPos1.z, sizeof(float));

			__m512 sepX = _mm512_sub_ps(_mm512_set1_ps(apos.x), bposX);
			__m512 sepY = _mm512_sub_ps(_mm512_set1_ps(apos.y), bposY);
			__m512 sepZ = _mm512_sub_ps(_mm512_set1_ps(apos.z), bposZ);

			__m512 dist = closeParticlesDist[bi];

			// Randomly separates particles that are very close so that the pressure gradient won't be zero.
			// dist < CORE_RADIUS || abs(sepX) < CORE_RADIUS || abs(sepY) < CORE_RADIUS || abs(sepZ) < CORE_RADIUS)
			auto tooCloseMask = _mm512_cmp_ps_mask(dist, _mm512_set1_ps(CORE_RADIUS), _CMP_LT_OQ) |
			                    _mm512_cmp_ps_mask(_mm512_abs_ps(sepX), _mm512_set1_ps(CORE_RADIUS), _CMP_LT_OQ) |
			                    _mm512_cmp_ps_mask(_mm512_abs_ps(sepY), _mm512_set1_ps(CORE_RADIUS), _CMP_LT_OQ) |
			                    _mm512_cmp_ps_mask(_mm512_abs_ps(sepZ), _mm512_set1_ps(CORE_RADIUS), _CMP_LT_OQ);
			if (tooCloseMask)
			{
				__m512 offsetX = m_randomOffsets[(randomOffsetIdx++) % std::size(m_randomOffsets)];
				sepX = _mm512_add_ps(sepX, _mm512_mask_blend_ps(tooCloseMask, _mm512_setzero_ps(), offsetX));

				__m512 offsetY = m_randomOffsets[(randomOffsetIdx++) % std::size(m_randomOffsets)];
				sepY = _mm512_add_ps(sepY, _mm512_mask_blend_ps(tooCloseMask, _mm512_setzero_ps(), offsetY));

				__m512 offsetZ = m_randomOffsets[(randomOffsetIdx++) % std::size(m_randomOffsets)];
				sepZ = _mm512_add_ps(sepZ, _mm512_mask_blend_ps(tooCloseMask, _mm512_setzero_ps(), offsetZ));

				dist = _mm512_sqrt_ps(_mm512_add_ps(
					_mm512_add_ps(_mm512_mul_ps(sepX, sepX), _mm512_mul_ps(sepY, sepY)), _mm512_mul_ps(sepZ, sepZ)));
			}

			__m512 dv = _mm512_mul_ps(dist, _mm512_set1_ps(1.0f / INFLUENCE_RADIUS));
			__m512 q = _mm512_max_ps(_mm512_sub_ps(_mm512_set1_ps(1), dv), _mm512_set1_ps(0));
			__m512 q2 = _mm512_mul_ps(q, q);
			__m512 q3 = _mm512_mul_ps(q2, q);

			__m512 densB = _mm512_i32gather_ps(indices, m_particleDensityX, sizeof(float));
			__m512 nearDensB = _mm512_i32gather_ps(indices, m_particleDensityY, sizeof(float));
			__m512 pressure = _mm512_mul_ps(
				_mm512_set1_ps(STIFFNESS), _mm512_add_ps(densB, _mm512_set1_ps(aDensityXSub2TargetDensity)));
			__m512 pressureNear = _mm512_mul_ps(
				_mm512_set1_ps(STIFFNESS * NEAR_TO_FAR), _mm512_add_ps(nearDensB, _mm512_set1_ps(nearDensA)));

			__m512 accelScale = _mm512_div_ps(
				_mm512_add_ps(_mm512_mul_ps(pressure, q2), _mm512_mul_ps(pressureNear, q3)),
				_mm512_add_ps(dist, _mm512_set1_ps(FLT_EPSILON)));

			accelX = _mm512_add_ps(accelX, _mm512_mul_ps(sepX, accelScale));
			accelY = _mm512_add_ps(accelY, _mm512_mul_ps(sepY, accelScale));
			accelZ = _mm512_add_ps(accelZ, _mm512_mul_ps(sepZ, accelScale));
		}

		glm::vec3 accel = relativeDensity * gravities[m_particlesGravity[a]];
		accel.x += SumFloatx16(accelX);
		accel.y += SumFloatx16(accelY);
		accel.z += SumFloatx16(accelZ);

		// Applies acceleration to the particle's velocity
		glm::vec3 velAdd = accel * args.dt;
		m_particlesVel1.x[a] += velAdd.x;
		m_particlesVel1.y[a] += velAdd.y;
		m_particlesVel1.z[a] += velAdd.z;
	}
}

#endif
