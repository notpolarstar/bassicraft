#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 3) in vec3 instancePosition;
layout(location = 4) in float instanceSize;
layout(location = 5) in uint instanceTexIndex;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 position;

void main() {
    mat4 model = ubo.model;
    model[3] = vec4(0, 0, 0, 1);
    gl_Position = ubo.proj * (ubo.view * model * vec4(instancePosition, 1) + vec4(inPosition.xy * instanceSize, 0, 0));
    fragColor = inColor;
    fragTexCoord = vec2(inTexCoord.x + mod(instanceTexIndex - 1.0, 16.0) / 16.0, inTexCoord.y + floor((instanceTexIndex - 1) / 16.0) / 16.0);
    position = gl_Position.xyz;
}

// #version 450

// layout(binding = 0) uniform UniformBufferObject {
//     mat4 model;
//     mat4 view;
//     mat4 proj;
// } ubo;

// layout(location = 0) in vec3 inPosition;
// layout(location = 1) in vec3 inColor;
// layout(location = 2) in vec2 inTexCoord;

// layout(location = 0) out vec3 fragColor;
// layout(location = 1) out vec2 fragTexCoord;
// layout(location = 2) out vec3 position;

// void main() {
//     mat4 model = ubo.model;
//     model[3] = vec4(0, 0, 0, 1);
//     gl_Position = ubo.proj * (ubo.view * model * ubo.model[3] + vec4(inPosition.xy, 0, 0));
//     fragColor = inColor;
//     fragTexCoord = inTexCoord;
//     position = gl_Position.xyz;
// }