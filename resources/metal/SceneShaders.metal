// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include <metal_stdlib>
using namespace metal;

struct SceneUniforms {
    float4 cameraPosition;
    float4 cameraRight;
    float4 cameraUp;
    float4 cameraForward;
    float4 viewportTime;
    float4 renderSettings;
};

struct SphereVertex { float4 position; };

struct PlanetInstance {
    float4 centerRadius;
    float4 colour0;
    float4 colour1;
    float4 colour2;
    float4 colour3;
    float4 style0;
    float4 style1;
};

struct StarInstance {
    float4 directionMagnitude;
    float4 colourKind;
};

struct TrailVertex {
    float4 previousSide;
    float4 currentAge;
    float4 nextWidth;
    float4 colour;
};

struct FullscreenOut {
    float4 position [[position]];
    float2 uv;
};

struct PlanetOut {
    float4 position [[position]];
    float3 worldPosition;
    float3 worldNormal;
    float3 localNormal;
    float4 colour0 [[flat]];
    float4 colour1 [[flat]];
    float4 colour2 [[flat]];
    float4 colour3 [[flat]];
    float4 style0 [[flat]];
    float4 style1 [[flat]];
};

struct ColourOut {
    float4 position [[position]];
    float4 colour;
    float2 uv;
};

constant float pi = 3.14159265358979323846;

float4 projectWorld(float3 world, constant SceneUniforms& u) {
    const float3 relative = world - u.cameraPosition.xyz;
    const float x = dot(relative, u.cameraRight.xyz);
    const float y = dot(relative, u.cameraUp.xyz);
    const float z = max(0.01, dot(relative, u.cameraForward.xyz));
    const float aspect = u.viewportTime.x / max(1.0, u.viewportTime.y);
    const float tanHalfFov = tan(pi * 0.125);
    constexpr float nearPlane = 0.05;
    constexpr float farPlane = 120.0;
    return float4(x / (tanHalfFov * aspect), y / tanHalfFov,
                  (farPlane * z - nearPlane * farPlane) / (farPlane - nearPlane), z);
}

