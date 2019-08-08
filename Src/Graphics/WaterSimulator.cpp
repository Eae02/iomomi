#include "WaterSimulator.hpp"
#include "../World/World.hpp"
#include "../World/Entities/WaterPlane.hpp"
#include "../World/Entities/ECLiquidPlane.hpp"
#include "../Vec3Compare.hpp"

constexpr uint32_t MAX_PARTICLES = 16384;
constexpr uint32_t MAX_CLOSE_PARTICLES = 64;
constexpr uint32_t NOISE_DATA_SIZE = 128;
constexpr uint32_t MAX_WORLD_SIZE = 128;

WaterSimulator::WaterSimulator()
{
	m_localSizeX = 64;
	
	uint32_t specConstData[] = { m_localSizeX };
	const eg::SpecializationConstantEntry specConstants[] = { { 0, 0, 4 } };
	
	eg::ComputePipelineCreateInfo pipeline0CI;
	pipeline0CI.computeShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/Water0DetectClose.cs.glsl").DefaultVariant();
	pipeline0CI.computeShader.specConstants = specConstants;
	pipeline0CI.computeShader.specConstantsData = specConstData;
	pipeline0CI.computeShader.specConstantsDataSize = sizeof(specConstData);
	m_pipeline0DetectClose = eg::Pipeline::Create(pipeline0CI);
	
	eg::ComputePipelineCreateInfo pipeline1CI;
	pipeline1CI.computeShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/Water1DensityComp.cs.glsl").DefaultVariant();
	m_pipeline1DensityComp = eg::Pipeline::Create(pipeline1CI);
	
	eg::ComputePipelineCreateInfo pipeline2CI;
	pipeline2CI.computeShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/Water2AccelComp.cs.glsl").DefaultVariant();
	m_pipeline2AccelComp = eg::Pipeline::Create(pipeline2CI);
	
	eg::ComputePipelineCreateInfo pipeline3CI;
	pipeline3CI.computeShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/Water3VelDiffusion.cs.glsl").DefaultVariant();
	m_pipeline3VelDiffusion = eg::Pipeline::Create(pipeline3CI);
	
	eg::ComputePipelineCreateInfo pipeline4CI;
	pipeline4CI.computeShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/Simulation/Water4Collision.cs.glsl").DefaultVariant();
	pipeline4CI.computeShader.specConstants = specConstants;
	pipeline4CI.computeShader.specConstantsData = specConstData;
	pipeline4CI.computeShader.specConstantsDataSize = sizeof(specConstData);
	m_pipeline4Collision = eg::Pipeline::Create(pipeline4CI);
	
	m_positionsBuffer = eg::Buffer(
		eg::BufferFlags::StorageBuffer | eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst,
		sizeof(float) * 4 * MAX_PARTICLES,
		nullptr);
	
	m_densityBuffer = eg::Buffer(eg::BufferFlags::StorageBuffer, sizeof(float) * 2 * MAX_PARTICLES, nullptr);
	
	m_numCloseBuffer = eg::Buffer(eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst, sizeof(uint32_t) * MAX_PARTICLES, nullptr);
	
	m_velocityBuffers[0] = eg::Buffer(eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst, sizeof(float) * 4 * MAX_PARTICLES, nullptr);
	m_velocityBuffers[1] = eg::Buffer(eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst, sizeof(float) * 4 * MAX_PARTICLES, nullptr);
	
	eg::TextureCreateInfo closeParticlesCI;
	closeParticlesCI.width = MAX_PARTICLES;
	closeParticlesCI.height = MAX_CLOSE_PARTICLES;
	closeParticlesCI.mipLevels = 1;
	closeParticlesCI.flags = eg::TextureFlags::CopyDst | eg::TextureFlags::StorageImage;
	closeParticlesCI.format = eg::Format::R16_UInt;
	m_closeParticlesImage = eg::Texture::Create2D(closeParticlesCI);
	
	{
		constexpr float CORE_RADIUS = 0.001;
		float noiseData[NOISE_DATA_SIZE * 4];
		std::mt19937 rng(std::time(nullptr));
		std::uniform_real_distribution<float> noiseDataRange(-CORE_RADIUS / 2, CORE_RADIUS / 2);
		for (float& noise : noiseData)
			noise = noiseDataRange(rng);
		
		m_noiseBuffer = eg::Buffer(eg::BufferFlags::UniformBuffer, sizeof(noiseData), noiseData);
		m_noiseBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Compute);
	}
	
	eg::TextureCreateInfo voxelSolidityImageCI;
	voxelSolidityImageCI.format = eg::Format::R8_UInt;
	voxelSolidityImageCI.flags = eg::TextureFlags::StorageImage | eg::TextureFlags::CopyDst;
	voxelSolidityImageCI.mipLevels = 1;
	voxelSolidityImageCI.width = MAX_WORLD_SIZE;
	voxelSolidityImageCI.height = MAX_WORLD_SIZE;
	voxelSolidityImageCI.depth = MAX_WORLD_SIZE;
	m_voxelSolidityImage = eg::Texture::Create3D(voxelSolidityImageCI);
}

