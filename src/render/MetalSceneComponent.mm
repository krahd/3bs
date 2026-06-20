// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Tomas Laurenzo

#include "render/MetalSceneComponent.h"

#include "core/Random.h"
#include "render/CameraController.h"
#include "render/TrailHistory.h"

#include <BinaryData.h>

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>
#import <simd/simd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace threebs {
namespace {

constexpr std::size_t trailCapacity = 4096;
constexpr std::size_t generatedStarCount = 12000;

void writeMetalDiagnostic(const char* stage, NSString* message) noexcept {
    const auto* text = message != nil ? message.UTF8String : "unknown Metal error";
    std::fprintf(stderr, "3bs Metal %s: %s\n", stage, text);
    if (auto* file = std::fopen("/tmp/3bs-metal-errors.log", "a")) {
        std::fprintf(file, "3bs Metal %s: %s\n", stage, text);
        std::fclose(file);
    }
}

struct alignas(16) SceneUniforms {
    simd_float4 cameraPosition{};
    simd_float4 cameraRight{};
    simd_float4 cameraUp{};
    simd_float4 cameraForward{};
    simd_float4 viewportTime{};
    simd_float4 renderSettings{};
};

struct alignas(16) SphereVertex { simd_float4 position{}; };

struct alignas(16) PlanetInstance {
    simd_float4 centerRadius{};
    simd_float4 colour0{};
    simd_float4 colour1{};
    simd_float4 colour2{};
    simd_float4 colour3{};
    simd_float4 style0{};
    simd_float4 style1{};
};

struct alignas(16) StarInstance {
    simd_float4 directionMagnitude{};
    simd_float4 colourKind{};
};

struct alignas(16) TrailVertex {
    simd_float4 previousSide{};
    simd_float4 currentAge{};
    simd_float4 nextWidth{};
    simd_float4 colour{};
};

struct alignas(16) PostUniforms {
    simd_float2 texel{};
    simd_float2 direction{};
    float bloom{};
    simd_float3 padding{};
};

simd_float4 vector(Vec3 value, float w = 0.0F) noexcept {
    return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z), w};
}

simd_float4 colour(Colour3 value, float alpha = 1.0F) noexcept {
    return {value.r, value.g, value.b, alpha};
}

std::vector<SphereVertex> makeIcosphere(std::vector<std::uint32_t>& indices, int subdivisions) {
    constexpr float golden = 1.6180339887498948482F;
    std::vector<simd_float3> positions{
        simd_make_float3(-1.0F, golden, 0.0F), simd_make_float3(1.0F, golden, 0.0F),
        simd_make_float3(-1.0F, -golden, 0.0F), simd_make_float3(1.0F, -golden, 0.0F),
        simd_make_float3(0.0F, -1.0F, golden), simd_make_float3(0.0F, 1.0F, golden),
        simd_make_float3(0.0F, -1.0F, -golden), simd_make_float3(0.0F, 1.0F, -golden),
        simd_make_float3(golden, 0.0F, -1.0F), simd_make_float3(golden, 0.0F, 1.0F),
        simd_make_float3(-golden, 0.0F, -1.0F), simd_make_float3(-golden, 0.0F, 1.0F)};
    for (auto& position : positions)
        position = simd_normalize(position);
    indices = {
        0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
        1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9,
        4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1};

    for (int level = 0; level < subdivisions; ++level) {
        std::unordered_map<std::uint64_t, std::uint32_t> midpointCache;
        std::vector<std::uint32_t> refined;
        refined.reserve(indices.size() * 4U);
        auto midpoint = [&](std::uint32_t first, std::uint32_t second) {
            const auto low = std::min(first, second);
            const auto high = std::max(first, second);
            const auto key = (static_cast<std::uint64_t>(low) << 32U) | high;
            if (const auto found = midpointCache.find(key); found != midpointCache.end())
                return found->second;
            const auto index = static_cast<std::uint32_t>(positions.size());
            positions.push_back(simd_normalize(positions[first] + positions[second]));
            midpointCache.emplace(key, index);
            return index;
        };
        for (std::size_t index = 0; index < indices.size(); index += 3U) {
            const auto a = indices[index];
            const auto b = indices[index + 1U];
            const auto c = indices[index + 2U];
            const auto ab = midpoint(a, b);
            const auto bc = midpoint(b, c);
            const auto ca = midpoint(c, a);
            refined.insert(refined.end(), {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca});
        }
        indices = std::move(refined);
    }

    std::vector<SphereVertex> result;
    result.reserve(positions.size());
    for (const auto position : positions)
        result.push_back({{position.x, position.y, position.z, 1.0F}});
    return result;
}

