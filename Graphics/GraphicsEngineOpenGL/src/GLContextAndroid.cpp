/*     Copyright 2015-2018 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include "pch.h"
#include <utility>

#include "GLContextAndroid.hpp"

#ifndef EGL_CONTEXT_MINOR_VERSION_KHR
#    define EGL_CONTEXT_MINOR_VERSION_KHR 0x30FB
#endif

#ifndef EGL_CONTEXT_MAJOR_VERSION_KHR
#    define EGL_CONTEXT_MAJOR_VERSION_KHR EGL_CONTEXT_CLIENT_VERSION
#endif

namespace Diligent
{

static void openglCallbackFunction(GLenum        source,
                                   GLenum        type,
                                   GLuint        id,
                                   GLenum        severity,
                                   GLsizei       length,
                                   const GLchar* message,
                                   const void*   userParam)
{
    std::stringstream MessageSS;

    MessageSS << "OpenGL debug message " << id << " (";
    switch (source)
    {
        // clang-format off
        case GL_DEBUG_SOURCE_API:             MessageSS << "Source: API.";             break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   MessageSS << "Source: Window System.";   break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: MessageSS << "Source: Shader Compiler."; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     MessageSS << "Source: Third Party.";     break;
        case GL_DEBUG_SOURCE_APPLICATION:     MessageSS << "Source: Application.";     break;
        case GL_DEBUG_SOURCE_OTHER:           MessageSS << "Source: Other.";           break;
        default:                              MessageSS << "Source: Unknown (" << source << ").";
            // clang-format on
    }

    switch (type)
    {
        // clang-format off
        case GL_DEBUG_TYPE_ERROR:               MessageSS << " Type: ERROR.";                break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: MessageSS << " Type: Deprecated Behaviour."; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  MessageSS << " Type: UNDEFINED BEHAVIOUR.";  break;
        case GL_DEBUG_TYPE_PORTABILITY:         MessageSS << " Type: Portability.";          break;
        case GL_DEBUG_TYPE_PERFORMANCE:         MessageSS << " Type: PERFORMANCE.";          break;
        case GL_DEBUG_TYPE_MARKER:              MessageSS << " Type: Marker.";               break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          MessageSS << " Type: Push Group.";           break;
        case GL_DEBUG_TYPE_POP_GROUP:           MessageSS << " Type: Pop Group.";            break;
        case GL_DEBUG_TYPE_OTHER:               MessageSS << " Type: Other.";                break;
        default:                                MessageSS << " Type: Unknown (" << type << ").";
            // clang-format on
    }

    switch (severity)
    {
        // clang-format off
        case GL_DEBUG_SEVERITY_HIGH:         MessageSS << " Severity: HIGH";         break;
        case GL_DEBUG_SEVERITY_MEDIUM:       MessageSS << " Severity: Medium";       break;
        case GL_DEBUG_SEVERITY_LOW:          MessageSS << " Severity: Low";          break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: MessageSS << " Severity: Notification"; break;
        default:                             MessageSS << " Severity: Unknown (" << severity << ")"; break;
            // clang-format on
    }

    MessageSS << "): " << message;

    LOG_INFO_MESSAGE(MessageSS.str().c_str());
}

bool GLContext::InitEGLSurface()
{
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY)
    {
        LOG_ERROR_AND_THROW("No EGL display found");
    }

    auto success = eglInitialize(display_, 0, 0);
    if (!success)
    {
        LOG_ERROR_AND_THROW("Failed to initialise EGL");
    }

    /*
     * Here specify the attributes of the desired configuration.
     * Below, we select an EGLConfig with at least 8 bits per color
     * component compatible with on-screen windows
     */
    color_size_ = 8;
    depth_size_ = 24;
    const EGLint attribs[] =
        {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, //Request opengl ES2.0
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            //EGL_COLORSPACE, EGL_COLORSPACE_sRGB, // does not work
            EGL_BLUE_SIZE, color_size_,
            EGL_GREEN_SIZE, color_size_,
            EGL_RED_SIZE, color_size_,
            EGL_ALPHA_SIZE, color_size_,
            EGL_DEPTH_SIZE, depth_size_,
            //EGL_SAMPLE_BUFFERS  , 1,
            //EGL_SAMPLES         , 4,
            EGL_NONE //
        };

    // Get a list of EGL frame buffer configurations that match specified attributes
    EGLint num_configs;
    success = eglChooseConfig(display_, attribs, &config_, 1, &num_configs);
    if (!success)
    {
        LOG_ERROR_AND_THROW("Failed to choose config");
    }

    if (!num_configs)
    {
        //Fall back to 16bit depth buffer
        depth_size_ = 16;
        const EGLint attribs[] =
            {
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, //Request opengl ES2.0
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_BLUE_SIZE, color_size_,
                EGL_GREEN_SIZE, color_size_,
                EGL_RED_SIZE, color_size_,
                EGL_ALPHA_SIZE, color_size_,
                EGL_DEPTH_SIZE, depth_size_,
                EGL_NONE //
            };
        success = eglChooseConfig(display_, attribs, &config_, 1, &num_configs);
        if (!success)
        {
            LOG_ERROR_AND_THROW("Failed to choose 16-bit depth config");
        }
    }

    if (!num_configs)
    {
        LOG_ERROR_AND_THROW("Unable to retrieve EGL config");
    }

    LOG_INFO_MESSAGE("Chosen EGL config: ", color_size_, " bit color, ", depth_size_, " bit depth");

    surface_ = eglCreateWindowSurface(display_, config_, window_, NULL);
    if (surface_ == EGL_NO_SURFACE)
    {
        LOG_ERROR_AND_THROW("Failed to create EGLSurface");
    }

    eglQuerySurface(display_, surface_, EGL_WIDTH, &screen_width_);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &screen_height_);

    /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
     * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
     * As soon as we picked a EGLConfig, we can safely reconfigure the
     * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
    EGLint format;
    eglGetConfigAttrib(display_, config_, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(window_, 0, 0, format);

    eglGetConfigAttrib(display_, config_, EGL_MIN_SWAP_INTERVAL, &min_swap_interval_);
    eglGetConfigAttrib(display_, config_, EGL_MAX_SWAP_INTERVAL, &max_swap_interval_);

    return true;
}

