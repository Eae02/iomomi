#include "PointLight.hpp"

const eg::DescriptorSetBinding PointLightShadowDrawArgs::PARAMETERS_DS_BINDINGS[1] = {
	eg::DescriptorSetBinding{
		.binding = 0,
		.type = eg::BindingType::UniformBufferDynamicOffset,
		.shaderAccess = eg::ShaderAccessFlags::Vertex | eg::ShaderAccessFlags::Fragment,
	},
};

void PointLightShadowDrawArgs::BindParametersDescriptorSet(eg::CommandContext& cc, uint32_t setIndex) const
{
	cc.BindDescriptorSet(parametersDescriptorSet, setIndex, { &parametersBufferOffset, 1 });
}