simd_float3 starColour(float colourIndex) noexcept {
    const auto amount = std::clamp((colourIndex + 0.4F) / 2.4F, 0.0F, 1.0F);
    const simd_float3 blue{0.58F, 0.72F, 1.0F};
    const simd_float3 white{1.0F, 0.96F, 0.88F};
    const simd_float3 amber{1.0F, 0.55F, 0.25F};
    return amount < 0.45F
        ? blue + (white - blue) * (amount / 0.45F)
        : white + (amber - white) * ((amount - 0.45F) / 0.55F);
}

std::vector<StarInstance> makeStars() {
    std::vector<StarInstance> stars;
    stars.reserve(generatedStarCount + 10000U);
    const auto appendCatalogue = [&stars](const char* data, int size) {
        const auto before = stars.size();
        const std::string catalogue(data, static_cast<std::size_t>(size));
        std::size_t lineStart{};
        while (lineStart < catalogue.size()) {
            const auto lineEnd = catalogue.find('\n', lineStart);
            const auto line = catalogue.substr(lineStart, lineEnd - lineStart);
            lineStart = lineEnd == std::string::npos ? catalogue.size() : lineEnd + 1U;
            if (line.empty() || line.front() == '#')
                continue;
            double rightAscension{}, declination{}, magnitude{}, colourIndex{};
            if (std::sscanf(line.c_str(), "%lf,%lf,%lf,%lf", &rightAscension, &declination,
                            &magnitude, &colourIndex) != 4)
                continue;
            constexpr double radians = 3.14159265358979323846 / 180.0;
            const auto ra = rightAscension * 15.0 * radians;
            const auto dec = declination * radians;
            const simd_float3 direction{
                static_cast<float>(std::cos(dec) * std::cos(ra)),
                static_cast<float>(std::sin(dec)),
                static_cast<float>(std::cos(dec) * std::sin(ra))};
            const auto rgb = starColour(static_cast<float>(colourIndex));
            stars.push_back({{direction.x, direction.y, direction.z, static_cast<float>(magnitude)},
                             {rgb.x, rgb.y, rgb.z, 1.0F}});
        }
        return stars.size() - before;
    };
    if (appendCatalogue(ThreeBSAssets::hygv41mag65_csv, ThreeBSAssets::hygv41mag65_csvSize) == 0U)
        appendCatalogue(ThreeBSAssets::brightstars_csv, ThreeBSAssets::brightstars_csvSize);

    Pcg32 random(0x5354415253ULL, 0x334253ULL);
    for (std::size_t index = 0; index < generatedStarCount; ++index) {
        const auto z = random.symmetric();
        const auto angle = random.nextUnit() * 6.2831853071795864769;
        const auto radius = std::sqrt(std::max(0.0, 1.0 - z * z));
        const simd_float3 direction{static_cast<float>(radius * std::cos(angle)),
                                    static_cast<float>(z),
                                    static_cast<float>(radius * std::sin(angle))};
        const auto magnitude = static_cast<float>(6.6 + random.nextUnit() * 3.4);
        const auto rgb = starColour(static_cast<float>(-0.2 + random.nextUnit() * 1.8));
        stars.push_back({{direction.x, direction.y, direction.z, magnitude},
                         {rgb.x, rgb.y, rgb.z, 0.0F}});
    }
    return stars;
}

} // namespace
} // namespace threebs

@class ThreeBSMetalDelegate;

@interface ThreeBSMetalView : MTKView {
    NSPoint _mouseDownPoint;
    NSPoint _lastMousePoint;
    BOOL _dragged;
}
@property(nonatomic, assign) ThreeBSMetalDelegate* sceneDelegate;
@end

@interface ThreeBSMetalDelegate : NSObject <MTKViewDelegate>
- (instancetype)initWithView:(ThreeBSMetalView*)view
                   snapshots:(threebs::SpscQueue<threebs::RenderSnapshot, 64>*)snapshots
                       owner:(threebs::MetalSceneComponent*)owner;
- (void)setPresentation:(const threebs::PresentationState&)state;
- (threebs::PresentationState)presentation;
- (void)beginCameraInteraction;
- (void)orbitByX:(double)x y:(double)y width:(double)width;
- (void)zoomBy:(double)delta;
- (void)selectAtX:(double)x y:(double)y width:(double)width height:(double)height;
- (void)finishCameraInteraction;
- (BOOL)isReady;
@end

