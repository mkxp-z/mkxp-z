#include <dlfcn.h>

#include <xcb/xcb.h>

static xcb_connection_t *(*_xcb_connect)(const char *displayname, int *screenp);
static int (*_xcb_connection_has_error)(xcb_connection_t *c);
static void (*_xcb_depth_next)(xcb_depth_iterator_t *i);
static xcb_visualtype_iterator_t (*_xcb_depth_visuals_iterator)(const xcb_depth_t *R);
static void (*_xcb_disconnect)(xcb_connection_t *c);
static xcb_get_geometry_cookie_t (*_xcb_get_geometry)(xcb_connection_t *c, xcb_drawable_t drawable);
static xcb_get_geometry_reply_t *(*_xcb_get_geometry_reply)(xcb_connection_t *c, xcb_get_geometry_cookie_t cookie, xcb_generic_error_t **e);
static xcb_get_input_focus_cookie_t (*_xcb_get_input_focus)(xcb_connection_t *c);
static xcb_get_input_focus_reply_t *(*_xcb_get_input_focus_reply)(xcb_connection_t *c, xcb_get_input_focus_cookie_t cookie, xcb_generic_error_t **e);
static const struct xcb_setup_t *(*_xcb_get_setup)(xcb_connection_t *c);
static xcb_get_window_attributes_cookie_t (*_xcb_get_window_attributes)(xcb_connection_t *c, xcb_window_t window);
static xcb_get_window_attributes_reply_t *(*_xcb_get_window_attributes_reply)(xcb_connection_t *c, xcb_get_window_attributes_cookie_t cookie, xcb_generic_error_t **e);
static xcb_query_tree_cookie_t (*_xcb_query_tree)(xcb_connection_t *c, xcb_window_t window);
static xcb_query_tree_reply_t *(*_xcb_query_tree_reply)(xcb_connection_t *c, xcb_query_tree_cookie_t cookie, xcb_generic_error_t **e);
static xcb_depth_iterator_t (*_xcb_screen_allowed_depths_iterator)(const xcb_screen_t *R);
static xcb_screen_iterator_t (*_xcb_setup_roots_iterator)(const xcb_setup_t *R);
static void (*_xcb_visualtype_next)(xcb_visualtype_iterator_t *i);