void WaterSimulator::Init(const World& world)
{
	glm::ivec3 soliditySize = world.GetBoundsMax() - world.GetBoundsMin();
	if (soliditySize.x > (int)MAX_WORLD_SIZE || soliditySize.y > (int)MAX_WORLD_SIZE || soliditySize.z > (int)MAX_WORLD_SIZE)
	{
		m_numParticles = 0;
		eg::Log(eg::LogLevel::Warning, "water", "World too big for water simulation");
		return;
	}
	
	//Generates a set of all underwater cells
	std::vector<glm::ivec3> underwaterCells;
	static eg::EntitySignature waterPlaneSignature = eg::EntitySignature::Create<ECLiquidPlane, ECWaterPlane>();
	for (eg::Entity& waterPlaneEntity : world.EntityManager().GetEntitySet(waterPlaneSignature))
	{
		ECLiquidPlane& liquidPlane = waterPlaneEntity.GetComponent<ECLiquidPlane>();
		liquidPlane.MaybeUpdate(waterPlaneEntity, world);
		for (glm::ivec3 cell : liquidPlane.UnderwaterCells())
		{
			underwaterCells.push_back(cell);
		}
	}
	if (underwaterCells.empty())
	{
		m_numParticles = 0;
		return;
	}
	std::sort(underwaterCells.begin(), underwaterCells.end(), IVec3Compare());
	size_t lastCell = std::unique(underwaterCells.begin(), underwaterCells.end()) - underwaterCells.begin();
	
	const int DENSITY = 4;
	
	//Generates positions
	std::vector<glm::vec3> positions;
	for (size_t i = 0; i < lastCell; i++)
	{
		for (int x = 0; x < DENSITY; x++)
		for (int y = 0; y < DENSITY; y++)
		for (int z = 0; z < DENSITY; z++)
		{
			positions.push_back(glm::vec3(underwaterCells[i]) + (glm::vec3(x, y, z) + 0.5f) / (float)DENSITY);
		}
	}
	
	m_numParticles = positions.size();
	if (m_numParticles > MAX_PARTICLES)
	{
		eg::Log(eg::LogLevel::Warning, "water", "Too many water particles, {0} / {1}", positions.size(), MAX_PARTICLES);
		m_numParticles = MAX_PARTICLES;
	}
	m_dispatchSizeX = (m_numParticles + m_localSizeX - 1) / m_localSizeX;
	
	constexpr uint32_t SOLIDITY_BYTES = MAX_WORLD_SIZE * MAX_WORLD_SIZE * MAX_WORLD_SIZE;
	
	uint64_t positionsBytes = sizeof(float) * 4 * m_numParticles;
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(positionsBytes * 2 + SOLIDITY_BYTES);
	auto* uploadMem = static_cast<glm::vec4*>(uploadBuffer.Map());
	
	//Writes positions and velocities to the upload buffer
	for (size_t i = 0; i < m_numParticles; i++)
	{
		uploadMem[i] = glm::vec4(positions[i], 0.0f);
		uploadMem[m_numParticles + i] = glm::vec4(0.0f);
	}
	
	//Writes solidity to the upload buffer
	uint8_t* isAirOut = reinterpret_cast<uint8_t*>(uploadMem + m_numParticles * 2);
	std::memset(isAirOut, 0, SOLIDITY_BYTES);
	for (int z = 0; z < soliditySize.z; z++)
	{
		for (int y = 0; y < soliditySize.y; y++)
		{
			for (int x = 0; x < soliditySize.x; x++)
			{
				isAirOut[x + y * MAX_WORLD_SIZE + z * MAX_WORLD_SIZE * MAX_WORLD_SIZE] =
					world.IsAir(glm::ivec3(x, y, z) + world.GetBoundsMin());
			}
		}
	}
	
	uploadBuffer.Flush();
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_positionsBuffer, uploadBuffer.offset, 0, positionsBytes);
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_velocityBuffers[0], uploadBuffer.offset + positionsBytes, 0, positionsBytes);
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_velocityBuffers[1], uploadBuffer.offset + positionsBytes, 0, positionsBytes);
	
	eg::TextureRange texRange = { };
	texRange.sizeX = MAX_WORLD_SIZE;
	texRange.sizeY = MAX_WORLD_SIZE;
	texRange.sizeZ = MAX_WORLD_SIZE;
	eg::DC.SetTextureData(m_voxelSolidityImage, texRange, uploadBuffer.buffer, uploadBuffer.offset + positionsBytes * 2);
	
	m_voxelSolidityImage.UsageHint(eg::TextureUsage::ILSRead, eg::ShaderAccessFlags::Compute);
	
	m_positionsBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	
	m_solidityMin = world.GetBoundsMin(); 
}

