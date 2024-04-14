//
//  main.cpp
//  LearningMetal
//
//  Created by eternal on 2024/4/13.
//

#include "main.hpp"
#include <simd/simd.h>

int main(int argc, char* argv[]) {
    NS::AutoreleasePool *pAutoReleasePool = NS::AutoreleasePool::alloc()->init();
    
    MyAppDelegate delegate;
    UI::ApplicationMain(argc, argv, &delegate);
    
    pAutoReleasePool->release();
    
    return EXIT_SUCCESS;
}

#pragma mark - AppDelegate
#pragma region AppDelegate {

MyAppDelegate::~MyAppDelegate() {
    _pMtkView->release();
    _pWindow->release();
    _pViewController->release();
    _pDevice->release();
    delete _pViewDelegate;
}

/**
 * Create the window
 */
bool MyAppDelegate::applicationDidFinishLaunching(UI::Application *pApp, NS::Value *options) {
    CGRect frame = UI::Screen::mainScreen()->bounds();
    
    _pWindow = UI::Window::alloc()->init(frame);
    
    _pViewController = UI::ViewController::alloc()->init(nullptr, nullptr);
    
    _pDevice = MTL::CreateSystemDefaultDevice(); // if the device supports metal, then it will return a non-null MTL::Device
    
    _pMtkView = MTK::View::alloc()->init(frame, _pDevice);
    _pMtkView->setColorPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
    _pMtkView->setClearColor(MTL::ClearColor::Make(1.0, 0.0, 0.0, 1.0));
    
    _pViewDelegate = new MyMTKViewDelegate(_pDevice);
    _pMtkView->setDelegate(_pViewDelegate);
    
    UI::View* mtkView = reinterpret_cast<UI::View*>(_pMtkView);
    mtkView->setAutoresizingMask(UI::ViewAutoresizingFlexibleWidth | UI::ViewAutoresizingFlexibleHeight);
    _pViewController->view()->addSubview(mtkView);
    _pWindow->setRootViewController(_pViewController);
    
    _pWindow->makeKeyAndVisible();
    
    return true;
}

void MyAppDelegate::applicationWillTerminate(UI::Application *pApp) {
    
}

#pragma endregion AppDelegate }

#pragma mark - ViewDelegate
#pragma region ViewDelegate {

MyMTKViewDelegate::MyMTKViewDelegate(MTL::Device* pDevice): MTK::ViewDelegate(), _pRenderer(new Renderer(pDevice)) {
    
}

MyMTKViewDelegate::~MyMTKViewDelegate() {
    delete _pRenderer;
}

void MyMTKViewDelegate::drawInMTKView(MTK::View *pView) {
    _pRenderer->draw(pView);
}

#pragma endregion ViewDelegate }

#pragma mark - Renderer
#pragma region Renderer {

Renderer::Renderer(MTL::Device* pDevice): _pDevice(pDevice->retain()) {
    _pCommandQueue = _pDevice->newCommandQueue();
    buildShaders();
    buildBuffers();
}

Renderer::~Renderer() {
    _pShaderLibrary->release();
    _pArgBuffer->release();
    _pVertexPositionsBuffer->release();
    _pVertexColorsBuffer->release();
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
    
        v2f vertex vertexMain(device const VertexData* vertexData [[buffer(0)]], uint vertexId [[vertex_id]]) {
            v2f o;
            o.position = float4(vertexData->positions[vertexId], 1.0);
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

void Renderer::draw(MTK::View *pView) {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer(); // encode commands for execution by the GPU
    // Start command
    MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd);
    
    pEnc->setRenderPipelineState(_pPSO); // Bind pipeline info
    pEnc->setVertexBuffer(_pArgBuffer, 0, 0); // Bind argument data
    pEnc->useResource(_pVertexPositionsBuffer, MTL::ResourceUsageRead);
    pEnc->useResource(_pVertexColorsBuffer, MTL::ResourceUsageRead);
    pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
    
    pEnc->endEncoding();
    pCmd->presentDrawable(pView->currentDrawable()); // Present the current drawable
    // End command
    pCmd->commit(); // Commit the command buffer to its command queue
    
    pPool->release();
}

#pragma endregion Renderer }
