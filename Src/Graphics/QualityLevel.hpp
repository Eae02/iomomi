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
