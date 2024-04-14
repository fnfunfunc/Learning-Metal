//
//  Renderer.cpp
//  LearningMetal
//
//  Created by eternal on 2024/4/14.
//

#include "Renderer.hpp"
#include <simd/simd.h>

#pragma mark - Renderer
#pragma region Renderer {

const int Renderer::kMaxFramesInFlight = 3;

Renderer::Renderer(MTL::Device* pDevice): _pDevice(pDevice->retain()), _angle(0.f), _frame(0) {
    _pCommandQueue = _pDevice->newCommandQueue();
    buildShaders();
    buildBuffers();
    buildFrameData();
    
    _semaphore = dispatch_semaphore_create(Renderer::kMaxFramesInFlight);
}

Renderer::~Renderer() {
    _pShaderLibrary->release();
    _pArgBuffer->release();
    _pVertexPositionsBuffer->release();
    _pVertexColorsBuffer->release();
    for (int i = 0; i < Renderer::kMaxFramesInFlight; ++i) {
        _pFrameData[i]->release();
    }
    _pPSO->release();
    _pCommandQueue->release();
    _pDevice->release();
}

void Renderer::buildShaders() {
    using NS::StringEncoding::UTF8StringEncoding;
    
    const char* shaderSrc = R"(
        #include <metal_stdlib>
        using namespace metal;
        
        struct v2f {
            float4 position [[position]];
            half3 color;
        };
    
        struct VertexData {
            device float3* positions [[id(0)]];
            device float3* colors [[id(1)]];
        };
    
        struct FrameData {
            float angle;
        };
    
        v2f vertex vertexMain(device const VertexData* vertexData [[buffer(0)]], constant FrameData* frameData [[buffer(1)]], uint vertexId [[vertex_id]]) {
            float a = frameData->angle;
            float3x3 rotationMatrix = float3x3(sin(a), cos(a), 0.0, cos(a), -sin(a), 0.0, 0.0, 0.0, 1.0);
            v2f o;
            o.position = float4(rotationMatrix * vertexData->positions[vertexId], 1.0);
            o.color = half3(vertexData->colors[vertexId]);
            return o;
        }
    
        half4 fragment fragmentMain(v2f in [[stage_in]]) {
            return half4(in.color, 1.0);
        }
    )";
    
    NS::Error* pError = nullptr;
    MTL::Library* pLibrary = _pDevice->newLibrary(NS::String::string(shaderSrc, UTF8StringEncoding), nullptr, &pError);
    
    if (pLibrary == nullptr) {
        __builtin_printf("%s", pError->localizedDescription()->utf8String());
        assert(false);
    }
    
    MTL::Function* pVertexFn = pLibrary->newFunction(NS::String::string("vertexMain", UTF8StringEncoding));
    MTL::Function* pFragmentFn = pLibrary->newFunction(NS::String::string("fragmentMain", UTF8StringEncoding));
    
    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction(pVertexFn);
    pDesc->setFragmentFunction(pFragmentFn);
    pDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
    
    _pPSO = _pDevice->newRenderPipelineState(pDesc, &pError);
    if (_pPSO == nullptr) {
        __builtin_printf("%s", pError->localizedDescription()->utf8String());
        assert(false);
    }
    
    pVertexFn->release();
    pFragmentFn->release();
    pDesc->release();
    _pShaderLibrary = pLibrary;
}

void Renderer::buildBuffers() {
    constexpr size_t NUM_VERTICES = 3;
    
    simd::float3 positions[NUM_VERTICES] = {
        { -0.8f, 0.8f, 0.0f },
        { 0.0f, -0.8f, 0.0f },
        { 0.8f, 0.8f, 0.0f }
    };
    
    simd::float3 colors[NUM_VERTICES] = {
        { 1.0f, 0.3f, 0.2f },
        { 0.8f, 1.0f, 0.0f },
        { 0.8f, 0.0f, 1.0f }
    };
    
    const size_t positionDataSize = NUM_VERTICES * sizeof(simd::float3);
    const size_t colorDataSize = NUM_VERTICES * sizeof(simd::float3);
    
    MTL::Buffer* pVertexPositionBuffer = _pDevice->newBuffer(positionDataSize, MTL::ResourceStorageModeShared);
    MTL::Buffer* pVertexColorBuffer = _pDevice->newBuffer(colorDataSize, MTL::ResourceStorageModeShared);
    
    _pVertexPositionsBuffer = pVertexPositionBuffer;
    _pVertexColorsBuffer = pVertexColorBuffer;
    
    memcpy(_pVertexPositionsBuffer->contents(), positions, positionDataSize);
    memcpy(_pVertexColorsBuffer->contents(), colors, colorDataSize);
    
    using NS::StringEncoding::UTF8StringEncoding;
    assert(_pShaderLibrary != nullptr);
    
    MTL::Function* pVertexFn = _pShaderLibrary->newFunction(NS::String::string("vertexMain", UTF8StringEncoding));
    MTL::ArgumentEncoder* pArgEncoder = pVertexFn->newArgumentEncoder(0);
    
    MTL::Buffer* pArgBuffer = _pDevice->newBuffer(pArgEncoder->encodedLength(), MTL::ResourceStorageModeShared);
    _pArgBuffer = pArgBuffer;
    
    pArgEncoder->setArgumentBuffer(_pArgBuffer, 0); // Set destination
    
    pArgEncoder->setBuffer(_pVertexPositionsBuffer, 0, 0); // Encode references to the corresponding index
    pArgEncoder->setBuffer(_pVertexColorsBuffer, 0, 1);
    
    pVertexFn->release();
    pArgEncoder->release();
}

void Renderer::buildFrameData() {
    for (int i = 0; i < Renderer::kMaxFramesInFlight; ++i) {
        _pFrameData[i] = _pDevice->newBuffer(sizeof(FrameData), MTL::ResourceStorageModeShared);
    }
}

void Renderer::draw(MTK::View *pView) {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    _frame = (_frame + 1) % Renderer::kMaxFramesInFlight;
    MTL::Buffer* pFrameDataBuffer = _pFrameData[_frame];
    
    reinterpret_cast<FrameData *>(pFrameDataBuffer->contents())->angle = (_angle += 0.01f);
    
    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer(); // encode commands for execution by the GPU
    dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER); // Force CPU to wait if the GPU hasn't finished reading from the next buffer in the cycle
    Renderer* pRenderer = this;
    pCmd->addCompletedHandler([pRenderer](MTL::CommandBuffer* pCmd) {
        dispatch_semaphore_signal(pRenderer->_semaphore);
    });
    // Start command
    MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd);
    
    pEnc->setRenderPipelineState(_pPSO); // Bind pipeline info
    pEnc->setVertexBuffer(_pArgBuffer, 0, 0); // Bind argument data
    pEnc->useResource(_pVertexPositionsBuffer, MTL::ResourceUsageRead);
    pEnc->useResource(_pVertexColorsBuffer, MTL::ResourceUsageRead);
    
    pEnc->setVertexBuffer(pFrameDataBuffer, 0, 1);
    pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
    
    pEnc->endEncoding();
    pCmd->presentDrawable(pView->currentDrawable()); // Present the current drawable
    // End command
    pCmd->commit(); // Commit the command buffer to its command queue
    
    pPool->release();
}

#pragma endregion Renderer }