static void open_xcb() {
    static bool xcb_loaded = false;
    if (xcb_loaded) {
        return;
    }
    xcb_loaded = true;
    void *xcb = nullptr;
    if ((xcb = dlopen(MKXP_ANGLE_SHIM_XCB_SONAME, RTLD_LAZY)) == nullptr) {
        return;
    }
    if (
        (_xcb_connect = reinterpret_cast<xcb_connection_t *(*)(const char *displayname, int *screenp)>(dlsym(xcb, "xcb_connect"))) == nullptr
            || (_xcb_connection_has_error = reinterpret_cast<int (*)(xcb_connection_t *c)>(dlsym(xcb, "xcb_connection_has_error"))) == nullptr
            || (_xcb_depth_next = reinterpret_cast<void (*)(xcb_depth_iterator_t *i)>(dlsym(xcb, "xcb_depth_next"))) == nullptr
            || (_xcb_depth_visuals_iterator = reinterpret_cast<xcb_visualtype_iterator_t (*)(const xcb_depth_t *R)>(dlsym(xcb, "xcb_depth_visuals_iterator"))) == nullptr
            || (_xcb_disconnect = reinterpret_cast<void (*)(xcb_connection_t *c)>(dlsym(xcb, "xcb_disconnect"))) == nullptr
            || (_xcb_get_geometry = reinterpret_cast<xcb_get_geometry_cookie_t (*)(xcb_connection_t *c, xcb_drawable_t drawable)>(dlsym(xcb, "xcb_get_geometry"))) == nullptr
            || (_xcb_get_geometry_reply = reinterpret_cast<xcb_get_geometry_reply_t *(*)(xcb_connection_t *c, xcb_get_geometry_cookie_t cookie, xcb_generic_error_t **e)>(dlsym(xcb, "xcb_get_geometry_reply"))) == nullptr
            || (_xcb_get_input_focus = reinterpret_cast<xcb_get_input_focus_cookie_t (*)(xcb_connection_t *c)>(dlsym(xcb, "xcb_get_input_focus"))) == nullptr
            || (_xcb_get_input_focus_reply = reinterpret_cast<xcb_get_input_focus_reply_t *(*)(xcb_connection_t *c, xcb_get_input_focus_cookie_t cookie, xcb_generic_error_t **e)>(dlsym(xcb, "xcb_get_input_focus_reply"))) == nullptr
            || (_xcb_get_setup = reinterpret_cast<const struct xcb_setup_t *(*)(xcb_connection_t *c)>(dlsym(xcb, "xcb_get_setup"))) == nullptr
            || (_xcb_get_window_attributes = reinterpret_cast<xcb_get_window_attributes_cookie_t (*)(xcb_connection_t *c, xcb_window_t window)>(dlsym(xcb, "xcb_get_window_attributes"))) == nullptr
            || (_xcb_get_window_attributes_reply = reinterpret_cast<xcb_get_window_attributes_reply_t *(*)(xcb_connection_t *c, xcb_get_window_attributes_cookie_t cookie, xcb_generic_error_t **e)>(dlsym(xcb, "xcb_get_window_attributes_reply"))) == nullptr
            || (_xcb_query_tree = reinterpret_cast<xcb_query_tree_cookie_t (*)(xcb_connection_t *c, xcb_window_t window)>(dlsym(xcb, "xcb_query_tree"))) == nullptr
            || (_xcb_query_tree_reply = reinterpret_cast<xcb_query_tree_reply_t *(*)(xcb_connection_t *c, xcb_query_tree_cookie_t cookie, xcb_generic_error_t **e)>(dlsym(xcb, "xcb_query_tree_reply"))) == nullptr
            || (_xcb_screen_allowed_depths_iterator = reinterpret_cast<xcb_depth_iterator_t (*)(const xcb_screen_t *R)>(dlsym(xcb, "xcb_screen_allowed_depths_iterator"))) == nullptr
            || (_xcb_setup_roots_iterator = reinterpret_cast<xcb_screen_iterator_t (*)(const xcb_setup_t *R)>(dlsym(xcb, "xcb_setup_roots_iterator"))) == nullptr
            || (_xcb_visualtype_next = reinterpret_cast<void (*)(xcb_visualtype_iterator_t *i)>(dlsym(xcb, "xcb_visualtype_next"))) == nullptr
    ) {
        dlclose(xcb);
        _xcb_connect = nullptr;
        _xcb_connection_has_error = nullptr;
        _xcb_depth_next = nullptr;
        _xcb_depth_visuals_iterator = nullptr;
        _xcb_disconnect = nullptr;
        _xcb_get_geometry = nullptr;
        _xcb_get_geometry_reply = nullptr;
        _xcb_get_input_focus = nullptr;
        _xcb_get_input_focus_reply = nullptr;
        _xcb_get_setup = nullptr;
        _xcb_get_window_attributes = nullptr;
        _xcb_get_window_attributes_reply = nullptr;
        _xcb_query_tree = nullptr;
        _xcb_query_tree_reply = nullptr;
        _xcb_screen_allowed_depths_iterator = nullptr;
        _xcb_setup_roots_iterator = nullptr;
        _xcb_visualtype_next = nullptr;
    }
}

extern "C" xcb_connection_t *mkxp_angle_xcb_shim_xcb_connect(const char *displayname, int *screenp) {
    open_xcb();
    return _xcb_connect != nullptr ? _xcb_connect(displayname, screenp) : nullptr;
}

extern "C" int mkxp_angle_xcb_shim_xcb_connection_has_error(xcb_connection_t *c) {
    open_xcb();
    return _xcb_connection_has_error != nullptr ? _xcb_connection_has_error(c) : XCB_CONN_ERROR;
}

extern "C" void mkxp_angle_xcb_shim_xcb_depth_next(xcb_depth_iterator_t *i) {
    open_xcb();
    if (_xcb_depth_next != nullptr) {
        _xcb_depth_next(i);
    }
}

extern "C" xcb_visualtype_iterator_t mkxp_angle_xcb_shim_xcb_depth_visuals_iterator(const xcb_depth_t *R) {
    open_xcb();
    return _xcb_depth_visuals_iterator != nullptr ? _xcb_depth_visuals_iterator(R) : xcb_visualtype_iterator_t();
}

