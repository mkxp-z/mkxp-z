#include <dlfcn.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

static int (*_xCloseDisplay)(Display *display) = nullptr;
static Colormap (*_xCreateColormap)(Display *display, Window w, Visual *visual, int alloc) = nullptr;
static Window (*_xCreateWindow)(Display *display, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width, int depth, unsigned int class_, Visual *visual, unsigned long valuemask, XSetWindowAttributes *attributes) = nullptr;
static int (*_xDestroyWindow)(Display *display, Window w) = nullptr;
static int (*_xFlush)(Display *display) = nullptr;
static int (*_xFree)(void *data) = nullptr;
static int (*_xFreeColormap)(Display *display, Colormap colormap) = nullptr;
static int (*_xGetErrorText)(Display *display, int code, char *buffer_return, int length) = nullptr;
static Status (*_xGetGeometry)(Display *display, Drawable d, Window *root_return, int *x_return, int *y_return, unsigned int *width_return, unsigned int *height_return, unsigned int *border_width_return, unsigned int *depth_return) = nullptr;
static XVisualInfo *(*_xGetVisualInfo)(Display *display, long vinfo_mask, XVisualInfo *vinfo_template, int *nitems_return) = nullptr;
static Status (*_xGetWindowAttributes)(Display *display, Window w, XWindowAttributes *window_attributes_return) = nullptr;
static int (*_xMapWindow)(Display *display, Window w) = nullptr;
static Display *(*_xOpenDisplay)(_Xconst char *display_name) = nullptr;
static int (*_xResizeWindow)(Display *display, Window w, unsigned int width, unsigned int height) = nullptr;
static XErrorHandler (*_xSetErrorHandler)(XErrorHandler handler) = nullptr;
static int (*_xSync)(Display *display, Bool discard) = nullptr;

static void open_x11() {
    static bool x11_loaded = false;
    if (x11_loaded) {
        return;
    }
    x11_loaded = true;
    void *x11 = nullptr;
    if ((x11 = dlopen(MKXP_ANGLE_SHIM_X11_SONAME, RTLD_LAZY)) == nullptr) {
        return;
    }
    if (
        (_xCloseDisplay = reinterpret_cast<int (*)(Display *display)>(dlsym(x11, "XCloseDisplay"))) == nullptr
            || (_xCreateColormap = reinterpret_cast<Colormap (*)(Display *display, Window w, Visual *visual, int alloc)>(dlsym(x11, "XCreateColormap"))) == nullptr
            || (_xCreateWindow = reinterpret_cast<Window (*)(Display *display, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width, int depth, unsigned int class_, Visual *visual, unsigned long valuemask, XSetWindowAttributes *attributes)>(dlsym(x11, "XCreateWindow"))) == nullptr
            || (_xDestroyWindow = reinterpret_cast<int (*)(Display *display, Window w)>(dlsym(x11, "XDestroyWindow"))) == nullptr
            || (_xFlush = reinterpret_cast<int (*)(Display *display)>(dlsym(x11, "XFlush"))) == nullptr
            || (_xFree = reinterpret_cast<int (*)(void *data)>(dlsym(x11, "XFree"))) == nullptr
            || (_xFreeColormap = reinterpret_cast<int (*)(Display *display, Colormap colormap)>(dlsym(x11, "XFreeColormap"))) == nullptr
            || (_xGetErrorText = reinterpret_cast<int (*)(Display *display, int code, char *buffer_return, int length)>(dlsym(x11, "XGetErrorText"))) == nullptr
            || (_xGetGeometry = reinterpret_cast<Status (*)(Display *display, Drawable d, Window *root_return, int *x_return, int *y_return, unsigned int *width_return, unsigned int *height_return, unsigned int *border_width_return, unsigned int *depth_return)>(dlsym(x11, "XGetGeometry"))) == nullptr
            || (_xGetVisualInfo = reinterpret_cast<XVisualInfo *(*)(Display *display, long vinfo_mask, XVisualInfo *vinfo_template, int *nitems_return)>(dlsym(x11, "XGetVisualInfo"))) == nullptr
            || (_xGetWindowAttributes = reinterpret_cast<Status (*)(Display *display, Window w, XWindowAttributes *window_attributes_return)>(dlsym(x11, "XGetWindowAttributes"))) == nullptr
            || (_xMapWindow = reinterpret_cast<int (*)(Display *display, Window w)>(dlsym(x11, "XMapWindow"))) == nullptr
            || (_xOpenDisplay = reinterpret_cast<Display *(*)(_Xconst char *display_name)>(dlsym(x11, "XOpenDisplay"))) == nullptr
            || (_xResizeWindow = reinterpret_cast<int (*)(Display *display, Window w, unsigned int width, unsigned int height)>(dlsym(x11, "XResizeWindow"))) == nullptr
            || (_xSetErrorHandler = reinterpret_cast<XErrorHandler (*)(XErrorHandler handler)>(dlsym(x11, "XSetErrorHandler"))) == nullptr
            || (_xSync = reinterpret_cast<int (*)(Display *display, Bool discard)>(dlsym(x11, "XSync"))) == nullptr
    ) {
        dlclose(x11);
        _xCloseDisplay = nullptr;
        _xCreateColormap = nullptr;
        _xCreateWindow = nullptr;
        _xDestroyWindow = nullptr;
        _xFlush = nullptr;
        _xFree = nullptr;
        _xFreeColormap = nullptr;
        _xGetErrorText = nullptr;
        _xGetGeometry = nullptr;
        _xGetVisualInfo = nullptr;
        _xGetWindowAttributes = nullptr;
        _xMapWindow = nullptr;
        _xOpenDisplay = nullptr;
        _xResizeWindow = nullptr;
        _xSetErrorHandler = nullptr;
        _xSync = nullptr;
    }
}

