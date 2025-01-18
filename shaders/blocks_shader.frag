#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 position;
layout(location = 2) in float fragLight;

layout(location = 0) out vec4 outColor;

void main() {
    if (texture(texSampler, fragTexCoord).a < 0.1) {
        discard;
    }

    outColor = texture(texSampler, fragTexCoord);

    // Apply fog

    float fogStart = 100.0;
    float fogEnd = 120.0;
    float fogFactor = -(fogEnd - position.z) / (fogEnd - fogStart);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    outColor.rgb = mix(outColor.rgb, vec3(0.5, 0.5, 0.5), fogFactor);

    // Apply lighting
    float light = float(fragLight) / 15.0;
    outColor.rgb *= light;
}