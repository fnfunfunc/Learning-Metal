//
//  main.hpp
//  LearningMetal
//
//  Created by eternal on 2024/4/13.
//

#ifndef main_h
#define main_h


#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>
#include <UIKit/UIKit.hpp>
#include <simd/simd.h>
#include "Renderer.hpp"

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

#endif /* main_h */
