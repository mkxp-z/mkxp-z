// Host-side tests for switch/switch_touch.h (no libnx needed).
// Run: meson test -C build touch (switch/tests/touch/meson.build)

#include "switch_touch.h"

#include <cstdio>

using namespace PickerTouch;

static int failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

// The picker's layout: tiles 256 wide with 16 between, from x 96, y 210
static Layout layout(int count) { return {96, 210, 256, 272, 4, count}; }
static const int FOOTER = 646;

// Touch down at (x, y), move to each point, then release; returns the tap result
struct Finger {
    Gesture g;
    int first = 0, sel = 0;

    bool frame(const Layout &l, bool touching, int x = 0, int y = 0) {
        return g.update(l.scroll(), touching, x, y, FOOTER, first, sel);
    }
    bool frame(const ListLayout &l, bool touching, int x = 0, int y = 0) {
        return g.update(l.scroll(), touching, x, y, FOOTER, first, sel);
    }
};

// The Settings lists: rows 46 high from y 170, 9 on screen
static ListLayout list(int count) { return {170, 46, 9, count}; }

static void test_tile_hit() {
    Layout l = layout(8);
    CHECK(tileAt(l, 0, 96, 210) == 0);
    CHECK(tileAt(l, 0, 96 + 255, 210 + 255) == 0);
    CHECK(tileAt(l, 0, 96 + 260, 300) == -1); // gap
    CHECK(tileAt(l, 0, 96 + 272, 300) == 1);
    CHECK(tileAt(l, 3, 96 + 272, 300) == 4);  // scrolled
    CHECK(tileAt(l, 0, 50, 300) == -1);       // left of the row
    CHECK(tileAt(l, 0, 200, 100) == -1);      // above
    CHECK(tileAt(l, 0, 200, 470) == -1);      // below
    CHECK(tileAt(layout(2), 0, 96 + 2 * 272, 300) == -1); // past the last tile
}

static void test_tap() {
    Layout l = layout(8);
    Finger f;
    CHECK(!f.frame(l, true, 400, 300));
    CHECK(!f.frame(l, true, 410, 305)); // a little wobble is still a tap
    CHECK(f.frame(l, false));
    CHECK(f.g.x0 == 400 && f.g.y0 == 300);
    CHECK(!f.frame(l, false)); // reported once
    CHECK(f.first == 0 && f.sel == 0);
}

static void test_drag_scrolls_not_taps() {
    Layout l = layout(8);
    Finger f;
    f.frame(l, true, 900, 300);
    f.frame(l, true, 900 - 2 * 272, 300); // two tiles to the left
    CHECK(f.first == 2);
    CHECK(f.sel == 2); // kept on screen
    CHECK(!f.frame(l, false));

    // Dragging back to the start, past it
    f.frame(l, true, 200, 300);
    f.frame(l, true, 200 + 5 * 272, 300);
    CHECK(f.first == 0);
    CHECK(f.sel == 2); // still on screen, so unchanged
    f.frame(l, false);
}

static void test_drag_stops_at_the_end() {
    Layout l = layout(6); // two more than fit
    Finger f;
    f.frame(l, true, 1200, 300);
    f.frame(l, true, 1200 - 10 * 272, 300);
    CHECK(f.first == 2);
    f.frame(l, false);

    Layout few = layout(3); // all fit
    Finger g;
    g.frame(few, true, 1000, 300);
    g.frame(few, true, 100, 300);
    CHECK(g.first == 0);
    g.frame(few, false);
}

static void test_moved_but_not_dragged_is_not_a_tap() {
    Layout l = layout(8);
    Finger f;
    f.frame(l, true, 600, 680); // starts in the footer: no scrolling
    f.frame(l, true, 900, 680);
    CHECK(f.first == 0);
    CHECK(!f.frame(l, false));

    Finger g; // vertical swipe
    g.frame(l, true, 400, 250);
    g.frame(l, true, 400, 400);
    CHECK(!g.frame(l, false));
}

static void test_selection_follows_scroll() {
    Layout l = layout(10);
    Finger f;
    f.sel = 7;
    f.first = 4;
    f.frame(l, true, 300, 300);
    f.frame(l, true, 300 + 4 * 272, 300); // back to the first tiles
    CHECK(f.first == 0);
    CHECK(f.sel == 3); // last tile on screen
    f.frame(l, false);
}

static void test_row_hit() {
    ListLayout l = list(20);
    CHECK(rowAt(l, 0, 170) == 0);
    CHECK(rowAt(l, 0, 170 + 45) == 0);
    CHECK(rowAt(l, 0, 170 + 46) == 1);
    CHECK(rowAt(l, 5, 170 + 46) == 6);         // scrolled
    CHECK(rowAt(l, 0, 120) == -1);             // title area
    CHECK(rowAt(l, 0, 170 + 9 * 46) == -1);    // below the last visible row
    CHECK(rowAt(list(3), 0, 170 + 3 * 46) == -1); // past the end of a short list
    CHECK(rowAt(list(0), 0, 200) == -1);       // a screen with no list
}

static void test_list_drag() {
    ListLayout l = list(20);
    Finger f;
    f.frame(l, true, 600, 500);
    f.frame(l, true, 600, 500 - 3 * 46); // finger up: later rows
    CHECK(f.first == 3);
    CHECK(f.sel == 3);
    f.frame(l, true, 640, 500 - 3 * 46); // sideways wobble doesn't matter
    CHECK(f.first == 3);
    CHECK(!f.frame(l, false));

    f.frame(l, true, 600, 200);
    f.frame(l, true, 600, 200 + 50 * 46);
    CHECK(f.first == 0);
    f.frame(l, false);

    f.frame(l, true, 600, 600);
    f.frame(l, true, 600, 600 - 50 * 46);
    CHECK(f.first == 11); // 20 rows, 9 shown
    CHECK(f.sel == 11);
    f.frame(l, false);
}

static void test_list_tap_and_sideways_swipe() {
    ListLayout l = list(5);
    Finger f;
    f.frame(l, true, 300, 260);
    CHECK(f.frame(l, false)); // tap
    CHECK(rowAt(l, 0, f.g.y0) == 1);

    Finger g; // a sideways swipe over a list neither scrolls nor taps
    g.frame(l, true, 300, 260);
    g.frame(l, true, 700, 262);
    CHECK(g.first == 0);
    CHECK(!g.frame(l, false));

    Finger e; // an empty list never scrolls
    e.frame(list(0), true, 300, 400);
    e.frame(list(0), true, 300, 100);
    CHECK(e.first == 0 && e.sel == 0);
    e.frame(list(0), false);
}

int main() {
    test_row_hit();
    test_list_drag();
    test_list_tap_and_sideways_swipe();
    test_tile_hit();
    test_tap();
    test_drag_scrolls_not_taps();
    test_drag_stops_at_the_end();
    test_moved_but_not_dragged_is_not_a_tap();
    test_selection_follows_scroll();
    if (failures)
        printf("%d check(s) failed\n", failures);
    else
        printf("all touch checks passed\n");
    return failures ? 1 : 0;
}