float hash31(float3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float valueNoise(float3 p) {
    const float3 cell = floor(p);
    float3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    const float n000 = hash31(cell + float3(0, 0, 0));
    const float n100 = hash31(cell + float3(1, 0, 0));
    const float n010 = hash31(cell + float3(0, 1, 0));
    const float n110 = hash31(cell + float3(1, 1, 0));
    const float n001 = hash31(cell + float3(0, 0, 1));
    const float n101 = hash31(cell + float3(1, 0, 1));
    const float n011 = hash31(cell + float3(0, 1, 1));
    const float n111 = hash31(cell + float3(1, 1, 1));
    const float x00 = mix(n000, n100, f.x);
    const float x10 = mix(n010, n110, f.x);
    const float x01 = mix(n001, n101, f.x);
    const float x11 = mix(n011, n111, f.x);
    return mix(mix(x00, x10, f.y), mix(x01, x11, f.y), f.z);
}

float fbm(float3 p) {
    float value = 0.0;
    float amplitude = 0.52;
    for (int octave = 0; octave < 5; ++octave) {
        value += valueNoise(p) * amplitude;
        p = p * 2.03 + float3(11.7, 7.1, 19.3);
        amplitude *= 0.49;
    }
    return value;
}

float3 rotatePlanet(float3 p, float angle, float tilt) {
    const float cy = cos(angle), sy = sin(angle);
    p = float3(cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z);
    const float cx = cos(tilt), sx = sin(tilt);
    return float3(p.x, cx * p.y - sx * p.z, sx * p.y + cx * p.z);
}

vertex FullscreenOut fullscreenVertex(uint vertexId [[vertex_id]]) {
    const float2 positions[3] = {float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};
    FullscreenOut out;
    out.position = float4(positions[vertexId], 0.999, 1.0);
    out.uv = positions[vertexId] * 0.5 + 0.5;
    return out;
}

fragment float4 backgroundFragment(FullscreenOut in [[stage_in]],
                                   constant SceneUniforms& u [[buffer(0)]]) {
    const float2 ndc = in.uv * 2.0 - 1.0;
    const float aspect = u.viewportTime.x / max(1.0, u.viewportTime.y);
    const float tanHalfFov = tan(pi * 0.125);
    const float3 direction = normalize(u.cameraForward.xyz
        + u.cameraRight.xyz * ndc.x * aspect * tanHalfFov
        + u.cameraUp.xyz * ndc.y * tanHalfFov);
    const float band = exp(-pow(abs(dot(direction, normalize(float3(0.21, 0.91, 0.35)))) * 5.2, 1.35));
    const float dust = fbm(direction * 5.5 + float3(3.0, 9.0, 1.0));
    const float lanes = smoothstep(0.38, 0.75, dust) * band;
    const float vignette = smoothstep(1.35, 0.18, length(ndc * float2(0.72, 1.0)));
    float3 colour = mix(float3(0.0007, 0.0012, 0.004), float3(0.004, 0.008, 0.018), vignette);
    colour += lanes * mix(float3(0.006, 0.009, 0.018), float3(0.025, 0.018, 0.038), dust);
    return float4(colour, 1.0);
}

vertex ColourOut starVertex(const device StarInstance* stars [[buffer(0)]],
                            constant SceneUniforms& u [[buffer(1)]],
                            uint vertexId [[vertex_id]], uint instanceId [[instance_id]]) {
    const float2 corners[6] = {
        float2(-1, -1), float2(1, -1), float2(-1, 1),
        float2(-1, 1), float2(1, -1), float2(1, 1)
    };
    const StarInstance star = stars[instanceId];
    const float3 direction = normalize(star.directionMagnitude.xyz);
    const float cameraX = dot(direction, u.cameraRight.xyz);
    const float cameraY = dot(direction, u.cameraUp.xyz);
    const float cameraZ = dot(direction, u.cameraForward.xyz);
    const float aspect = u.viewportTime.x / max(1.0, u.viewportTime.y);
    const float tanHalfFov = tan(pi * 0.125);
    float4 clip = float4(cameraX / (tanHalfFov * aspect), cameraY / tanHalfFov,
                         cameraZ * 0.999, cameraZ);
    const float magnitude = star.directionMagnitude.w;
    const float catalogue = star.colourKind.w;
    const float brightness = clamp(exp2(-0.4 * (magnitude - 5.0)) * 0.24, 0.08, 1.35);
    const float faintScale = mix(u.renderSettings.y, 1.0, catalogue);
    const float size = (0.58 + sqrt(brightness) * 0.92) * mix(0.72, 1.0, faintScale);
    const float2 corner = corners[vertexId];
    clip.xy += corner * float2(size * 2.0 / max(1.0, u.viewportTime.x),
                               size * 2.0 / max(1.0, u.viewportTime.y)) * clip.w;
    if (cameraZ <= 0.0 || faintScale <= 0.001)
        clip = float4(2.0, 2.0, 1.0, 1.0);
    ColourOut out;
    out.position = clip;
    out.uv = corner;
    out.colour = float4(star.colourKind.rgb * brightness * faintScale, brightness);
    return out;
}

fragment float4 starFragment(ColourOut in [[stage_in]]) {
    const float radius = length(in.uv);
    const float core = pow(saturate(1.0 - radius), 3.2);
    const float spike = pow(saturate(1.0 - min(abs(in.uv.x), abs(in.uv.y)) * 6.0), 9.0)
                        * saturate(1.0 - radius) * step(0.8, in.colour.a);
    return float4(in.colour.rgb * (core + spike * 0.07), core);
}

vertex PlanetOut planetVertex(const device SphereVertex* vertices [[buffer(0)]],
                              const device PlanetInstance* instances [[buffer(1)]],
                              constant SceneUniforms& u [[buffer(2)]],
                              uint vertexId [[vertex_id]], uint instanceId [[instance_id]]) {
    const PlanetInstance instance = instances[instanceId];
    const float3 local = vertices[vertexId].position.xyz;
    const float3 rotated = rotatePlanet(local, u.viewportTime.z * instance.style1.x, instance.style1.z);
    const float3 world = instance.centerRadius.xyz + rotated * instance.centerRadius.w;
    PlanetOut out;
    out.position = projectWorld(world, u);
    out.worldPosition = world;
    out.worldNormal = rotated;
    out.localNormal = local;
    out.colour0 = instance.colour0;
    out.colour1 = instance.colour1;
    out.colour2 = instance.colour2;
    out.colour3 = instance.colour3;
    out.style0 = instance.style0;
    out.style1 = instance.style1;
    return out;
}

float terrainField(float3 local, float seed, float scale) {
    const float3 offset = float3(seed * 0.000013, seed * 0.000021, seed * 0.000034);
    const float warp = fbm(local * 1.7 + offset);
    const float base = fbm(local * scale + offset + warp * 1.8);
    const float ridge = 1.0 - abs(fbm(local * scale * 2.1 - offset) * 2.0 - 1.0);
    return base * 0.78 + ridge * 0.22;
}

fragment float4 planetFragment(PlanetOut in [[stage_in]],
                               constant SceneUniforms& u [[buffer(0)]]) {
    const float seed = in.style0.x;
    const float terrain = terrainField(normalize(in.localNormal), seed, in.style0.y);
    const float ocean = smoothstep(in.style0.z - 0.035, in.style0.z + 0.035, terrain);
    const float highland = smoothstep(0.58, 0.86, terrain);
    float3 albedo = mix(in.colour0.rgb, in.colour1.rgb, ocean);
    albedo = mix(albedo, in.colour2.rgb, highland);
    albedo = mix(albedo, in.colour3.rgb, smoothstep(0.74, 0.93, terrain) * 0.72);

    const float epsilon = 0.018;
    const float3 local = normalize(in.localNormal);
    const float3 gradient = float3(
        terrainField(normalize(local + float3(epsilon, 0, 0)), seed, in.style0.y) - terrain,
        terrainField(normalize(local + float3(0, epsilon, 0)), seed, in.style0.y) - terrain,
        terrainField(normalize(local + float3(0, 0, epsilon)), seed, in.style0.y) - terrain) / epsilon;
    const float3 normal = normalize(in.worldNormal - gradient * 0.11);
    const float3 lightDirection = normalize(float3(-0.58, 0.31, 0.75));
    const float3 viewDirection = normalize(u.cameraPosition.xyz - in.worldPosition);
    const float diffuse = smoothstep(-0.12, 0.58, dot(normal, lightDirection));
    const float3 halfVector = normalize(lightDirection + viewDirection);
    const float waterSpecular = pow(saturate(dot(normal, halfVector)), 72.0) * (1.0 - ocean);
    const float fresnel = pow(1.0 - saturate(dot(normal, viewDirection)), 3.5);
    float3 colour = albedo * (0.09 + diffuse * 0.91);
    colour += waterSpecular * in.colour3.rgb * 1.4;
    colour += fresnel * in.colour2.rgb * 0.16;
    return float4(colour, 1.0);
}

fragment float4 cloudFragment(PlanetOut in [[stage_in]],
                              constant SceneUniforms& u [[buffer(0)]]) {
    const float3 local = normalize(in.localNormal);
    const float flow = u.viewportTime.z * in.style1.y;
    const float3 offset = float3(flow, flow * -0.37, flow * 0.23) + in.style0.x * 0.00001;
    const float cloud = smoothstep(0.57, 0.76, fbm(local * (in.style0.y * 2.2) + offset));
    const float3 normal = normalize(in.worldNormal);
    const float3 viewDirection = normalize(u.cameraPosition.xyz - in.worldPosition);
    const float light = 0.18 + 0.82 * smoothstep(-0.2, 0.65,
        dot(normal, normalize(float3(-0.58, 0.31, 0.75))));
    const float rim = pow(1.0 - saturate(dot(normal, viewDirection)), 2.0);
    return float4(mix(in.colour3.rgb, float3(0.92, 0.96, 1.0), 0.65) * light,
                  cloud * (0.17 + rim * 0.22));
}

fragment float4 atmosphereFragment(PlanetOut in [[stage_in]],
                                   constant SceneUniforms& u [[buffer(0)]]) {
    const float3 normal = normalize(in.worldNormal);
    const float3 viewDirection = normalize(u.cameraPosition.xyz - in.worldPosition);
    const float fresnel = pow(1.0 - saturate(dot(normal, viewDirection)), 2.6);
    const float sunlight = smoothstep(-0.45, 0.35,
        dot(normal, normalize(float3(-0.58, 0.31, 0.75))));
    const float alpha = fresnel * (0.16 + sunlight * 0.34);
    return float4(in.colour2.rgb * (0.45 + sunlight * 0.9), alpha);
}

vertex ColourOut trailVertex(const device TrailVertex* vertices [[buffer(0)]],
                             constant SceneUniforms& u [[buffer(1)]], uint vertexId [[vertex_id]]) {
    const TrailVertex sample = vertices[vertexId];
    const float extrusion = (1.0 - sample.currentAge.w) * u.renderSettings.z * 7.0;
    const float3 currentWorld = sample.currentAge.xyz - u.cameraForward.xyz * extrusion;
    const float3 previousWorld = sample.previousSide.xyz - u.cameraForward.xyz * extrusion;
    const float3 nextWorld = sample.nextWidth.xyz - u.cameraForward.xyz * extrusion;
    float4 current = projectWorld(currentWorld, u);
    const float4 previous = projectWorld(previousWorld, u);
    const float4 next = projectWorld(nextWorld, u);
    const float2 previousNdc = previous.xy / max(0.01, previous.w);
    const float2 nextNdc = next.xy / max(0.01, next.w);
    const float2 tangent = normalize(nextNdc - previousNdc + float2(0.00001, 0.0));
    const float2 normal = float2(-tangent.y, tangent.x);
    const float width = sample.nextWidth.w * (0.18 + sample.currentAge.w * 0.82);
    current.xy += normal * sample.previousSide.w * width
                  * float2(2.0 / max(1.0, u.viewportTime.x), 2.0 / max(1.0, u.viewportTime.y))
                  * current.w;
    ColourOut out;
    out.position = current;
    out.uv = float2(sample.previousSide.w, sample.currentAge.w);
    const float fade = sample.currentAge.w * sample.currentAge.w;
    out.colour = float4(sample.colour.rgb * (0.55 + fade * 1.35), sample.colour.a * fade);
    return out;
}

fragment float4 trailFragment(ColourOut in [[stage_in]]) {
    const float edge = smoothstep(1.0, 0.25, abs(in.uv.x));
    return float4(in.colour.rgb * edge, in.colour.a * edge);
}

fragment float4 bloomExtractFragment(FullscreenOut in [[stage_in]],
                                     texture2d<float> source [[texture(0)]]) {
    constexpr sampler linearSampler(filter::linear, address::clamp_to_edge);
    const float3 colour = source.sample(linearSampler, in.uv).rgb;
    const float luminance = dot(colour, float3(0.2126, 0.7152, 0.0722));
    return float4(colour * smoothstep(0.32, 1.25, luminance), 1.0);
}

struct PostUniforms {
    float2 texel;
    float2 direction;
    float bloom;
    float3 padding;
};

fragment float4 blurFragment(FullscreenOut in [[stage_in]], texture2d<float> source [[texture(0)]],
                             constant PostUniforms& post [[buffer(0)]]) {
    constexpr sampler linearSampler(filter::linear, address::clamp_to_edge);
    const float2 step = post.texel * post.direction;
    float3 colour = source.sample(linearSampler, in.uv).rgb * 0.227027;
    colour += source.sample(linearSampler, in.uv + step * 1.384615).rgb * 0.316216;
    colour += source.sample(linearSampler, in.uv - step * 1.384615).rgb * 0.316216;
    colour += source.sample(linearSampler, in.uv + step * 3.230769).rgb * 0.070270;
    colour += source.sample(linearSampler, in.uv - step * 3.230769).rgb * 0.070270;
    return float4(colour, 1.0);
}

float3 acesToneMap(float3 value) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((value * (a * value + b)) / (value * (c * value + d) + e));
}

fragment float4 compositeFragment(FullscreenOut in [[stage_in]],
                                  texture2d<float> scene [[texture(0)]],
                                  texture2d<float> bloomTexture [[texture(1)]],
                                  constant PostUniforms& post [[buffer(0)]]) {
    constexpr sampler linearSampler(filter::linear, address::clamp_to_edge);
    float3 colour = scene.sample(linearSampler, in.uv).rgb;
    colour += bloomTexture.sample(linearSampler, in.uv).rgb * post.bloom * 1.65;
    colour = acesToneMap(colour);
    const float dither = hash31(float3(in.position.xy, 0.37)) - 0.5;
    return float4(colour + dither / 255.0, 1.0);
}
