//
//  Renderer.cpp
//  LearningMetal
//
//  Created by eternal on 2024/4/14.
//

#include "MathUtils.hpp"
#include "Renderer.hpp"
#include <simd/simd.h>

#pragma mark - Renderer
#pragma region Renderer {

const int Renderer::kMaxFramesInFlight = 3;

Renderer::Renderer(MTL::Device* pDevice): _pDevice(pDevice->retain()), _angle(0.f), _frame(0) {
    _pCommandQueue = _pDevice->newCommandQueue();
    buildShaders();
    buildDepthStencilStates();
    buildBuffers();
    
    _semaphore = dispatch_semaphore_create(Renderer::kMaxFramesInFlight);
}

Renderer::~Renderer() {
    _pShaderLibrary->release();
    _pVertexDataBuffer->release();
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        _pInstanceDataBuffer[i]->release();
    }
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        _pCameraDataBuffer[i]->release();
    }
    _pIndexBuffer->release();
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
            float3 position;
        };
    
        struct InstanceData {
            float4x4 instanceTransform;
            float4 instanceColor;
        };
    
        struct CameraData {
            float4x4 perspectiveTransform;
            float4x4 worldTransform;
        };
    
        v2f vertex vertexMain(device const VertexData* vertexData [[buffer(0)]], device const InstanceData* instanceData [[buffer(1)]], device const CameraData& cameraData [[buffer(2)]], uint vertexId [[vertex_id]], uint instanceId [[instance_id]]) {
            v2f o;
            float4 pos = float4(vertexData[vertexId].position, 1.0);
            pos = instanceData[instanceId].instanceTransform * pos;
            pos = cameraData.perspectiveTransform * cameraData.worldTransform * pos;
            o.position = pos;
            o.color = half3(instanceData[instanceId].instanceColor.rgb);
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
    pDesc->setDepthAttachmentPixelFormat(MTL::PixelFormat::PixelFormatDepth16Unorm);
    
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

void Renderer::buildDepthStencilStates() {
    MTL::DepthStencilDescriptor* pDsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDsDesc->setDepthCompareFunction(MTL::CompareFunction::CompareFunctionLess);
    pDsDesc->setDepthWriteEnabled(true);
    
    _pDepthStencilState = _pDevice->newDepthStencilState(pDsDesc);
    
    pDsDesc->release();
}

void Renderer::buildBuffers() {
    using simd::float3;
    
    const float s = 0.5f;
    
    float3 verts[] = {
        { -s, -s, +s },
        { +s, -s, +s },
        { +s, +s, +s },
        { -s, +s, +s },

        { -s, -s, -s },
        { -s, +s, -s },
        { +s, +s, -s },
        { +s, -s, -s }
    };
    
    uint16_t indices[] = {
        0, 1, 2, /* front */
        2, 3, 0,

        1, 7, 6, /* right */
        6, 2, 1,

        7, 4, 5, /* back */
        5, 6, 7,

        4, 0, 3, /* left */
        3, 5, 4,

        3, 2, 6, /* top */
        6, 5, 3,

        4, 7, 1, /* bottom */
        1, 0, 4
    };
    
    const size_t vertexDataSize = sizeof(verts);
    const size_t indexDataSize = sizeof(indices);
    
    MTL::Buffer* pVertexBuffer = _pDevice->newBuffer(vertexDataSize, MTL::ResourceStorageModeShared);
    MTL::Buffer* pIndexBuffer = _pDevice->newBuffer(indexDataSize, MTL::ResourceStorageModeShared);
    
    _pVertexDataBuffer = pVertexBuffer;
    _pIndexBuffer = pIndexBuffer;
    
    memcpy(_pVertexDataBuffer->contents(), verts, vertexDataSize);
    memcpy(_pIndexBuffer->contents(), indices, indexDataSize);
    
    const size_t instanceDataSize = kMaxFramesInFlight * kNumInstances * sizeof(shader_types::InstanceData);
    
    for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
        _pInstanceDataBuffer[i] = _pDevice->newBuffer(instanceDataSize, MTL::ResourceStorageModeShared);
    }
    
    const size_t cameraDataSize = kMaxFramesInFlight * sizeof(shader_types::CameraData);
    for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
        _pCameraDataBuffer[i] = _pDevice->newBuffer(cameraDataSize, MTL::ResourceStorageModeShared);
    }
}

