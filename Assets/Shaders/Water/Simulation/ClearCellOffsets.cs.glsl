layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(set = 0, binding = 0, r32ui) restrict writeonly uniform uimage3D cellOffsetsImage;

void main()
{
	if (all(lessThan(gl_GlobalInvocationID, imageSize(cellOffsetsImage))))
	{
		imageStore(cellOffsetsImage, ivec3(gl_GlobalInvocationID), uvec4(0xFFFFu));
	}
}
