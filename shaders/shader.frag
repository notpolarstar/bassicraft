#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 position;

layout(location = 0) out vec4 outColor;

void main() {
    if (texture(texSampler, fragTexCoord).a < 0.1) {
        discard;
    }

    vec4 fog_color = vec4(0.5, 0.5, 0.5, 1.0);
    float dist = length(position.xyz);
    float fog_factor = smoothstep(80.0, 100.0, dist);
    fog_factor = clamp(fog_factor, 0.0, 1.0);
    
    outColor = mix(vec4(fragColor, 1.0) * texture(texSampler, fragTexCoord), fog_color, fog_factor);

    //outColor = vec4(fragColor, 1.0) * texture(texSampler, fragTexCoord);
}

// #version 450
// #extension GL_ARB_separate_shader_objects : enable

// layout (location = 0) in vec3 fragColor;

// layout (location = 0) out vec4 outColor;

// void main () { outColor = vec4 (fragColor, 1.0); }
