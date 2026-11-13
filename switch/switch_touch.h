#pragma once

// Touch input for the game picker: taps, and drags that scroll the tile row
// or a list. Kept free of libnx so it can be tested on the build machine
// (tests/touch).

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace PickerTouch {

// How far a finger may move, in pixels, and still count as a tap
const int SLOP = 24;

// What a drag scrolls: `visible` of `count` items, `step` pixels apart,
// sideways or up and down
struct Scroll {
    bool vertical;
    int step, visible, count;
};

// The picker's row of tiles
struct Layout {
    int rowX, tileY, tile; // left edge and top of the row, tile size
    int step;              // tile plus gap
    int visible;           // whole tiles on screen
    int count;             // tiles, Settings included

    Scroll scroll() const { return {false, step, visible, count}; }
};

// The tile under (x, y) with `first` as the leftmost tile, or -1
inline int tileAt(const Layout &l, int first, int x, int y) {
    if (y < l.tileY || y >= l.tileY + l.tile || x < l.rowX)
        return -1;
    if ((x - l.rowX) % l.step >= l.tile)
        return -1; // in the gap between two tiles
    int i = first + (x - l.rowX) / l.step;
    return i < l.count ? i : -1;
}

// A Settings list, whose rows span the screen
struct ListLayout {
    int listY, rowH; // top of the first row, row height
    int visible;     // rows on screen
    int count;       // rows in the list

    Scroll scroll() const { return {true, rowH, visible, count}; }
};

// The row at height y with `top` as the first row shown, or -1
inline int rowAt(const ListLayout &l, int top, int y) {
    if (y < l.listY)
        return -1;
    int i = (y - l.listY) / l.rowH;
    if (i >= l.visible)
        return -1;
    i += top;
    return i < l.count ? i : -1;
}

// One finger, tracked from touch down to release. A touch that moves more
// than SLOP along the scroll direction is a drag; one that doesn't is a tap,
// reported on release so a drag never starts anything.
struct Gesture {
    bool active = false, dragged = false;
    int x0 = 0, y0 = 0, x = 0, y = 0, first0 = 0;

    // Feeds one frame. touching, tx, ty: the first touch point, if any.
    // Drags only start above dragMaxY (the footer). While dragging, scrolls
    // first (the first item shown) and keeps sel on screen. Returns true on
    // the frame a tap ends; it was at (x0, y0).
    bool update(const Scroll &s, bool touching, int tx, int ty, int dragMaxY,
                int &first, int &sel) {
        if (!touching) {
            if (!active)
                return false;
            active = false;
            return !dragged && std::abs(x - x0) <= SLOP && std::abs(y - y0) <= SLOP;
        }
        if (!active) {
            active = true;
            dragged = false;
            x0 = tx;
            y0 = ty;
            first0 = first;
        }
        x = tx;
        y = ty;
        int d = s.vertical ? y - y0 : x - x0;
        if (!dragged && y0 < dragMaxY && std::abs(d) > SLOP)
            dragged = true;
        if (dragged && s.count > 0) {
            // Finger left or up shows later items, like the home menu
            int maxFirst = std::max(0, s.count - s.visible);
            int moved = (int)std::lround((double)d / s.step);
            first = std::max(0, std::min(maxFirst, first0 - moved));
            int last = std::min(s.count - 1, first + s.visible - 1);
            sel = std::max(first, std::min(last, sel));
        }
        return false;
    }
};

} // namespace PickerTouch