@implementation ThreeBSMetalView
@synthesize sceneDelegate = _sceneDelegate;

- (BOOL)acceptsFirstResponder { return YES; }

- (NSPoint)cameraPoint:(NSEvent*)event {
    const auto local = [self convertPoint:event.locationInWindow fromView:nil];
    return NSMakePoint(local.x, self.bounds.size.height - local.y);
}

- (void)mouseDown:(NSEvent*)event {
    [self.window makeFirstResponder:self];
    _mouseDownPoint = [self cameraPoint:event];
    _lastMousePoint = _mouseDownPoint;
    _dragged = NO;
    [_sceneDelegate beginCameraInteraction];
}

- (void)mouseDragged:(NSEvent*)event {
    const auto point = [self cameraPoint:event];
    const auto totalX = point.x - _mouseDownPoint.x;
    const auto totalY = point.y - _mouseDownPoint.y;
    if (!threebs::CameraController::isClick(totalX, totalY))
        _dragged = YES;
    if (_dragged)
        [_sceneDelegate orbitByX:point.x - _lastMousePoint.x
                               y:point.y - _lastMousePoint.y
                           width:self.bounds.size.width];
    _lastMousePoint = point;
}

- (void)mouseUp:(NSEvent*)event {
    const auto point = [self cameraPoint:event];
    if (!_dragged)
        [_sceneDelegate selectAtX:point.x y:point.y width:self.bounds.size.width height:self.bounds.size.height];
    [_sceneDelegate finishCameraInteraction];
}

- (void)scrollWheel:(NSEvent*)event {
    const auto scale = event.hasPreciseScrollingDeltas ? 0.16 : 1.0;
    [_sceneDelegate zoomBy:event.scrollingDeltaY * scale];
    if (!event.hasPreciseScrollingDeltas || event.phase == NSEventPhaseEnded
        || event.momentumPhase == NSEventPhaseEnded)
        [_sceneDelegate finishCameraInteraction];
}
@end

@implementation ThreeBSMetalDelegate {
    id<MTLDevice> _device;
    id<MTLCommandQueue> _queue;
    id<MTLRenderPipelineState> _backgroundPipeline;
    id<MTLRenderPipelineState> _starPipeline;
    id<MTLRenderPipelineState> _trailPipeline;
    id<MTLRenderPipelineState> _planetPipeline;
    id<MTLRenderPipelineState> _cloudPipeline;
    id<MTLRenderPipelineState> _atmospherePipeline;
    id<MTLRenderPipelineState> _bloomExtractPipeline;
    id<MTLRenderPipelineState> _blurPipeline;
    id<MTLRenderPipelineState> _compositePipeline;
    id<MTLDepthStencilState> _depthWrite;
    id<MTLDepthStencilState> _depthRead;
    id<MTLDepthStencilState> _depthNone;
    id<MTLBuffer> _sphereVertices;
    id<MTLBuffer> _sphereIndices;
    id<MTLBuffer> _planetInstances;
    id<MTLBuffer> _cloudInstances;
    id<MTLBuffer> _atmosphereInstances;
    id<MTLBuffer> _starInstances;
    id<MTLBuffer> _trailVertices;
    id<MTLTexture> _sceneTexture;
    id<MTLTexture> _depthTexture;
    id<MTLTexture> _bloomA;
    id<MTLTexture> _bloomB;
    threebs::SpscQueue<threebs::RenderSnapshot, 64>* _snapshots;
    threebs::MetalSceneComponent* _owner;
    threebs::PresentationState _presentation;
    threebs::CameraState _hostCamera;
    threebs::CameraController _camera;
    threebs::RenderSnapshot _latest;
    std::array<threebs::TrailHistory<threebs::trailCapacity>, threebs::bodyCount> _trails;
    std::array<threebs::PlanetVisualStyle, threebs::bodyCount> _styles;
    std::array<threebs::TrailVertex, threebs::trailCapacity * threebs::bodyCount * 2U> _trailStaging;
    std::size_t _sphereIndexCount;
    std::size_t _starCount;
    std::uint64_t _lastSequence;
    NSUInteger _targetWidth;
    NSUInteger _targetHeight;
    CFTimeInterval _startTime;
    BOOL _ready;
}

