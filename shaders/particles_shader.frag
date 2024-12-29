#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 position;

layout(location = 0) out vec4 outColor;

void main() {
    if (texture(texSampler, fragTexCoord).a < 0.1) {
        discard;
    }
    
    outColor = texture(texSampler, fragTexCoord);
}