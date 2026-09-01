#ifdef _WIN32
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

#define MKXP_EGL_SHIM_MANGLE(name) mkxp_angle_##name
#include <EGL/egl.h>
#undef MKXP_EGL_SHIM_MANGLE
#define MKXP_EGL_SHIM_MANGLE(name) name

extern bool mkxp_use_angle;

#define FOR_EACH_EGL_PROC(macro) \
    macro(PFNEGLBINDAPIPROC, eglBindAPI) \
    macro(PFNEGLCHOOSECONFIGPROC, eglChooseConfig) \
    macro(PFNEGLCREATECONTEXTPROC, eglCreateContext) \
    macro(PFNEGLCREATEPBUFFERSURFACEPROC, eglCreatePbufferSurface) \
    macro(PFNEGLCREATEWINDOWSURFACEPROC, eglCreateWindowSurface) \
    macro(PFNEGLDESTROYCONTEXTPROC, eglDestroyContext) \
    macro(PFNEGLDESTROYSURFACEPROC, eglDestroySurface) \
    macro(PFNEGLGETCONFIGATTRIBPROC, eglGetConfigAttrib) \
    macro(PFNEGLGETERRORPROC, eglGetError) \
    macro(PFNEGLGETDISPLAYPROC, eglGetDisplay) \
    macro(PFNEGLGETPLATFORMDISPLAYPROC, eglGetPlatformDisplay) \
    macro(PFNEGLGETPROCADDRESSPROC, eglGetProcAddress) \
    macro(PFNEGLINITIALIZEPROC, eglInitialize) \
    macro(PFNEGLMAKECURRENTPROC, eglMakeCurrent) \
    macro(PFNEGLQUERYAPIPROC, eglQueryAPI) \
    macro(PFNEGLQUERYSTRINGPROC, eglQueryString) \
    macro(PFNEGLSWAPBUFFERSPROC, eglSwapBuffers) \
    macro(PFNEGLSWAPINTERVALPROC, eglSwapInterval) \
    macro(PFNEGLTERMINATEPROC, eglTerminate) \
    macro(PFNEGLWAITGLPROC, eglWaitGL) \
    macro(PFNEGLWAITNATIVEPROC, eglWaitNative) \

#define DECLARE_PROC(type, name) static type _##name;
#define LOAD_PROC_FROM_ANGLE(type, name) _##name = mkxp_angle_##name;
#ifdef _WIN32
#  define GET_PROC_ADDRESS(name) GetProcAddress(egl, #name)
#else
#  define GET_PROC_ADDRESS(name) dlsym(egl, #name)
#endif
#define LOAD_PROC_FROM_SYSTEM(type, name) if ((_##name = reinterpret_cast<type>(GET_PROC_ADDRESS(name))) == nullptr && reinterpret_cast<void *>(&_##name) != reinterpret_cast<void *>(&_eglGetPlatformDisplay)) goto error;
#define UNLOAD_PROC(type, name) _##name = nullptr;

FOR_EACH_EGL_PROC(DECLARE_PROC)

static void open_egl() {
#ifdef _WIN32
    static HMODULE egl = nullptr;
#else
    static void *egl = nullptr;
#endif
    static bool egl_loaded = false;
    static bool egl_using_angle;
    if (egl_loaded) {
        if (egl_using_angle != mkxp_use_angle) {
            if (egl != nullptr) {
#ifdef _WIN32
                FreeLibrary(egl);
#else
                dlclose(egl);
#endif
            }
            FOR_EACH_EGL_PROC(UNLOAD_PROC);
        } else {
            return;
        }
    }
    egl_loaded = true;
    egl_using_angle = mkxp_use_angle;
    if (egl_using_angle) {
        FOR_EACH_EGL_PROC(LOAD_PROC_FROM_ANGLE);
        return;
    }
    if (
#ifdef _WIN32
        (egl = LoadLibraryA(MKXP_ANGLE_SHIM_EGL_SONAME)) == nullptr
#else
        (egl = dlopen(MKXP_ANGLE_SHIM_EGL_SONAME, RTLD_LAZY)) == nullptr
#endif
    ) {
        return;
    }
    FOR_EACH_EGL_PROC(LOAD_PROC_FROM_SYSTEM);
    if (_eglGetPlatformDisplay == nullptr) {
        _eglGetPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYPROC>(_eglGetProcAddress("eglGetPlatformDisplayEXT"));
    }
    return;
error:
#ifdef _WIN32
    FreeLibrary(egl);
#else
    dlclose(egl);
#endif
    FOR_EACH_EGL_PROC(UNLOAD_PROC)
}

