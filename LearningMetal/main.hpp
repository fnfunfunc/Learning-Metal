//
//  main.hpp
//  LearningMetal
//
//  Created by eternal on 2024/4/13.
//

#ifndef main_h
#define main_h

#define NS_PRIVATE_IMPLEMENTATION
#define UI_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>
#include <UIKit/UIKit.hpp>

#include <simd/simd.h>

class Renderer {
public:
    Renderer(MTL::Device* pDevice);
    ~Renderer();
    void buildShaders();
    void buildBuffers();
    void buildFrameData();
    void draw(MTK::View* pView);
    
private:
    MTL::Device* _pDevice;
    MTL::CommandQueue* _pCommandQueue;
    MTL::Library* _pShaderLibrary;
    MTL::RenderPipelineState* _pPSO;
    MTL::Buffer* _pArgBuffer;
    MTL::Buffer* _pVertexPositionsBuffer;
    MTL::Buffer* _pVertexColorsBuffer;
    MTL::Buffer* _pFrameData[3];
    float _angle;
    int _frame;
    dispatch_semaphore_t _semaphore;
    static const int kMaxFramesInFlight;
};

class MyMTKViewDelegate : public MTK::ViewDelegate {
public:
    MyMTKViewDelegate(MTL::Device* pDevice);
    virtual ~MyMTKViewDelegate() override;
    virtual void drawInMTKView(MTK::View* pView) override;
    
private:
    Renderer* _pRenderer;
};

class MyAppDelegate : public UI::ApplicationDelegate {
public:
    ~MyAppDelegate();
    
    bool applicationDidFinishLaunching(UI::Application *pApp, NS::Value *options) override;
    
    void applicationWillTerminate(UI::Application *pApp) override;
    
private:
    UI::Window* _pWindow;
    UI::ViewController* _pViewController;
    MTK::View* _pMtkView;
    MTL::Device* _pDevice;
    MyMTKViewDelegate* _pViewDelegate = nullptr;
};

struct FrameData {
    float angle;
};

#endif /* main_h */
