#include <dlfcn.h>

#include <wayland-client-core.h>

static struct wl_display *(*_wl_display_connect)(const char *name) = nullptr;
static void (*_wl_display_disconnect)(struct wl_display *display) = nullptr;
static int (*_wl_display_get_error)(struct wl_display *display) = nullptr;
static struct wl_display *(*_wl_proxy_get_display)(struct wl_proxy *proxy) = nullptr;

static void open_wayland_client() {
    static bool wayland_client_loaded = false;
    if (wayland_client_loaded) {
        return;
    }
    wayland_client_loaded = true;
    void *wayland_client = nullptr;
    if ((wayland_client = dlopen(MKXP_ANGLE_SHIM_WAYLAND_CLIENT_SONAME, RTLD_LAZY)) == nullptr) {
        return;
    }
    if (
        (_wl_display_connect = reinterpret_cast<struct wl_display *(*)(const char *name)>(dlsym(wayland_client, "wl_display_connect"))) == nullptr
            || (_wl_display_disconnect = reinterpret_cast<void (*)(struct wl_display *display)>(dlsym(wayland_client, "wl_display_disconnect"))) == nullptr
            || (_wl_display_get_error = reinterpret_cast<int (*)(struct wl_display *display)>(dlsym(wayland_client, "wl_display_get_error"))) == nullptr
            || (_wl_proxy_get_display = reinterpret_cast<struct wl_display *(*)(struct wl_proxy *proxy)>(dlsym(wayland_client, "wl_proxy_get_display"))) == nullptr
    ) {
        dlclose(wayland_client);
        _wl_display_connect = nullptr;
        _wl_display_disconnect = nullptr;
        _wl_display_get_error = nullptr;
    }
}

extern "C" struct wl_display *mkxp_angle_wayland_client_shim_wl_display_connect(const char *name) {
    open_wayland_client();
    return _wl_display_connect != nullptr ? _wl_display_connect(name) : nullptr;
}

extern "C" void mkxp_angle_wayland_client_shim_wl_display_disconnect(struct wl_display *display) {
    open_wayland_client();
    if (_wl_display_disconnect != nullptr) {
        _wl_display_disconnect(display);
    }
}

extern "C" int mkxp_angle_wayland_client_shim_wl_display_get_error(struct wl_display *display) {
    open_wayland_client();
    return _wl_display_get_error != nullptr ? _wl_display_get_error(display) : -1;
}

extern "C" struct wl_display *mkxp_angle_wayland_client_shim_wl_proxy_get_display(struct wl_proxy *proxy) {
    open_wayland_client();
    return _wl_proxy_get_display != nullptr ? _wl_proxy_get_display(proxy) : nullptr;
}