- (instancetype)initWithView:(ThreeBSMetalView*)view
                   snapshots:(threebs::SpscQueue<threebs::RenderSnapshot, 64>*)snapshots
                       owner:(threebs::MetalSceneComponent*)owner {
    self = [super init];
    if (self == nil)
        return nil;
    _snapshots = snapshots;
    _owner = owner;
    _device = [view.device retain];
    _queue = [_device newCommandQueue];
    _presentation = threebs::PresentationState{};
    _hostCamera = _presentation.camera;
    _styles = threebs::makePlanetVisualStyles(_presentation.visual.palette, _presentation.visualSeed);
    _startTime = CACurrentMediaTime();
    _latest.bodies = {{{1.0, {-0.95, 0.2, 0.0}, {}}, {1.0, {0.95, -0.2, 0.0}, {}},
                       {1.0, {0.0, 0.0, 0.0}, {}}}};
    _camera.setState(_presentation.camera, _latest.bodies, 0.0);

    NSError* error = nil;
    NSString* source = [[NSString alloc] initWithBytes:ThreeBSAssets::SceneShaders_metal
                                                length:ThreeBSAssets::SceneShaders_metalSize
                                              encoding:NSUTF8StringEncoding];
    id<MTLLibrary> library = [_device newLibraryWithSource:source options:nil error:&error];
    [source release];
    if (library == nil) {
        NSLog(@"3bs Metal shader error: %@", error);
        threebs::writeMetalDiagnostic("shader", error.localizedDescription);
        return self;
    }

    auto makePipeline = ^id<MTLRenderPipelineState>(NSString* vertexName, NSString* fragmentName,
                                                     MTLPixelFormat format, BOOL depth, BOOL blend) {
        MTLRenderPipelineDescriptor* descriptor = [MTLRenderPipelineDescriptor new];
        id<MTLFunction> vertex = [library newFunctionWithName:vertexName];
        id<MTLFunction> fragment = [library newFunctionWithName:fragmentName];
        descriptor.vertexFunction = vertex;
        descriptor.fragmentFunction = fragment;
        descriptor.colorAttachments[0].pixelFormat = format;
        descriptor.depthAttachmentPixelFormat = depth ? MTLPixelFormatDepth32Float : MTLPixelFormatInvalid;
        if (blend) {
            descriptor.colorAttachments[0].blendingEnabled = YES;
            descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
            descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
            descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        }
        NSError* pipelineError = nil;
        auto pipeline = [_device newRenderPipelineStateWithDescriptor:descriptor error:&pipelineError];
        if (pipeline == nil) {
            NSLog(@"3bs Metal pipeline error (%@/%@): %@", vertexName, fragmentName, pipelineError);
            threebs::writeMetalDiagnostic("pipeline", pipelineError.localizedDescription);
        }
        [vertex release];
        [fragment release];
        [descriptor release];
        return pipeline;
    };

    constexpr auto hdr = MTLPixelFormatRGBA16Float;
    _backgroundPipeline = makePipeline(@"fullscreenVertex", @"backgroundFragment", hdr, YES, NO);
    _starPipeline = makePipeline(@"starVertex", @"starFragment", hdr, YES, YES);
    _trailPipeline = makePipeline(@"trailVertex", @"trailFragment", hdr, YES, YES);
    _planetPipeline = makePipeline(@"planetVertex", @"planetFragment", hdr, YES, NO);
    _cloudPipeline = makePipeline(@"planetVertex", @"cloudFragment", hdr, YES, YES);
    _atmospherePipeline = makePipeline(@"planetVertex", @"atmosphereFragment", hdr, YES, YES);
    _bloomExtractPipeline = makePipeline(@"fullscreenVertex", @"bloomExtractFragment", hdr, NO, NO);
    _blurPipeline = makePipeline(@"fullscreenVertex", @"blurFragment", hdr, NO, NO);
    _compositePipeline = makePipeline(@"fullscreenVertex", @"compositeFragment", view.colorPixelFormat, NO, NO);
    [library release];

    MTLDepthStencilDescriptor* depth = [MTLDepthStencilDescriptor new];
    depth.depthCompareFunction = MTLCompareFunctionLess;
    depth.depthWriteEnabled = YES;
    _depthWrite = [_device newDepthStencilStateWithDescriptor:depth];
    depth.depthCompareFunction = MTLCompareFunctionLessEqual;
    depth.depthWriteEnabled = NO;
    _depthRead = [_device newDepthStencilStateWithDescriptor:depth];
    depth.depthCompareFunction = MTLCompareFunctionAlways;
    _depthNone = [_device newDepthStencilStateWithDescriptor:depth];
    [depth release];

    std::vector<std::uint32_t> indices;
    const auto sphere = threebs::makeIcosphere(indices, 4);
    _sphereIndexCount = indices.size();
    _sphereVertices = [_device newBufferWithBytes:sphere.data()
                                             length:sphere.size() * sizeof(threebs::SphereVertex)
                                            options:MTLResourceStorageModeShared];
    _sphereIndices = [_device newBufferWithBytes:indices.data()
                                            length:indices.size() * sizeof(std::uint32_t)
                                           options:MTLResourceStorageModeShared];
    _planetInstances = [_device newBufferWithLength:sizeof(threebs::PlanetInstance) * threebs::bodyCount
                                            options:MTLResourceStorageModeShared];
    _cloudInstances = [_device newBufferWithLength:sizeof(threebs::PlanetInstance) * threebs::bodyCount
                                           options:MTLResourceStorageModeShared];
    _atmosphereInstances = [_device newBufferWithLength:sizeof(threebs::PlanetInstance) * threebs::bodyCount
                                                options:MTLResourceStorageModeShared];
    const auto stars = threebs::makeStars();
    _starCount = stars.size();
    _starInstances = [_device newBufferWithBytes:stars.data()
                                           length:stars.size() * sizeof(threebs::StarInstance)
                                          options:MTLResourceStorageModeShared];
    _trailVertices = [_device newBufferWithLength:sizeof(_trailStaging)
                                           options:MTLResourceStorageModeShared];

    _ready = _queue != nil && _backgroundPipeline != nil && _starPipeline != nil
        && _trailPipeline != nil && _planetPipeline != nil && _cloudPipeline != nil
        && _atmospherePipeline != nil && _bloomExtractPipeline != nil
        && _blurPipeline != nil && _compositePipeline != nil && _sphereVertices != nil
        && _sphereIndices != nil && _starInstances != nil && _trailVertices != nil;
    return self;
}

