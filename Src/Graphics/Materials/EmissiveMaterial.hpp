#pragma once

class EmissiveMaterial : public eg::IMaterial
{
public:
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	const eg::ColorLin& Color() const
	{
		return m_color;
	}
	
	void SetColor(const eg::ColorLin& color)
	{
		m_color = color;
	}
	
private:
	eg::ColorLin m_color;
};
