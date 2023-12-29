#include "WaterSimulatorImpl.hpp"

#include <immintrin.h>

static_assert(MAX_CLOSE % alignof(__m256) == 0);

class WaterSimulatorImplAvx2 : public WaterSimulatorImpl
{
public:
	explicit WaterSimulatorImplAvx2(const ConstructorArgs& args) : WaterSimulatorImpl(args, alignof(__m256))
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
	__m256 m_randomOffsets[16];
};

std::unique_ptr<WaterSimulatorImpl> CreateWaterSimulatorImplAvx2(const WaterSimulatorImpl::ConstructorArgs& args)
{
	return std::make_unique<WaterSimulatorImplAvx2>(args);
}

constexpr uint32_t PROC_PER_SIMD_ITERATION = 8;

inline float SumFloatx8(__m256 v)
{
	float ans = 0;
	for (uint32_t i = 0; i < 8; i++)
		ans += v[i];
	return ans;
}

inline __m256 FloatX8Abs(__m256 v)
{
	return _mm256_andnot_ps(_mm256_set1_ps(-0.0f), v);
}

void WaterSimulatorImplAvx2::Stage2_ComputeNumberDensity(SimulateStageArgs args)
{
	for (uint32_t a = args.loIdx; a < args.hiIdx; a++)
	{
		__m256 densityX = _mm256_set1_ps(0);
		__m256 densityY = _mm256_set1_ps(0);

		uint32_t numClose = GetCloseParticlesCount(a);
		if (numClose == 0)
			continue;

		__m256* closeParticlesDist = reinterpret_cast<__m256*>(GetCloseParticlesDistPtr(a));

		uint32_t numSimdIterations = (numClose + PROC_PER_SIMD_ITERATION - 1) / PROC_PER_SIMD_ITERATION;

		// Fills additional distances in the last simd iteration with INFLUENCE_RADIUS (no effect)
		for (uint32_t i = numClose; i < numSimdIterations * PROC_PER_SIMD_ITERATION; i++)
		{
			GetCloseParticlesDistPtr(a)[i] = INFLUENCE_RADIUS;
			GetCloseParticlesIdxPtr(a)[i] = GetCloseParticlesIdxPtr(a)[numClose - 1];
		}

		for (uint32_t bi = 0; bi < numSimdIterations; bi++)
		{
			__m256 dv = _mm256_mul_ps(closeParticlesDist[bi], _mm256_set1_ps(1.0f / INFLUENCE_RADIUS));
			__m256 q = _mm256_max_ps(_mm256_sub_ps(_mm256_set1_ps(1), dv), _mm256_set1_ps(0));
			__m256 q2 = _mm256_mul_ps(q, q);
			__m256 q3 = _mm256_mul_ps(q2, q);
			__m256 q4 = _mm256_mul_ps(q2, q2);

			densityX = _mm256_add_ps(densityX, q3);
			densityY = _mm256_add_ps(densityY, q4);
		}

		m_particleDensityX[a] = 1.0f + SumFloatx8(densityX);
		m_particleDensityY[a] = 1.0f + SumFloatx8(densityY);
	}
}

