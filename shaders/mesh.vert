#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outFragPos;
layout (location = 4) out mat3 outTangentMat;

struct Vertex 
{
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
	vec4 tangent;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer 
{ 
	Vertex vertices[];
};

layout( push_constant ) uniform PushConstants 
{
	mat4 modelMat;
	mat4 normalMat;
	VertexBuffer vertexBuffer;
} constants;

mat3 CalculateTangentMatrix(vec3 normal, vec3 tangent)
{
	// First fix tangent orthogonality to normal vector, using Gram-Schmidt
	tangent = normalize(tangent - dot(tangent, normal) * normal);
	vec3 bitangent = cross(normal, tangent);
	return mat3(tangent, bitangent, normal);
}

void main() 
{
	Vertex v = constants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);

	gl_Position =  sceneData.viewProj * constants.modelMat * position;

	vec3 tangent = normalize(vec3(constants.modelMat * vec4(v.tangent.xyz, 0)));

	outNormal = normalize(mat3(constants.normalMat) * v.normal);
	outColor = v.color.xyz * materialData.colorFactors.xyz;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outFragPos = vec3(constants.modelMat * position);
	outTangentMat = CalculateTangentMatrix(outNormal, tangent);
}