extern "C" int mkxp_angle_x11_shim_XCloseDisplay(Display *display) {
    open_x11();
    return _xCloseDisplay != nullptr ? _xCloseDisplay(display) : 0;
}

extern "C" Colormap mkxp_angle_x11_shim_XCreateColormap(Display *display, Window w, Visual *visual, int alloc) {
    open_x11();
    return _xCreateColormap != nullptr ? _xCreateColormap(display, w, visual, alloc) : 0;
}

extern "C" Window mkxp_angle_x11_shim_XCreateWindow(Display *display, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width, int depth, unsigned int class_, Visual *visual, unsigned long valuemask, XSetWindowAttributes *attributes) {
    open_x11();
    return _xCreateWindow != nullptr ? _xCreateWindow(display, parent, x, y, width, height, border_width, depth, class_, visual, valuemask, attributes) : 0;
}

extern "C" int mkxp_angle_x11_shim_XDestroyWindow(Display *display, Window w) {
    open_x11();
    return _xDestroyWindow != nullptr ? _xDestroyWindow(display, w) : 0;
}

extern "C" int mkxp_angle_x11_shim_XFlush(Display *display) {
    open_x11();
    return _xFlush != nullptr ? _xFlush(display) : 0;
}

extern "C" int mkxp_angle_x11_shim_XFree(void *data) {
    open_x11();
    return _xFree != nullptr ? _xFree(data) : 0;
}

extern "C" int mkxp_angle_x11_shim_XFreeColormap(Display *display, Colormap colormap) {
    open_x11();
    return _xFreeColormap != nullptr ? _xFreeColormap(display, colormap) : 0;
}

extern "C" int mkxp_angle_x11_shim_XGetErrorText(Display *display, int code, char *buffer_return, int length) {
    open_x11();
    return _xGetErrorText != nullptr ? _xGetErrorText(display, code, buffer_return, length) : 0;
}

extern "C" Status mkxp_angle_x11_shim_XGetGeometry(Display *display, Drawable d, Window *root_return, int *x_return, int *y_return, unsigned int *width_return, unsigned int *height_return, unsigned int *border_width_return, unsigned int *depth_return) {
    open_x11();
    return _xGetGeometry != nullptr ? _xGetGeometry(display, d, root_return, x_return, y_return, width_return, height_return, border_width_return, depth_return) : 0;
}

extern "C" XVisualInfo *mkxp_angle_x11_shim_XGetVisualInfo(Display *display, long vinfo_mask, XVisualInfo *vinfo_template, int *nitems_return) {
    open_x11();
    return _xGetVisualInfo != nullptr ? _xGetVisualInfo(display, vinfo_mask, vinfo_template, nitems_return) : nullptr;
}

extern "C" Status mkxp_angle_x11_shim_XGetWindowAttributes(Display *display, Window w, XWindowAttributes *window_attributes_return) {
    open_x11();
    return _xGetWindowAttributes != nullptr ? _xGetWindowAttributes(display, w, window_attributes_return) : 0;
}

extern "C" int mkxp_angle_x11_shim_XMapWindow(Display *display, Window w) {
    open_x11();
    return _xMapWindow != nullptr ? _xMapWindow(display, w) : 0;
}

extern "C" Display *mkxp_angle_x11_shim_XOpenDisplay(_Xconst char *display_name) {
    open_x11();
    return _xOpenDisplay != nullptr ? _xOpenDisplay(display_name) : nullptr;
}

extern "C" int mkxp_angle_x11_shim_XResizeWindow(Display *display, Window w, unsigned int width, unsigned int height) {
    open_x11();
    return _xResizeWindow != nullptr ? _xResizeWindow(display, w, width, height) : 0;
}

extern "C" XErrorHandler mkxp_angle_x11_shim_XSetErrorHandler(XErrorHandler handler) {
    open_x11();
    return _xSetErrorHandler != nullptr ? _xSetErrorHandler(handler) : nullptr;
}

extern "C" int mkxp_angle_x11_shim_XSync(Display *display, Bool discard) {
    open_x11();
    return _xSync != nullptr ? _xSync(display, discard) : 0;
}
