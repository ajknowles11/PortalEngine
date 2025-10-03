#version 450

#extension GL_EXT_buffer_reference : require

layout(set = 0, binding = 0) uniform  SceneData
{   
	mat4 view;
	mat4 proj;
	mat4 viewProj;
	mat4 cullViewProj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

layout(buffer_reference, std430) readonly buffer VertexBuffer
{ 
	vec4 vertices[];
};

layout( push_constant ) uniform constants
{
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
} PushConstants;

void main() 
{
	vec4 position = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

	gl_Position =  sceneData.viewProj * PushConstants.render_matrix * position;
}