bool GLContext::InitEGLContext()
{
    std::pair<int, int> es_versions[] = {{3, 2}, {3, 1}, {3, 0}};
    for (size_t i = 0; i < _countof(es_versions) && context_ == EGL_NO_CONTEXT; ++i)
    {
        const auto& version = es_versions[i];
        major_version_      = version.first;
        minor_version_      = version.second;

        const EGLint context_attribs[] =
            {
                EGL_CONTEXT_CLIENT_VERSION, major_version_,
                EGL_CONTEXT_MINOR_VERSION_KHR, minor_version_,
                EGL_NONE};

        context_ = eglCreateContext(display_, config_, NULL, context_attribs);
    }

    if (context_ == EGL_NO_CONTEXT)
    {
        LOG_ERROR_AND_THROW("Failed to create EGLContext");
    }

    if (eglMakeCurrent(display_, surface_, surface_, context_) == EGL_FALSE)
    {
        LOG_ERROR_AND_THROW("Unable to eglMakeCurrent");
    }

    LOG_INFO_MESSAGE("Created OpenGLES Context ", major_version_, '.', minor_version_);
    context_valid_ = true;
    return true;
}

void GLContext::AttachToCurrentEGLContext()
{
    if (eglGetCurrentContext() == EGL_NO_CONTEXT)
    {
        LOG_ERROR_AND_THROW("Failed to attach to EGLContext: no active context");
    }
    context_valid_ = true;
    glGetIntegerv(GL_MAJOR_VERSION, &major_version_);
    glGetIntegerv(GL_MINOR_VERSION, &minor_version_);
}

GLContext::NativeGLContextType GLContext::GetCurrentNativeGLContext()
{
    return eglGetCurrentContext();
}

void GLContext::InitGLES()
{
    if (gles_initialized_)
        return;
    //
    //Initialize OpenGL ES 3 if available
    //
    const char* versionStr = (const char*)glGetString(GL_VERSION);
    LOG_INFO_MESSAGE("GL Version: ", versionStr, '\n');

    LoadGLFunctions();

    // When GL_FRAMEBUFFER_SRGB is enabled, and if the destination image is in the sRGB colorspace
    // then OpenGL will assume the shader's output is in the linear RGB colorspace. It will therefore
    // convert the output from linear RGB to sRGB.
    // Any writes to images that are not in the sRGB format should not be affected.
    // Thus this setting should be just set once and left that way
    glEnable(GL_FRAMEBUFFER_SRGB);
    if (glGetError() != GL_NO_ERROR)
        LOG_ERROR_MESSAGE("Failed to enable SRGB framebuffers");

    gles_initialized_ = true;
}

bool GLContext::Init(ANativeWindow* window)
{
    if (egl_context_initialized_)
        return true;

    //
    //Initialize EGL
    //
    window_ = window;
    if (window != nullptr)
    {
        InitEGLSurface();
        InitEGLContext();
    }
    else
    {
        AttachToCurrentEGLContext();
    }
    InitGLES();

    if (glDebugMessageCallback)
    {
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(openglCallbackFunction, nullptr);
        if (glGetError() != GL_NO_ERROR)
            LOG_ERROR_MESSAGE("Failed to enable debug messages");
    }

    egl_context_initialized_ = true;

    return true;
}