extern "C" EGLBoolean EGLAPIENTRY eglBindAPI(EGLenum api) {
    open_egl();
    return _eglBindAPI != nullptr ? _eglBindAPI(api) : 0;
}

extern "C" EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config) {
    open_egl();
    return _eglChooseConfig != nullptr ? _eglChooseConfig(dpy, attrib_list, configs, config_size, num_config) : 0;
}

extern "C" EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list) {
    open_egl();
    return _eglCreateContext != nullptr ? _eglCreateContext(dpy, config, share_context, attrib_list) : nullptr;
}

extern "C" EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list) {
    open_egl();
    return _eglCreatePbufferSurface != nullptr ? _eglCreatePbufferSurface(dpy, config, attrib_list) : nullptr;
}

extern "C" EGLSurface EGLAPIENTRY eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list) {
    open_egl();
    return _eglCreateWindowSurface != nullptr ? _eglCreateWindowSurface(dpy, config, win, attrib_list) : nullptr;
}

extern "C" EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
    open_egl();
    return _eglDestroyContext != nullptr ? _eglDestroyContext(dpy, ctx) : 0;
}

extern "C" EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
    open_egl();
    return _eglDestroySurface != nullptr ? _eglDestroySurface(dpy, surface) : 0;
}

extern "C" EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value) {
    open_egl();
    return _eglGetConfigAttrib != nullptr ? _eglGetConfigAttrib(dpy, config, attribute, value) : 0;
}

extern "C" EGLint EGLAPIENTRY eglGetError() {
    open_egl();
    return _eglGetError != nullptr ? _eglGetError() : 0;
}

extern "C" EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id) {
    open_egl();
    return _eglGetDisplay != nullptr ? _eglGetDisplay(display_id) : nullptr;
}

extern "C" EGLDisplay EGLAPIENTRY eglGetPlatformDisplay(EGLenum platform, void *native_display, const EGLAttrib *attrib_list) {
    open_egl();
    return _eglGetPlatformDisplay != nullptr ? _eglGetPlatformDisplay(platform, native_display, attrib_list) : nullptr;
}

extern "C" __eglMustCastToProperFunctionPointerType EGLAPIENTRY eglGetProcAddress(const char *procname) {
    open_egl();
    return _eglGetProcAddress != nullptr ? _eglGetProcAddress(procname) : nullptr;
}

extern "C" EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor) {
    open_egl();
    return _eglInitialize != nullptr ? _eglInitialize(dpy, major, minor) : 0;
}

extern "C" EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
    open_egl();
    return _eglMakeCurrent != nullptr ? _eglMakeCurrent(dpy, draw, read, ctx) : 0;
}

extern "C" EGLenum EGLAPIENTRY eglQueryAPI() {
    open_egl();
    return _eglQueryAPI != nullptr ? _eglQueryAPI() : 0;
}

extern "C" const char *EGLAPIENTRY eglQueryString(EGLDisplay dpy, EGLint name) {
    open_egl();
    return _eglQueryString != nullptr ? _eglQueryString(dpy, name) : nullptr;
}

extern "C" EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    open_egl();
    return _eglSwapBuffers != nullptr ? _eglSwapBuffers(dpy, surface) : 0;
}

extern "C" EGLBoolean EGLAPIENTRY eglSwapInterval(EGLDisplay dpy, EGLint interval) {
    open_egl();
    return _eglSwapInterval != nullptr ? _eglSwapInterval(dpy, interval) : 0;
}

extern "C" EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay dpy) {
    open_egl();
    return _eglTerminate != nullptr ? _eglTerminate(dpy) : 0;
}

extern "C" EGLBoolean EGLAPIENTRY eglWaitGL() {
    open_egl();
    return _eglWaitGL != nullptr ? _eglWaitGL() : 0;
}

extern "C" EGLBoolean EGLAPIENTRY eglWaitNative(EGLint engine) {
    open_egl();
    return _eglWaitNative != nullptr ? _eglWaitNative(engine) : 0;
}