- (void)dealloc {
    [_bloomB release];
    [_bloomA release];
    [_depthTexture release];
    [_sceneTexture release];
    [_trailVertices release];
    [_starInstances release];
    [_atmosphereInstances release];
    [_cloudInstances release];
    [_planetInstances release];
    [_sphereIndices release];
    [_sphereVertices release];
    [_depthNone release];
    [_depthRead release];
    [_depthWrite release];
    [_compositePipeline release];
    [_blurPipeline release];
    [_bloomExtractPipeline release];
    [_atmospherePipeline release];
    [_cloudPipeline release];
    [_planetPipeline release];
    [_trailPipeline release];
    [_starPipeline release];
    [_backgroundPipeline release];
    [_queue release];
    [_device release];
    [super dealloc];
}

- (BOOL)isReady { return _ready; }

- (void)setPresentation:(const threebs::PresentationState&)state {
    const auto now = CACurrentMediaTime() - _startTime;
    if (!(state.camera == _hostCamera)) {
        _hostCamera = state.camera;
        _camera.setState(state.camera, _latest.bodies, now);
    }
    if (state.visual.palette != _presentation.visual.palette || state.visualSeed != _presentation.visualSeed)
        _styles = threebs::makePlanetVisualStyles(state.visual.palette, state.visualSeed);
    _presentation = state;
    _presentation.camera = _camera.state();
}

- (threebs::PresentationState)presentation {
    auto state = _presentation;
    state.camera = _camera.state();
    return state;
}

- (void)beginCameraInteraction {
    _camera.beginInteraction(CACurrentMediaTime() - _startTime);
}

- (void)orbitByX:(double)x y:(double)y width:(double)width {
    _camera.orbit(x, y, width, CACurrentMediaTime() - _startTime);
}

- (void)zoomBy:(double)delta {
    _camera.zoom(delta, CACurrentMediaTime() - _startTime);
}

- (void)selectAtX:(double)x y:(double)y width:(double)width height:(double)height {
    const auto selected = _camera.hitTest(x, y, width, height, _latest.bodies);
    _camera.selectFocus(selected, _latest.bodies, CACurrentMediaTime() - _startTime);
}