#pragma pack(push, 1)
struct PC
{
	uint32_t numParticles;
	float dt;
};

struct CollisionPC
{
	uint32_t numParticles;
	float dt;
	float _p1;
	float _p2;
	glm::ivec3 solidityMin;
};
#pragma pack(pop)

void WaterSimulator::Update(float dt)
{
	if (m_numParticles == 0)
		return;
	
	dt = std::min(dt, 1.0f / 30.0f);
	
	auto cpuTimer = eg::StartCPUTimer("Water Update");
	auto gpuTimer = eg::StartGPUTimer("Water Update");
	
	PC pc;
	pc.numParticles = m_numParticles;
	pc.dt = dt;
	
	eg::BufferRef mainVelocityBuffer = m_velocityBuffers[m_mainVelocityBuffer];
	eg::BufferRef auxVelocityBuffer = m_velocityBuffers[m_auxVelocityBuffer];
	
	// ** Stage 0: Detect close **
	{
		auto detectCloseTimer = eg::StartGPUTimer("Detect Close");
		
		eg::DC.ClearColorTexture(m_closeParticlesImage, 0, glm::ivec4(0));
		
		eg::DC.FillBuffer(m_numCloseBuffer, 0, m_numParticles * sizeof(uint32_t), 0);
		
		m_closeParticlesImage.UsageHint(eg::TextureUsage::ILSWrite, eg::ShaderAccessFlags::Compute);
		m_numCloseBuffer.UsageHint(eg::BufferUsage::StorageBufferReadWrite, eg::ShaderAccessFlags::Compute);
		m_positionsBuffer.UsageHint(eg::BufferUsage::StorageBufferRead, eg::ShaderAccessFlags::Compute);
		
		eg::DC.BindPipeline(m_pipeline0DetectClose);
		
		eg::DC.BindStorageBuffer(m_positionsBuffer,    0, 0, 0, sizeof(float) * 4 * m_numParticles);
		eg::DC.BindStorageBuffer(m_numCloseBuffer,     0, 1, 0, sizeof(uint32_t) * m_numParticles);
		eg::DC.BindStorageImage(m_closeParticlesImage, 0, 2);
		
		eg::DC.PushConstants(0, 4, &pc);
		
		eg::DC.DispatchCompute(m_dispatchSizeX, (m_numParticles + 1) / 2, 1);
	}
	
	// ** Stage 1: Density Computation **
	{
		auto densityTimer = eg::StartGPUTimer("Density");
		
		m_densityBuffer.UsageHint(eg::BufferUsage::StorageBufferWrite, eg::ShaderAccessFlags::Compute);
		m_closeParticlesImage.UsageHint(eg::TextureUsage::ILSRead, eg::ShaderAccessFlags::Compute);
		
		eg::DC.BindPipeline(m_pipeline1DensityComp);
		
		eg::DC.BindStorageBuffer(m_positionsBuffer,    0, 0, 0, sizeof(float) * 4 * m_numParticles);
		eg::DC.BindStorageBuffer(m_densityBuffer,      0, 1, 0, sizeof(float) * 2 * m_numParticles);
		eg::DC.BindStorageImage(m_closeParticlesImage, 0, 2);
		
		eg::DC.PushConstants(0, 4, &pc);
		
		eg::DC.DispatchCompute(m_numParticles, 1, 1);
	}
	
	// ** Stage 2: Acceleration Computation **
	{
		auto densityTimer = eg::StartGPUTimer("Acceleration");
		
		m_densityBuffer.UsageHint(eg::BufferUsage::StorageBufferRead, eg::ShaderAccessFlags::Compute);
		mainVelocityBuffer.UsageHint(eg::BufferUsage::StorageBufferReadWrite, eg::ShaderAccessFlags::Compute);
		
		eg::DC.BindPipeline(m_pipeline2AccelComp);
		
		eg::DC.BindStorageBuffer(m_positionsBuffer,  0, 0, 0, sizeof(float) * 4 * m_numParticles);
		eg::DC.BindStorageBuffer(mainVelocityBuffer, 0, 1, 0, sizeof(float) * 4 * m_numParticles);
		eg::DC.BindStorageBuffer(m_densityBuffer,    0, 2, 0, sizeof(float) * 2 * m_numParticles);
		eg::DC.BindUniformBuffer(m_noiseBuffer,      0, 3, 0, sizeof(float) * 4 * NOISE_DATA_SIZE);
		eg::DC.BindStorageImage(m_closeParticlesImage, 0, 4);
		
		eg::DC.PushConstants(0, 8, &pc);
		
		eg::DC.DispatchCompute(m_numParticles, 1, 1);
	}
	
	// ** Stage 3: Velocity Diffusion **
	{
		auto densityTimer = eg::StartGPUTimer("Velocity diffusion");
		
		mainVelocityBuffer.UsageHint(eg::BufferUsage::StorageBufferRead, eg::ShaderAccessFlags::Compute);
		auxVelocityBuffer.UsageHint(eg::BufferUsage::StorageBufferWrite, eg::ShaderAccessFlags::Compute);
		
		eg::DC.BindPipeline(m_pipeline3VelDiffusion);
		
		eg::DC.BindStorageBuffer(m_positionsBuffer,  0, 0, 0, sizeof(float) * 4 * m_numParticles);
		eg::DC.BindStorageBuffer(mainVelocityBuffer, 0, 1, 0, sizeof(float) * 4 * m_numParticles);
		eg::DC.BindStorageBuffer(auxVelocityBuffer,  0, 2, 0, sizeof(float) * 4 * m_numParticles);
		
		eg::DC.PushConstants(0, 4, &pc);
		
		eg::DC.DispatchCompute(m_numParticles, 1, 1);
	}
	
	// ** Stage 4: Collision **
	auto collisionTimer = eg::StartGPUTimer("Collision");
	
	m_positionsBuffer.UsageHint(eg::BufferUsage::StorageBufferRead, eg::ShaderAccessFlags::Compute);
	auxVelocityBuffer.UsageHint(eg::BufferUsage::StorageBufferReadWrite, eg::ShaderAccessFlags::Compute);
	
	eg::DC.BindPipeline(m_pipeline4Collision);
	
	eg::DC.BindStorageBuffer(m_positionsBuffer,  0, 0, 0, sizeof(float) * 4 * m_numParticles);
	eg::DC.BindStorageBuffer(auxVelocityBuffer,  0, 1, 0, sizeof(float) * 4 * m_numParticles);
	eg::DC.BindStorageImage(m_voxelSolidityImage, 0, 2);
	
	CollisionPC colPC;
	colPC.numParticles = m_numParticles;
	colPC.dt = dt;
	colPC.solidityMin = m_solidityMin;
	eg::DC.PushConstants(0, sizeof(CollisionPC), &colPC);
	
	eg::DC.DispatchCompute(m_dispatchSizeX, 1, 1);
	
	m_positionsBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	
	std::swap(m_mainVelocityBuffer, m_auxVelocityBuffer);
}

eg::BufferRef WaterSimulator::GetPositionsBuffer() const
{
	return m_positionsBuffer;
}

uint32_t WaterSimulator::NumParticles() const
{
	return m_numParticles;
}
