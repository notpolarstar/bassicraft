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
    // vec4 viewPos = ubo.view * ubo.model * vec4(inPosition, 1.0);
    // float dist = -viewPos.z;
    // gl_Position = ubo.proj * (viewPos + vec4(inPosition.xy * dist, 0, 0));
    // vec4 billboard_pos = ubo.model[3];
    mat4 model = ubo.model;
    model[3] = vec4(0, 0, 0, 1);
    gl_Position = ubo.proj * (ubo.view * model * ubo.model[3] + vec4(inPosition.xy, 0, 0));
    //gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    position = gl_Position.xyz;
}