- (void)finishCameraInteraction {
    _presentation.camera = _camera.state();
    _hostCamera = _presentation.camera;
    if (_owner != nullptr && _owner->onCameraInteractionComplete)
        _owner->onCameraInteractionComplete(_presentation.camera);
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {
    (void)view;
    (void)size;
    _targetWidth = 0;
    _targetHeight = 0;
}

- (void)ensureTargetsForView:(MTKView*)view {
    const auto width = std::max<NSUInteger>(1U, static_cast<NSUInteger>(view.drawableSize.width));
    const auto height = std::max<NSUInteger>(1U, static_cast<NSUInteger>(view.drawableSize.height));
    if (_targetWidth == width && _targetHeight == height && _sceneTexture != nil)
        return;
    _targetWidth = width;
    _targetHeight = height;
    [_sceneTexture release];
    [_depthTexture release];
    [_bloomA release];
    [_bloomB release];
    MTLTextureDescriptor* scene = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                                                                    width:width height:height mipmapped:NO];
    scene.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    scene.storageMode = MTLStorageModePrivate;
    _sceneTexture = [_device newTextureWithDescriptor:scene];
    MTLTextureDescriptor* depth = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                    width:width height:height mipmapped:NO];
    depth.usage = MTLTextureUsageRenderTarget;
    depth.storageMode = MTLStorageModePrivate;
    _depthTexture = [_device newTextureWithDescriptor:depth];
    const auto bloomWidth = std::max<NSUInteger>(1U, width / 2U);
    const auto bloomHeight = std::max<NSUInteger>(1U, height / 2U);
    MTLTextureDescriptor* bloom = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                                                                    width:bloomWidth height:bloomHeight mipmapped:NO];
    bloom.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    bloom.storageMode = MTLStorageModePrivate;
    _bloomA = [_device newTextureWithDescriptor:bloom];
    _bloomB = [_device newTextureWithDescriptor:bloom];
}