void Renderer::draw(MTK::View *pView) {
    using simd::float3;
    using simd::float4;
    using simd::float4x4;
    
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    _frame = (_frame + 1) % Renderer::kMaxFramesInFlight;
    MTL::Buffer* pInstanceDataBuffer = _pInstanceDataBuffer[_frame];
    
    
    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer(); // encode commands for execution by the GPU
    dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER); // Force CPU to wait if the GPU hasn't finished reading from the next buffer in the cycle
    Renderer* pRenderer = this;
    pCmd->addCompletedHandler([pRenderer](MTL::CommandBuffer* pCmd) {
        dispatch_semaphore_signal(pRenderer->_semaphore);
    });
    
    _angle += 0.01f;
    
    const float scl = 0.1f;
    shader_types::InstanceData* pInstanceData = reinterpret_cast<shader_types::InstanceData *>(pInstanceDataBuffer->contents());
    
    float3 objectPosition = { 0.0f, 0.0f, -5.0f };
    
    // Update instance positions
    float4x4 rt = math_utils::makeTranslate(objectPosition);
    float4x4 rr = math_utils::makeYRotate(-_angle);
    float4x4 rtInv = math_utils::makeTranslate({ -objectPosition.x, -objectPosition.y, -objectPosition.z });
    float4x4 fullObjectRot = rt * rr * rtInv;
    
    for (size_t i = 0; i < kNumInstances; ++i) {
        float iDivNumInstances = i / static_cast<float>(kNumInstances);
        float xoff = (iDivNumInstances * 2.0f - 1.0f) + (1.f / kNumInstances);
        float yoff = sin((iDivNumInstances + _angle) * 2.0f * M_PI);
        
        // Use the tiny math utils to apply a 3D transformation to the instance.
        float4x4 scale = math_utils::makeScale(scl);
        float4x4 zrot = math_utils::makeZRotate(_angle);
        float4x4 yrot = math_utils::makeYRotate(_angle);
        float4x4 translate = math_utils::makeTranslate(math_utils::add(objectPosition, { xoff, yoff, 0.f }));
        
        pInstanceData[i].instanceTransform = fullObjectRot * translate * yrot * zrot * scale;
        float r = iDivNumInstances;
        float g = 1.0f - r;
        float b = sinf(M_PI * 2.0f * iDivNumInstances);
        pInstanceData[i].instanceColor = { r, g, b, 1.0f };
    }
    
    // Update camera state
    MTL::Buffer* pCameraDataBuffer = _pCameraDataBuffer[_frame];
    shader_types::CameraData* pCameraData = reinterpret_cast<shader_types::CameraData*>(pCameraDataBuffer->contents());
    pCameraData->perspectiveTransform = math_utils::makePerspective(45.f * M_PI / 180.f, 1.f, 0.03f, 500.0f);
    pCameraData->worldTransform = math_utils::makeIdentity();
    
    // Begin render pass
    MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd);
    
    pEnc->setRenderPipelineState(_pPSO); // Bind pipeline info
    pEnc->setDepthStencilState(_pDepthStencilState);
    
    pEnc->setVertexBuffer(_pVertexDataBuffer, 0, 0);
    pEnc->setVertexBuffer(pInstanceDataBuffer, 0, 1);
    pEnc->setVertexBuffer(pCameraDataBuffer, 0, 2);
    
    pEnc->setCullMode(MTL::CullModeBack);
    pEnc->setFrontFacingWinding(MTL::Winding::WindingCounterClockwise);
    
    pEnc->drawIndexedPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, 6 * 6, MTL::IndexType::IndexTypeUInt16, _pIndexBuffer, 0, kNumInstances);
    
    pEnc->endEncoding();
    pCmd->presentDrawable(pView->currentDrawable()); // Present the current drawable
    // End command
    pCmd->commit(); // Commit the command buffer to its command queue
    
    pPool->release();
}

#pragma endregion Renderer }