void WaterSimulatorImplAvx2::Stage3_Acceleration(SimulateStageArgs args)
{
	uint32_t randomOffsetIdx = m_threadRngs[args.threadIndex]();

	for (uint32_t a = args.loIdx; a < args.hiIdx; a++)
	{
		const float aDensityX = m_particleDensityX[a];
		const float nearDensA = m_particleDensityY[a];
		const float relativeDensity = (aDensityX - AMBIENT_DENSITY) / aDensityX;

		__m256 accelX = _mm256_setzero_ps();
		__m256 accelY = _mm256_setzero_ps();
		__m256 accelZ = _mm256_setzero_ps();

		const float aDensityXSub2TargetDensity = aDensityX - 2 * TARGET_NUMBER_DENSITY;

		glm::vec3 apos = m_particlesPos1[a];

		uint32_t numClose = GetCloseParticlesCount(a);
		__m128i* closeParticlesIdx = reinterpret_cast<__m128i*>(GetCloseParticlesIdxPtr(a));
		__m256* closeParticlesDist = reinterpret_cast<__m256*>(GetCloseParticlesDistPtr(a));

		uint32_t numSimdIterations = (numClose + PROC_PER_SIMD_ITERATION - 1) / PROC_PER_SIMD_ITERATION;

		for (uint32_t bi = 0; bi < numSimdIterations; bi++)
		{
			__m256i indices = _mm256_cvtepu16_epi32(closeParticlesIdx[bi]);

			__m256 bposX = _mm256_i32gather_ps(m_particlesPos1.x, indices, sizeof(float));
			__m256 bposY = _mm256_i32gather_ps(m_particlesPos1.y, indices, sizeof(float));
			__m256 bposZ = _mm256_i32gather_ps(m_particlesPos1.z, indices, sizeof(float));

			__m256 sepX = _mm256_sub_ps(_mm256_set1_ps(apos.x), bposX);
			__m256 sepY = _mm256_sub_ps(_mm256_set1_ps(apos.y), bposY);
			__m256 sepZ = _mm256_sub_ps(_mm256_set1_ps(apos.z), bposZ);

			__m256 dist = closeParticlesDist[bi];

			// Randomly separates particles that are very close so that the pressure gradient won't be zero.
			// dist < CORE_RADIUS || abs(sepX) < CORE_RADIUS || abs(sepY) < CORE_RADIUS || abs(sepZ) < CORE_RADIUS)
			auto tooCloseMask = _mm256_or_ps(
				_mm256_or_ps(
					_mm256_cmp_ps(dist, _mm256_set1_ps(CORE_RADIUS), _CMP_LT_OQ),
					_mm256_cmp_ps(FloatX8Abs(sepX), _mm256_set1_ps(CORE_RADIUS), _CMP_LT_OQ)),
				_mm256_or_ps(
					_mm256_cmp_ps(FloatX8Abs(sepY), _mm256_set1_ps(CORE_RADIUS), _CMP_LT_OQ),
					_mm256_cmp_ps(FloatX8Abs(sepZ), _mm256_set1_ps(CORE_RADIUS), _CMP_LT_OQ)));
			if (_mm256_movemask_ps(tooCloseMask))
			{
				__m256 offsetX = m_randomOffsets[(randomOffsetIdx++) % std::size(m_randomOffsets)];
				sepX = _mm256_add_ps(sepX, _mm256_blendv_ps(_mm256_setzero_ps(), offsetX, tooCloseMask));

				__m256 offsetY = m_randomOffsets[(randomOffsetIdx++) % std::size(m_randomOffsets)];
				sepY = _mm256_add_ps(sepY, _mm256_blendv_ps(_mm256_setzero_ps(), offsetY, tooCloseMask));

				__m256 offsetZ = m_randomOffsets[(randomOffsetIdx++) % std::size(m_randomOffsets)];
				sepZ = _mm256_add_ps(sepZ, _mm256_blendv_ps(_mm256_setzero_ps(), offsetZ, tooCloseMask));

				dist = _mm256_sqrt_ps(_mm256_add_ps(
					_mm256_add_ps(_mm256_mul_ps(sepX, sepX), _mm256_mul_ps(sepY, sepY)), _mm256_mul_ps(sepZ, sepZ)));
			}

			__m256 dv = _mm256_mul_ps(dist, _mm256_set1_ps(1.0f / INFLUENCE_RADIUS));
			__m256 q = _mm256_max_ps(_mm256_sub_ps(_mm256_set1_ps(1), dv), _mm256_set1_ps(0));
			__m256 q2 = _mm256_mul_ps(q, q);
			__m256 q3 = _mm256_mul_ps(q2, q);

			__m256 densB = _mm256_i32gather_ps(m_particleDensityX, indices, sizeof(float));
			__m256 nearDensB = _mm256_i32gather_ps(m_particleDensityY, indices, sizeof(float));
			__m256 pressure = _mm256_mul_ps(
				_mm256_set1_ps(STIFFNESS), _mm256_add_ps(densB, _mm256_set1_ps(aDensityXSub2TargetDensity)));
			__m256 pressureNear = _mm256_mul_ps(
				_mm256_set1_ps(STIFFNESS * NEAR_TO_FAR), _mm256_add_ps(nearDensB, _mm256_set1_ps(nearDensA)));

			__m256 accelScale = _mm256_div_ps(
				_mm256_add_ps(_mm256_mul_ps(pressure, q2), _mm256_mul_ps(pressureNear, q3)),
				_mm256_add_ps(dist, _mm256_set1_ps(FLT_EPSILON)));

			accelX = _mm256_add_ps(accelX, _mm256_mul_ps(sepX, accelScale));
			accelY = _mm256_add_ps(accelY, _mm256_mul_ps(sepY, accelScale));
			accelZ = _mm256_add_ps(accelZ, _mm256_mul_ps(sepZ, accelScale));
		}

		glm::vec3 accel = relativeDensity * gravities[m_particlesGravity[a]];
		accel.x += SumFloatx8(accelX);
		accel.y += SumFloatx8(accelY);
		accel.z += SumFloatx8(accelZ);

		// Applies acceleration to the particle's velocity
		glm::vec3 velAdd = accel * args.dt;
		m_particlesVel1.x[a] += velAdd.x;
		m_particlesVel1.y[a] += velAdd.y;
		m_particlesVel1.z[a] += velAdd.z;
	}
}