extern "C" void mkxp_angle_xcb_shim_xcb_disconnect(xcb_connection_t *c) {
    open_xcb();
    if (_xcb_disconnect != nullptr) {
        _xcb_disconnect(c);
    }
}

extern "C" xcb_get_geometry_cookie_t mkxp_angle_xcb_shim_xcb_get_geometry(xcb_connection_t *c, xcb_drawable_t drawable) {
    open_xcb();
    return _xcb_get_geometry != nullptr ? _xcb_get_geometry(c, drawable) : xcb_get_geometry_cookie_t();
}

extern "C" xcb_get_geometry_reply_t *mkxp_angle_xcb_shim_xcb_get_geometry_reply(xcb_connection_t *c, xcb_get_geometry_cookie_t cookie, xcb_generic_error_t **e) {
    open_xcb();
    return _xcb_get_geometry_reply != nullptr ? _xcb_get_geometry_reply(c, cookie, e) : nullptr;
}

extern "C" xcb_get_input_focus_cookie_t mkxp_angle_xcb_shim_xcb_get_input_focus(xcb_connection_t *c) {
    open_xcb();
    return _xcb_get_input_focus != nullptr ? _xcb_get_input_focus(c) : xcb_get_input_focus_cookie_t();
}

extern "C" xcb_get_input_focus_reply_t *mkxp_angle_xcb_shim_xcb_get_input_focus_reply(xcb_connection_t *c, xcb_get_input_focus_cookie_t cookie, xcb_generic_error_t **e) {
    open_xcb();
    return _xcb_get_input_focus_reply != nullptr ? _xcb_get_input_focus_reply(c, cookie, e) : nullptr;
}

extern "C" const struct xcb_setup_t *mkxp_angle_xcb_shim_xcb_get_setup(xcb_connection_t *c) {
    open_xcb();
    return _xcb_get_setup != nullptr ? _xcb_get_setup(c) : nullptr;
}

extern "C" xcb_get_window_attributes_cookie_t mkxp_angle_xcb_shim_xcb_get_window_attributes(xcb_connection_t *c, xcb_window_t window) {
    open_xcb();
    return _xcb_get_window_attributes != nullptr ? _xcb_get_window_attributes(c, window) : xcb_get_window_attributes_cookie_t();
}

extern "C" xcb_get_window_attributes_reply_t *mkxp_angle_xcb_shim_xcb_get_window_attributes_reply(xcb_connection_t *c, xcb_get_window_attributes_cookie_t cookie, xcb_generic_error_t **e) {
    open_xcb();
    return _xcb_get_window_attributes_reply != nullptr ? _xcb_get_window_attributes_reply(c, cookie, e) : nullptr;
}

extern "C" xcb_query_tree_cookie_t mkxp_angle_xcb_shim_xcb_query_tree(xcb_connection_t *c, xcb_window_t window) {
    open_xcb();
    return _xcb_query_tree != nullptr ? _xcb_query_tree(c, window) : xcb_query_tree_cookie_t();
}

extern "C" xcb_query_tree_reply_t *mkxp_angle_xcb_shim_xcb_query_tree_reply(xcb_connection_t *c, xcb_query_tree_cookie_t cookie, xcb_generic_error_t **e) {
    open_xcb();
    return _xcb_query_tree_reply != nullptr ? _xcb_query_tree_reply(c, cookie, e) : nullptr;
}

extern "C" xcb_depth_iterator_t mkxp_angle_xcb_shim_xcb_screen_allowed_depths_iterator(const xcb_screen_t *R) {
    open_xcb();
    return _xcb_screen_allowed_depths_iterator != nullptr ? _xcb_screen_allowed_depths_iterator(R) : xcb_depth_iterator_t();
}

extern "C" xcb_screen_iterator_t mkxp_angle_xcb_shim_xcb_setup_roots_iterator(const xcb_setup_t *R) {
    open_xcb();
    return _xcb_setup_roots_iterator != nullptr ? _xcb_setup_roots_iterator(R) : xcb_screen_iterator_t();
}

extern "C" void mkxp_angle_xcb_shim_xcb_visualtype_next(xcb_visualtype_iterator_t *i) {
    open_xcb();
    if (_xcb_visualtype_next != nullptr) {
        _xcb_visualtype_next(i);
    }
}