- (void)drawInMTKView:(MTKView*)view {
    if (!_ready || view.currentDrawable == nil)
        return;
    [self ensureTargetsForView:view];
    if (_sceneTexture == nil || _depthTexture == nil || _bloomA == nil || _bloomB == nil)
        return;

    const auto now = CACurrentMediaTime() - _startTime;
    threebs::RenderSnapshot incoming;
    BOOL received = NO;
    while (_snapshots->pop(incoming)) {
        _latest = incoming;
        received = YES;
    }
    if (received && _latest.sequence != _lastSequence) {
        _lastSequence = _latest.sequence;
        for (std::size_t body = 0; body < threebs::bodyCount; ++body)
            _trails[body].append(_latest.bodies[body].position, now, _latest.trajectoryRevision);
    }
    for (auto& trail : _trails)
        trail.prune(now, std::clamp(static_cast<double>(_presentation.visual.trailSeconds), 5.0, 60.0));

    _camera.update(now, _latest.bodies);
    const auto basis = _camera.basis();
    threebs::SceneUniforms uniforms;
    uniforms.cameraPosition = threebs::vector(basis.position, 1.0F);
    uniforms.cameraRight = threebs::vector(basis.right);
    uniforms.cameraUp = threebs::vector(basis.up);
    uniforms.cameraForward = threebs::vector(basis.forward);
    uniforms.viewportTime = {static_cast<float>(_targetWidth), static_cast<float>(_targetHeight),
                             static_cast<float>(now), 0.0F};
    uniforms.renderSettings = {_presentation.visual.bloom, _presentation.visual.starDensity,
                               _presentation.visual.extrusion, _presentation.visual.trailWidth};

    auto* planets = static_cast<threebs::PlanetInstance*>(_planetInstances.contents);
    auto* clouds = static_cast<threebs::PlanetInstance*>(_cloudInstances.contents);
    auto* atmospheres = static_cast<threebs::PlanetInstance*>(_atmosphereInstances.contents);
    for (std::size_t body = 0; body < threebs::bodyCount; ++body) {
        const auto& source = _latest.bodies[body];
        const auto& style = _styles[body];
        threebs::PlanetInstance instance;
        instance.centerRadius = threebs::vector(source.position,
            static_cast<float>(threebs::planetRadius(source.mass)));
        instance.colour0 = threebs::colour(style.colours[0]);
        instance.colour1 = threebs::colour(style.colours[1]);
        instance.colour2 = threebs::colour(style.colours[2]);
        instance.colour3 = threebs::colour(style.colours[3]);
        instance.style0 = {static_cast<float>(style.seed), style.terrainScale, style.oceanLevel, 0.0F};
        instance.style1 = {style.rotationRate, style.cloudRate, style.axialTilt, 0.0F};
        planets[body] = instance;
        clouds[body] = instance;
        clouds[body].centerRadius.w *= 1.025F;
        atmospheres[body] = instance;
        atmospheres[body].centerRadius.w *= 1.085F;
    }

    std::array<NSRange, threebs::bodyCount> trailRanges{};
    std::size_t trailVertexCount{};
    const auto trailSeconds = std::max(5.0, static_cast<double>(_presentation.visual.trailSeconds));
    for (std::size_t body = 0; body < threebs::bodyCount; ++body) {
        const auto& trail = _trails[body];
        trailRanges[body].location = trailVertexCount;
        for (std::size_t index = 0; index < trail.size(); ++index) {
            const auto previous = trail[index == 0U ? index : index - 1U].position;
            const auto current = trail[index].position;
            const auto next = trail[index + 1U < trail.size() ? index + 1U : index].position;
            const auto age = static_cast<float>(std::clamp((trail[index].time - (now - trailSeconds))
                                                           / trailSeconds, 0.0, 1.0));
            const auto trailColour = _styles[body].colours[2];
            for (const auto side : {-1.0F, 1.0F}) {
                auto& vertex = _trailStaging[trailVertexCount++];
                vertex.previousSide = threebs::vector(previous, side);
                vertex.currentAge = threebs::vector(current, age);
                vertex.nextWidth = threebs::vector(next, std::min(_presentation.visual.trailWidth, 0.95F));
                vertex.colour = threebs::colour(trailColour, 0.72F);
            }
        }
        trailRanges[body].length = trailVertexCount - trailRanges[body].location;
    }
    if (trailVertexCount > 0U)
        std::memcpy(_trailVertices.contents, _trailStaging.data(),
                    trailVertexCount * sizeof(threebs::TrailVertex));

    id<MTLCommandBuffer> command = [_queue commandBuffer];
    MTLRenderPassDescriptor* scenePass = [MTLRenderPassDescriptor renderPassDescriptor];
    scenePass.colorAttachments[0].texture = _sceneTexture;
    scenePass.colorAttachments[0].loadAction = MTLLoadActionClear;
    scenePass.colorAttachments[0].storeAction = MTLStoreActionStore;
    scenePass.colorAttachments[0].clearColor = MTLClearColorMake(0.0005, 0.001, 0.004, 1.0);
    scenePass.depthAttachment.texture = _depthTexture;
    scenePass.depthAttachment.loadAction = MTLLoadActionClear;
    scenePass.depthAttachment.storeAction = MTLStoreActionStore;
    scenePass.depthAttachment.clearDepth = 1.0;
    id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:scenePass];
    [encoder setCullMode:MTLCullModeNone];
    [encoder setDepthStencilState:_depthNone];
    [encoder setRenderPipelineState:_backgroundPipeline];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];

    [encoder setDepthStencilState:_depthRead];
    [encoder setRenderPipelineState:_trailPipeline];
    [encoder setVertexBuffer:_trailVertices offset:0 atIndex:0];
    [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
    for (const auto range : trailRanges) {
        if (range.length >= 4U)
            [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:range.location vertexCount:range.length];
    }

    [encoder setCullMode:MTLCullModeBack];
    [encoder setDepthStencilState:_depthWrite];
    [encoder setRenderPipelineState:_planetPipeline];
    [encoder setVertexBuffer:_sphereVertices offset:0 atIndex:0];
    [encoder setVertexBuffer:_planetInstances offset:0 atIndex:1];
    [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:2];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:_sphereIndexCount
                         indexType:MTLIndexTypeUInt32 indexBuffer:_sphereIndices indexBufferOffset:0
                     instanceCount:threebs::bodyCount];

    [encoder setDepthStencilState:_depthRead];
    [encoder setRenderPipelineState:_cloudPipeline];
    [encoder setVertexBuffer:_cloudInstances offset:0 atIndex:1];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:_sphereIndexCount
                         indexType:MTLIndexTypeUInt32 indexBuffer:_sphereIndices indexBufferOffset:0
                     instanceCount:threebs::bodyCount];
    [encoder setCullMode:MTLCullModeFront];
    [encoder setRenderPipelineState:_atmospherePipeline];
    [encoder setVertexBuffer:_atmosphereInstances offset:0 atIndex:1];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:_sphereIndexCount
                         indexType:MTLIndexTypeUInt32 indexBuffer:_sphereIndices indexBufferOffset:0
                     instanceCount:threebs::bodyCount];
    [encoder endEncoding];

    MTLRenderPassDescriptor* bloomPass = [MTLRenderPassDescriptor renderPassDescriptor];
    bloomPass.colorAttachments[0].texture = _bloomA;
    bloomPass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    bloomPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    encoder = [command renderCommandEncoderWithDescriptor:bloomPass];
    [encoder setRenderPipelineState:_bloomExtractPipeline];
    [encoder setFragmentTexture:_sceneTexture atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];

    // Stars are composited after bloom extraction so bright catalogue entries remain point-like.
    MTLRenderPassDescriptor* starPass = [MTLRenderPassDescriptor renderPassDescriptor];
    starPass.colorAttachments[0].texture = _sceneTexture;
    starPass.colorAttachments[0].loadAction = MTLLoadActionLoad;
    starPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    starPass.depthAttachment.texture = _depthTexture;
    starPass.depthAttachment.loadAction = MTLLoadActionLoad;
    starPass.depthAttachment.storeAction = MTLStoreActionDontCare;
    encoder = [command renderCommandEncoderWithDescriptor:starPass];
    [encoder setDepthStencilState:_depthRead];
    [encoder setRenderPipelineState:_starPipeline];
    [encoder setVertexBuffer:_starInstances offset:0 atIndex:0];
    [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:_starCount];
    [encoder endEncoding];

    threebs::PostUniforms post;
    post.texel = {1.0F / static_cast<float>(_bloomA.width), 1.0F / static_cast<float>(_bloomA.height)};
    post.direction = {1.0F, 0.0F};
    post.bloom = _presentation.visual.bloom;
    MTLRenderPassDescriptor* blurPass = [MTLRenderPassDescriptor renderPassDescriptor];
    blurPass.colorAttachments[0].texture = _bloomB;
    blurPass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    blurPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    encoder = [command renderCommandEncoderWithDescriptor:blurPass];
    [encoder setRenderPipelineState:_blurPipeline];
    [encoder setFragmentTexture:_bloomA atIndex:0];
    [encoder setFragmentBytes:&post length:sizeof(post) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];
    blurPass.colorAttachments[0].texture = _bloomA;
    encoder = [command renderCommandEncoderWithDescriptor:blurPass];
    post.direction = {0.0F, 1.0F};
    [encoder setRenderPipelineState:_blurPipeline];
    [encoder setFragmentTexture:_bloomB atIndex:0];
    [encoder setFragmentBytes:&post length:sizeof(post) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];

    MTLRenderPassDescriptor* compositePass = view.currentRenderPassDescriptor;
    if (compositePass == nil)
        return;
    compositePass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    encoder = [command renderCommandEncoderWithDescriptor:compositePass];
    [encoder setRenderPipelineState:_compositePipeline];
    [encoder setFragmentTexture:_sceneTexture atIndex:0];
    [encoder setFragmentTexture:_bloomA atIndex:1];
    [encoder setFragmentBytes:&post length:sizeof(post) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
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
        view = [[ThreeBSMetalView alloc] initWithFrame:NSMakeRect(0, 0, 800, 500) device:device];
        view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        view.clearColor = MTLClearColorMake(0.001, 0.002, 0.008, 1.0);
        view.preferredFramesPerSecond = 60;
        view.paused = NO;
        view.enableSetNeedsDisplay = NO;
        delegate = [[ThreeBSMetalDelegate alloc] initWithView:view snapshots:&snapshots owner:&owner];
        view.sceneDelegate = delegate;
        view.delegate = delegate;
        owner.setView(static_cast<void*>(view));
    }

    ~Impl() {
        view.delegate = nil;
        view.sceneDelegate = nil;
        [delegate release];
        [view release];
    }

    ThreeBSMetalView* view{};
    ThreeBSMetalDelegate* delegate{};
};

MetalSceneComponent::MetalSceneComponent(SpscQueue<RenderSnapshot, 64>& snapshots)
    : impl_(std::make_unique<Impl>(*this, snapshots)) {}

MetalSceneComponent::~MetalSceneComponent() {
    setView(nullptr);
}

void MetalSceneComponent::setPresentationState(const PresentationState& state) noexcept {
    if (impl_ != nullptr && impl_->delegate != nil)
        [impl_->delegate setPresentation:state];
}

PresentationState MetalSceneComponent::presentationState() const noexcept {
    if (impl_ != nullptr && impl_->delegate != nil)
        return [impl_->delegate presentation];
    return {};
}

bool MetalSceneComponent::rendererAvailable() const noexcept {
    return impl_ != nullptr && impl_->delegate != nil && [impl_->delegate isReady];
}

} // namespace threebs
