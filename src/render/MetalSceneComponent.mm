// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "render/MetalSceneComponent.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <vector>

namespace threebs {
namespace {

constexpr std::size_t trailCapacity = 384;

struct GpuVertex {
    float x, y, z;
    float r, g, b, a;
    float size;
};

struct Uniforms {
    float time;
    float aspect;
    float extrusion;
    float bloom;
    float stars;
    float orbit;
    float focusX;
    float focusY;
    float focusZ;
};

constexpr std::array<std::array<float, 3>, bodyCount> bodyColours{{
    {{0.96F, 0.63F, 0.22F}},
    {{0.18F, 0.82F, 0.92F}},
    {{0.62F, 0.35F, 0.92F}},
}};

NSString* shaderSource() {
    return @R"METAL(
#include <metal_stdlib>
using namespace metal;

struct Vertex { float x, y, z, r, g, b, a, size; };
struct Uniforms {
    float time, aspect, extrusion, bloom, stars, orbit, focusX, focusY, focusZ;
};
struct Out {
    float4 position [[position]];
    float4 colour;
    float pointSize [[point_size]];
};

vertex Out backgroundVertex(uint id [[vertex_id]]) {
    const float2 points[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
    Out out;
    out.position = float4(points[id], 0.999, 1.0);
    out.colour = float4(1.0);
    out.pointSize = 1.0;
    return out;
}

float hash21(float2 p) {
    p = fract(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

fragment float4 backgroundFragment(Out in [[stage_in]], constant Uniforms& u [[buffer(1)]]) {
    float2 uv = in.position.xy / float2(max(1.0, in.position.w));
    float2 cell = floor(uv * 0.32);
    float star = step(0.9972 - u.stars * 0.0018, hash21(cell));
    float pulse = 0.62 + 0.38 * sin(u.time * 0.35 + hash21(cell + 4.0) * 6.283);
    float vignette = smoothstep(1.25, 0.12, length((uv / float2(1200.0, 760.0)) - 0.5));
    float3 base = mix(float3(0.002, 0.004, 0.012), float3(0.008, 0.012, 0.03), vignette);
    return float4(base + star * pulse * float3(0.42, 0.55, 0.75), 1.0);
}

vertex Out sceneVertex(const device Vertex* vertices [[buffer(0)]],
                       constant Uniforms& u [[buffer(1)]], uint id [[vertex_id]]) {
    Vertex v = vertices[id];
    float3 p = float3(v.x - u.focusX, v.y - u.focusY, v.z - u.focusZ);
    float angle = u.time * u.orbit;
    float c = cos(angle), s = sin(angle);
    p = float3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
    float tilt = -0.34;
    float ct = cos(tilt), st = sin(tilt);
    p = float3(p.x, ct * p.y - st * p.z, st * p.y + ct * p.z);
    float depth = max(2.2, 7.0 - p.z);
    Out out;
    out.position = float4((p.x / depth) / max(0.55, u.aspect) * 2.15, p.y / depth * 2.15,
                          0.25 + p.z * 0.015, 1.0);
    out.colour = float4(v.r, v.g, v.b, v.a);
    out.pointSize = v.size * (7.0 / depth);
    return out;
}

fragment float4 trailFragment(Out in [[stage_in]]) {
    return float4(in.colour.rgb * (0.45 + in.colour.a * 0.9), in.colour.a);
}

fragment float4 planetFragment(Out in [[stage_in]], float2 point [[point_coord]],
                               constant Uniforms& u [[buffer(1)]]) {
    float2 q = point * 2.0 - 1.0;
    float radius2 = dot(q, q);
    if (radius2 > 1.0) discard_fragment();
    float z = sqrt(max(0.0, 1.0 - radius2));
    float3 normal = normalize(float3(q.x, -q.y, z));
    float light = max(0.0, dot(normal, normalize(float3(-0.55, -0.32, 0.78))));
    float limb = pow(max(0.0, z), 0.42);
    float bands = 0.92 + 0.08 * sin((q.y + u.time * 0.004) * 23.0 + q.x * 5.0);
    float3 surface = in.colour.rgb * bands * (0.14 + 0.86 * light) * limb;
    float atmosphere = smoothstep(0.72, 1.0, sqrt(radius2)) * (1.0 - sqrt(radius2));
    surface += in.colour.rgb * atmosphere * (1.4 + u.bloom * 2.0);
    return float4(surface, smoothstep(1.0, 0.82, radius2));
}
)METAL";
}

} // namespace
} // namespace threebs

@interface ThreeBSMetalDelegate : NSObject <MTKViewDelegate>
- (instancetype)initWithView:(MTKView*)view snapshots:(threebs::SpscQueue<threebs::RenderSnapshot, 64>*)snapshots;
- (void)setVisual:(const threebs::VisualSettings&)settings;
- (BOOL)isReady;
@end

@implementation ThreeBSMetalDelegate {
    id<MTLDevice> _device;
    id<MTLCommandQueue> _queue;
    id<MTLRenderPipelineState> _backgroundPipeline;
    id<MTLRenderPipelineState> _trailPipeline;
    id<MTLRenderPipelineState> _planetPipeline;
    threebs::SpscQueue<threebs::RenderSnapshot, 64>* _snapshots;
    threebs::VisualSettings _settings;
    threebs::RenderSnapshot _latest;
    std::array<std::vector<threebs::GpuVertex>, threebs::bodyCount> _trails;
    CFTimeInterval _startTime;
    BOOL _ready;
}

- (instancetype)initWithView:(MTKView*)view snapshots:(threebs::SpscQueue<threebs::RenderSnapshot, 64>*)snapshots {
    self = [super init];
    if (self == nil)
        return nil;
    _snapshots = snapshots;
    _device = [view.device retain];
    _queue = [_device newCommandQueue];
    _settings = threebs::VisualSettings{};
    _startTime = CACurrentMediaTime();
    _latest.bodies = {{{1.0, {-0.95, 0.2, 0.0}, {}}, {1.0, {0.95, -0.2, 0.0}, {}},
                       {1.0, {0.0, 0.0, 0.0}, {}}}};

    NSError* error = nil;
    id<MTLLibrary> library = [_device newLibraryWithSource:threebs::shaderSource() options:nil error:&error];
    if (library == nil) {
        NSLog(@"3bs Metal shader error: %@", error);
        std::fprintf(stderr, "3bs Metal shader error: %s\n", error.localizedDescription.UTF8String);
        return self;
    }

    auto makePipeline = ^id<MTLRenderPipelineState>(NSString* vertexName, NSString* fragmentName,
                                                     BOOL blending) {
        MTLRenderPipelineDescriptor* descriptor = [MTLRenderPipelineDescriptor new];
        id<MTLFunction> vertex = [library newFunctionWithName:vertexName];
        id<MTLFunction> fragment = [library newFunctionWithName:fragmentName];
        descriptor.vertexFunction = vertex;
        descriptor.fragmentFunction = fragment;
        descriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
        if (blending) {
            descriptor.colorAttachments[0].blendingEnabled = YES;
            descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
            descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
            descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        }
        NSError* pipelineError = nil;
        auto pipeline = [_device newRenderPipelineStateWithDescriptor:descriptor error:&pipelineError];
        [vertex release];
        [fragment release];
        [descriptor release];
        if (pipeline == nil)
            NSLog(@"3bs Metal pipeline error: %@", pipelineError);
        if (pipeline == nil)
            std::fprintf(stderr, "3bs Metal pipeline error: %s\n", pipelineError.localizedDescription.UTF8String);
        return pipeline;
    };

    _backgroundPipeline = makePipeline(@"backgroundVertex", @"backgroundFragment", NO);
    _trailPipeline = makePipeline(@"sceneVertex", @"trailFragment", YES);
    _planetPipeline = makePipeline(@"sceneVertex", @"planetFragment", YES);
    [library release];
    _ready = _queue != nil && _backgroundPipeline != nil && _trailPipeline != nil && _planetPipeline != nil;
    return self;
}

- (BOOL)isReady { return _ready; }

- (void)dealloc {
    [_planetPipeline release];
    [_trailPipeline release];
    [_backgroundPipeline release];
    [_queue release];
    [_device release];
    [super dealloc];
}

- (void)setVisual:(const threebs::VisualSettings&)settings { _settings = settings; }

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size { (void)view; (void)size; }

- (void)drawInMTKView:(MTKView*)view {
    if (!_ready || view.currentDrawable == nil || view.currentRenderPassDescriptor == nil)
        return;

    threebs::RenderSnapshot incoming;
    BOOL received = NO;
    while (_snapshots->pop(incoming)) {
        _latest = incoming;
        received = YES;
    }
    if (received) {
        for (std::size_t body = 0; body < threebs::bodyCount; ++body) {
            auto& trail = _trails[body];
            const auto& position = _latest.bodies[body].position;
            const auto colour = threebs::bodyColours[body];
            trail.push_back({static_cast<float>(position.x), static_cast<float>(position.y),
                             static_cast<float>(position.z), colour[0], colour[1], colour[2], 1.0F, 1.0F});
            if (trail.size() > threebs::trailCapacity)
                trail.erase(trail.begin(), trail.begin() + static_cast<std::ptrdiff_t>(trail.size() - threebs::trailCapacity));
        }
    }

    const auto elapsed = static_cast<float>(CACurrentMediaTime() - _startTime);
    threebs::Vec3 focus{};
    if (_settings.focusBody >= 0 && _settings.focusBody < static_cast<int>(threebs::bodyCount))
        focus = _latest.bodies[static_cast<std::size_t>(_settings.focusBody)].position;
    threebs::Uniforms uniforms{elapsed,
        static_cast<float>(view.drawableSize.width / std::max(1.0, view.drawableSize.height)),
        _settings.extrusion, _settings.bloom, _settings.starDensity, _settings.autoOrbit,
        static_cast<float>(focus.x), static_cast<float>(focus.y), static_cast<float>(focus.z)};

    id<MTLCommandBuffer> command = [_queue commandBuffer];
    id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:view.currentRenderPassDescriptor];
    [encoder setRenderPipelineState:_backgroundPipeline];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:1];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];

    [encoder setRenderPipelineState:_trailPipeline];
    [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
    const auto retain = static_cast<std::size_t>(std::clamp(_settings.trailLength, 0.05F, 1.0F)
                                                  * static_cast<float>(threebs::trailCapacity));
    for (std::size_t body = 0; body < threebs::bodyCount; ++body) {
        auto& trail = _trails[body];
        const auto start = trail.size() > retain ? trail.size() - retain : 0U;
        std::vector<threebs::GpuVertex> vertices;
        vertices.reserve(trail.size() - start);
        for (std::size_t index = start; index < trail.size(); ++index) {
            auto vertex = trail[index];
            const auto age = static_cast<float>(index - start + 1U) / static_cast<float>(trail.size() - start + 1U);
            vertex.a = age * age * 0.62F;
            vertex.z -= (1.0F - age) * _settings.extrusion * 7.0F;
            vertices.push_back(vertex);
        }
        if (vertices.size() > 1) {
            [encoder setVertexBytes:vertices.data() length:vertices.size() * sizeof(threebs::GpuVertex) atIndex:0];
            [encoder drawPrimitives:MTLPrimitiveTypeLineStrip vertexStart:0 vertexCount:vertices.size()];
        }
    }

    std::array<threebs::GpuVertex, threebs::bodyCount> planets{};
    for (std::size_t body = 0; body < threebs::bodyCount; ++body) {
        const auto& source = _latest.bodies[body];
        const auto colour = threebs::bodyColours[body];
        planets[body] = {static_cast<float>(source.position.x), static_cast<float>(source.position.y),
                         static_cast<float>(source.position.z), colour[0], colour[1], colour[2], 1.0F,
                         42.0F + static_cast<float>(std::cbrt(std::max(0.05, source.mass))) * 17.0F};
    }
    [encoder setRenderPipelineState:_planetPipeline];
    [encoder setVertexBytes:planets.data() length:sizeof(planets) atIndex:0];
    [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:1];
    [encoder drawPrimitives:MTLPrimitiveTypePoint vertexStart:0 vertexCount:threebs::bodyCount];
    [encoder endEncoding];
    [command presentDrawable:view.currentDrawable];
    [command commit];
}

