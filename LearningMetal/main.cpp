//
//  main.cpp
//  LearningMetal
//
//  Created by eternal on 2024/4/13.
//

#include "main.hpp"

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
}

Renderer::~Renderer() {
    _pCommandQueue->release();
    _pDevice->release();
}

void Renderer::draw(MTK::View *pView) {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer(); // encode commands for execution by the GPU
    // Start command
    MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd);
    pEnc->endEncoding();
    pCmd->presentDrawable(pView->currentDrawable()); // Present the current drawable
    // End command
    pCmd->commit(); // Commit the command buffer to its command queue
    
    pPool->release();
}

#pragma endregion Renderer }
