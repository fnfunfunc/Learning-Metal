//
//  main.cpp
//  LearningMetal
//
//  Created by eternal on 2024/4/13.
//

#define NS_PRIVATE_IMPLEMENTATION
#define UI_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
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
