//
//  Renderer.hpp
//  LearningMetal
//
//  Created by eternal on 2024/4/14.
//

#ifndef Renderer_hpp
#define Renderer_hpp

#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>
#include <simd/simd.h>

static constexpr size_t kNumInstances = 32;
static constexpr size_t kMaxFramesInFlight = 3;

class Renderer {
public:
    Renderer(MTL::Device * pDevice);
    ~Renderer();
    void buildShaders();
    void buildBuffers();
    void draw(MTK::View *pView);

private:
    MTL::Device* _pDevice;
    MTL::CommandQueue* _pCommandQueue;
    MTL::Library* _pShaderLibrary;
    MTL::RenderPipelineState *_pPSO;
    MTL::Buffer* _pArgBuffer;
    MTL::Buffer* _pVertexDataBuffer;
    MTL::Buffer* _pIndexBuffer;
    MTL::Buffer* _pInstanceDataBuffer[kMaxFramesInFlight];
    float _angle;
    int _frame;
    dispatch_semaphore_t _semaphore;
    static const int kMaxFramesInFlight;
};

struct FrameData {
    float angle;
};

namespace shader_types {
    struct InstanceData {
        simd::float4x4 instanceTransform;
        simd::float4 instanceColor;
    };
}

#endif /* Renderer_hpp */
