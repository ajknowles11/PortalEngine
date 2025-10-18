#version 450

#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outLocalPos;

layout(set = 0, binding = 0) uniform  SceneData
{   
	mat4 view;
	mat4 invView;
	mat4 proj;
	mat4 viewProj;
	mat4 cullViewProj;
} sceneData;

layout(buffer_reference, std430) readonly buffer VertexBuffer
{ 
	vec4 vertices[];
};

layout( push_constant ) uniform PushConstants
{
	VertexBuffer vertexBuffer;
} constants;

void main() 
{
	outLocalPos = constants.vertexBuffer.vertices[gl_VertexIndex].xyz;

	mat4 rotView = mat4(mat3(sceneData.view));
	vec4 position = sceneData.proj * rotView * vec4(outLocalPos, 1.0);

	position.z = 0;

	gl_Position =  position;
}