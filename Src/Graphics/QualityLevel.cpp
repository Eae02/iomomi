#include "QualityLevel.hpp"

QualityVariable<int> qvar::waterBlurSamples(6, 10, 10, 16, 24);
QualityVariable<float> qvar::waterBaseBlurRadius(60.0f, 80.0f, 100.0f, 120.0f, 150.0f);
QualityVariable<bool> qvar::waterUseHQShader(QualityLevel::High);
QualityVariable<bool> qvar::waterUse32BitDepth(QualityLevel::Medium);
QualityVariable<bool> qvar::waterRenderCaustics(QualityLevel::Low);

QualityVariable<uint32_t> qvar::ssrLinearSamples(0, 4, 4, 6, 8);
QualityVariable<uint32_t> qvar::ssrBinSearchSamples(0, 4, 6, 6, 8);
QualityVariable<uint32_t> qvar::ssrBlurRadius(0, 8, 12, 12, 16);
QualityVariable<bool>     qvar::ssrUse16BitColor(QualityLevel::High);

QualityVariable<float> qvar::shadowSoftness(0.0f, 0.0f, 1.0f, 1.3f, 1.6f);
QualityVariable<int> qvar::shadowResolution(256, 512, 512, 768, 1024);
QualityVariable<int> qvar::shadowUpdateLimitPerFrame(5, 5, 10, 15, 20);

QualityVariable<int> qvar::gravitySwitchVolLightSamples(0, 0, 16, 24, 32);
QualityVariable<bool> qvar::renderBlurredGlass(QualityLevel::Medium);