@end

namespace threebs {

class MetalSceneComponent::Impl {
public:
    Impl(MetalSceneComponent& owner, SpscQueue<RenderSnapshot, 64>& snapshots) {
        auto device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            std::fprintf(stderr, "3bs Metal device unavailable\n");
            return;
        }
        view = [[MTKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 500) device:device];
        view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        view.clearColor = MTLClearColorMake(0.001, 0.002, 0.008, 1.0);
        view.preferredFramesPerSecond = 60;
        view.paused = NO;
        view.enableSetNeedsDisplay = NO;
        delegate = [[ThreeBSMetalDelegate alloc] initWithView:view snapshots:&snapshots];
        view.delegate = delegate;
        owner.setView(static_cast<void*>(view));
    }

    ~Impl() {
        view.delegate = nil;
        [delegate release];
        [view release];
    }

    MTKView* view{};
    ThreeBSMetalDelegate* delegate{};
};

MetalSceneComponent::MetalSceneComponent(SpscQueue<RenderSnapshot, 64>& snapshots)
    : impl_(std::make_unique<Impl>(*this, snapshots)) {}

MetalSceneComponent::~MetalSceneComponent() {
    setView(nullptr);
}

void MetalSceneComponent::setVisualSettings(const VisualSettings& settings) noexcept {
    if (impl_ != nullptr && impl_->delegate != nil)
        [impl_->delegate setVisual:settings];
}

bool MetalSceneComponent::rendererAvailable() const noexcept {
    return impl_ != nullptr && impl_->delegate != nil && [impl_->delegate isReady];
}

} // namespace threebs
