#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 position;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    position = gl_Position.xyz;
}

// #version 450

// layout (location = 0) out vec3 fragColor;

// layout (location = 0) in vec3 positions;
// layout (location = 1) in vec3 colors;
// layout (location = 2) in vec2 texCoords;

// void main ()
// {
//     gl_Position = vec4(positions, 1.0 + (texCoords.x + texCoords.y) * 0);
//     fragColor = colors;
// }
