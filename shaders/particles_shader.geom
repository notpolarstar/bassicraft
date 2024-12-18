#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

// layout(binding = 0) uniform UniformBufferObject {
//     mat4 model;
//     mat4 view;
//     mat4 proj;
// } ubo;

// layout(location = 0) in vec3 fragPos[];
// layout(location = 1) in vec2 texCoords[];

// layout(location = 0) out vec2 TexCoords;

void main() {
    gl_Position = gl_in[0].gl_Position + vec4(-1.0, -1.0, 0.0, 0.0);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4( 1.0, -1.0, 0.0, 0.0);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(-1.0,  1.0, 0.0, 0.0);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4( 1.0,  1.0, 0.0, 0.0);
    EmitVertex();

    EndPrimitive();
}