GLContext::GLContext(const EngineGLCreateInfo& InitAttribs, DeviceCaps& deviceCaps, const struct SwapChainDesc* /*pSCDesc*/) :
    display_(EGL_NO_DISPLAY),
    surface_(EGL_NO_SURFACE),
    context_(EGL_NO_CONTEXT),
    egl_context_initialized_(false),
    gles_initialized_(false),
    major_version_(0),
    minor_version_(0)
{
    auto* NativeWindow = reinterpret_cast<ANativeWindow*>(InitAttribs.Window.pAWindow);
    Init(NativeWindow);

    FillDeviceCaps(deviceCaps);
}

GLContext::~GLContext()
{
    Terminate();
}

void GLContext::SwapBuffers(int SwapInterval)
{
    if (surface_ == EGL_NO_SURFACE)
    {
        LOG_WARNING_MESSAGE("No EGL surface when swapping buffers. This happens when SwapBuffers() is called after Suspend(). The operation will be ignored.");
        return;
    }

    SwapInterval = std::max(SwapInterval, min_swap_interval_);
    SwapInterval = std::min(SwapInterval, max_swap_interval_);
    eglSwapInterval(display_, SwapInterval);

    bool b = eglSwapBuffers(display_, surface_);
    if (!b)
    {
        EGLint err = eglGetError();
        if (err == EGL_BAD_SURFACE)
        {
            LOG_INFO_MESSAGE("EGL surface has been lost. Attempting to recreate");
            try
            {
                InitEGLSurface();
            }
            catch (std::runtime_error&)
            {
                LOG_ERROR_MESSAGE("Failed to recreate EGL surface");
            }
            //return EGL_SUCCESS; //Still consider glContext is valid
        }
        else if (err == EGL_CONTEXT_LOST || err == EGL_BAD_CONTEXT)
        {
            //Context has been lost!!
            context_valid_ = false;
            Terminate();
            InitEGLContext();
        }
        //return err;
    }
    //return EGL_SUCCESS;
}

void GLContext::Terminate()
{
    if (display_ != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT)
        {
            eglDestroyContext(display_, context_);
        }

        if (surface_ != EGL_NO_SURFACE)
        {
            eglDestroySurface(display_, surface_);
        }
        eglTerminate(display_);
    }

    display_       = EGL_NO_DISPLAY;
    context_       = EGL_NO_CONTEXT;
    surface_       = EGL_NO_SURFACE;
    context_valid_ = false;
}


void GLContext::UpdateScreenSize()
{
    int32_t new_screen_width  = 0;
    int32_t new_screen_height = 0;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &new_screen_width);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &new_screen_height);

    if (new_screen_width != screen_width_ || new_screen_height != screen_height_)
    {
        screen_width_  = new_screen_width;
        screen_height_ = new_screen_height;
        //Screen resized
        LOG_INFO_MESSAGE("Window size changed to ", screen_width_, "x", screen_height_);
    }
}

EGLint GLContext::Resume(ANativeWindow* window)
{
    LOG_INFO_MESSAGE("Resuming gl context\n");

    if (egl_context_initialized_ == false)
    {
        Init(window);
        return EGL_SUCCESS;
    }

    //Create surface
    window_  = window;
    surface_ = eglCreateWindowSurface(display_, config_, window_, NULL);
    UpdateScreenSize();

    if (eglMakeCurrent(display_, surface_, surface_, context_) == EGL_TRUE)
        return EGL_SUCCESS;

    EGLint err = eglGetError();
    LOG_WARNING_MESSAGE("Unable to eglMakeCurrent ", err, '\n');

    if (err == EGL_CONTEXT_LOST)
    {
        //Recreate context
        LOG_INFO_MESSAGE("Re-creating egl context\n");
        InitEGLContext();
    }
    else
    {
        //Recreate surface
        LOG_INFO_MESSAGE("Re-creating egl context and surface\n");
        Terminate();
        InitEGLSurface();
        InitEGLContext();
    }

    return err;
}

void GLContext::Suspend()
{
    LOG_INFO_MESSAGE("Suspending gl context\n");
    if (surface_ != EGL_NO_SURFACE)
    {
        LOG_INFO_MESSAGE("Destroying egl surface\n");
        eglDestroySurface(display_, surface_);
        surface_ = EGL_NO_SURFACE;
    }
}

bool GLContext::Invalidate()
{
    LOG_INFO_MESSAGE("Invalidating gl context\n");
    Terminate();

    egl_context_initialized_ = false;
    return true;
}

void GLContext::FillDeviceCaps(DeviceCaps& deviceCaps)
{
    deviceCaps.DevType      = RENDER_DEVICE_TYPE_GLES;
    deviceCaps.MajorVersion = major_version_;
    deviceCaps.MinorVersion = minor_version_;
}

} // namespace Diligent
