#pragma once

enum class QualityLevel
{
	VeryLow,
	Low,
	Medium,
	High,
	VeryHigh
};

inline bool operator<(QualityLevel a, QualityLevel b)
{
	return (int)a < (int)b;
}

inline bool operator<=(QualityLevel a, QualityLevel b)
{
	return (int)a <= (int)b;
}

inline bool operator>(QualityLevel a, QualityLevel b)
{
	return (int)a > (int)b;
}

inline bool operator>=(QualityLevel a, QualityLevel b)
{
	return (int)a >= (int)b;
}

template <typename T>
struct QualityVariable
{
	T values[5];
	
	T operator()(QualityLevel level) const { return values[(int)level]; }
	
	QualityVariable(T vLow, T low, T medium, T high, T vHigh) noexcept
		: values{vLow, low, medium, high, vHigh} { }
	
	template <class = std::enable_if<std::is_same_v<T, bool>>>
	QualityVariable(QualityLevel minLevel) noexcept
		: values{
			minLevel <= QualityLevel::VeryLow,
			minLevel <= QualityLevel::Low,
			minLevel <= QualityLevel::Medium,
			minLevel <= QualityLevel::High,
			minLevel <= QualityLevel::VeryHigh
		} { }
};

namespace qvar
{
	extern QualityVariable<int>   waterBlurSamples;
	extern QualityVariable<float> waterBaseBlurRadius;
	extern QualityVariable<bool>  waterUseHQShader;
	extern QualityVariable<bool>  waterUse32BitDepth;
	extern QualityVariable<bool>  waterRenderCaustics;
	
	extern QualityVariable<uint32_t> ssrLinearSamples;
	extern QualityVariable<uint32_t> ssrBinSearchSamples;
	extern QualityVariable<uint32_t> ssrBlurRadius;
	
	extern QualityVariable<float> shadowSoftness;
	extern QualityVariable<int> shadowResolution;
	extern QualityVariable<int> shadowUpdateLimitPerFrame;
	
	extern QualityVariable<int> gravitySwitchVolLightSamples;
	
	extern QualityVariable<bool> renderBlurredGlass;
};
