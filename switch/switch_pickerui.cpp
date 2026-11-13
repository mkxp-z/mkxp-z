#ifdef __SWITCH__

// Avoid pulling in Ruby's assert.h from libnx service.h (see switch_main.cpp)
#define RUBY_ASSERT_H 1

#include <switch.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <algorithm>

#include "switch_pickerui.h"
#include "switch_controls.h"
#include "switch_touch.h"

// switch_swkbd.cpp
extern "C" int switch_swkbd_get_text(char *out, size_t outSize);

// Draws a Switch home-screen look-alike with SDL_ttf/SDL_image onto a plain
// surface and copies it into a libnx framebuffer, so SDL video stays
// untouched until mkxp-z initializes it.

namespace {

const int SCREEN_W = 1280, SCREEN_H = 720;
const int TILE = 256, TILE_GAP = 16, TILE_RADIUS = 6;
const int ROW_X = 96, TILE_Y = 210;
const int VISIBLE_TILES = (SCREEN_W - ROW_X) / (TILE + TILE_GAP);
const int FOOTER_Y = 646;

struct Rgb { Uint8 r, g, b; };
const Rgb BG = {45, 45, 45};
const Rgb TEXT = {255, 255, 255};
const Rgb TEXT_DIM = {170, 170, 170};
const Rgb LINE = {90, 90, 90};
const Rgb TILE_BG = {70, 70, 70};
const Rgb ACCENT = {0, 195, 227};
const Rgb ACCENT_HI = {130, 235, 250};
const Rgb WARN = {255, 190, 40};
const Rgb WARN_BG = {72, 54, 18};

struct Fonts {
    TTF_Font *small = NULL, *medium = NULL, *title = NULL, *initials = NULL;
};

Uint32 mapRgb(SDL_Surface *s, Rgb c) {
    return SDL_MapRGBA(s->format, c.r, c.g, c.b, 255);
}

Rgb lerp(Rgb a, Rgb b, double t) {
    return {(Uint8)(a.r + (b.r - a.r) * t), (Uint8)(a.g + (b.g - a.g) * t),
            (Uint8)(a.b + (b.b - a.b) * t)};
}

// How far row y of an h-tall rounded rect is inset by its corners
int cornerInset(int y, int h, int rad) {
    int dy = 0;
    if (y < rad)
        dy = rad - y;
    else if (y >= h - rad)
        dy = y - (h - rad) + 1;
    if (dy <= 0)
        return 0;
    double d = dy - 0.5;
    return (int)(rad - sqrt((double)rad * rad - d * d) + 0.5);
}

void fillRoundRect(SDL_Surface *s, SDL_Rect r, int rad, Uint32 color) {
    for (int y = 0; y < r.h; ++y) {
        int inset = cornerInset(y, r.h, rad);
        SDL_Rect row = {r.x + inset, r.y + y, r.w - 2 * inset, 1};
        SDL_FillRect(s, &row, color);
    }
}

// Makes the corners of a 32-bit surface with alpha transparent
void roundCorners(SDL_Surface *s, int rad) {
    SDL_LockSurface(s);
    for (int y = 0; y < s->h; ++y) {
        int inset = cornerInset(y, s->h, rad);
        Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
        for (int x = 0; x < inset; ++x)
            row[x] = row[s->w - 1 - x] = 0;
    }
    SDL_UnlockSurface(s);
}

// Shortens s with "..." until it fits in maxW pixels
std::string fitText(TTF_Font *f, std::string s, int maxW) {
    int w = 0;
    if (TTF_SizeUTF8(f, s.c_str(), &w, NULL) != 0 || w <= maxW)
        return s;
    while (!s.empty()) {
        do
            s.pop_back(); // drop a whole UTF-8 sequence
        while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80);
        if (!s.empty())
            s.pop_back();
        std::string t = s + "...";
        if (TTF_SizeUTF8(f, t.c_str(), &w, NULL) == 0 && w <= maxW)
            return t;
    }
    return s;
}

enum Align { LEFT, CENTER, RIGHT };

// Returns the drawn width, 0 if nothing could be rendered
int drawText(SDL_Surface *dst, TTF_Font *f, const std::string &s, Rgb c,
             int x, int y, Align align) {
    SDL_Color col = {c.r, c.g, c.b, 255};
    SDL_Surface *t = TTF_RenderUTF8_Blended(f, s.c_str(), col);
    if (!t)
        return 0;
    SDL_Rect r = {x - (align == CENTER ? t->w / 2 : align == RIGHT ? t->w : 0), y, t->w, t->h};
    SDL_BlitSurface(t, NULL, dst, &r);
    int w = t->w;
    SDL_FreeSurface(t);
    return w;
}

// ---- Thumbnails ----

// <dir>/<base>.<image ext>, any capitalization, or ""
std::string findImage(const std::string &dir, const std::string &base) {
    static const char *exts[] = {"png", "jpg", "jpeg", "bmp", NULL};
    DIR *d = opendir(dir.c_str());
    if (!d)
        return "";
    std::string found;
    while (struct dirent *e = readdir(d)) {
        const char *n = e->d_name;
        if (strncasecmp(n, base.c_str(), base.size()) != 0 || n[base.size()] != '.')
            continue;
        for (const char **ext = exts; *ext && found.empty(); ++ext)
            if (strcasecmp(n + base.size() + 1, *ext) == 0)
                found = dir + "/" + n;
        if (!found.empty())
            break;
    }
    closedir(d);
    return found;
}

// Ruby Marshal integer at p (lengths only), or -1
long marshalInt(const std::string &d, size_t &p) {
    if (p >= d.size())
        return -1;
    signed char c = d[p++];
    if (c == 0)
        return 0;
    if (c > 4)
        return c - 5;
    if (c < 0)
        return -1;
    long n = 0;
    for (int i = 0; i < c && p < d.size(); ++i)
        n |= (long)(unsigned char)d[p++] << (8 * i);
    return n;
}

// Title screen image name from Data/System: @title_name (XP) or
// @title1_name (VX Ace), a Marshal String optionally wrapped in 'I'.
std::string systemTitleName(const std::string &dir) {
    static const char *files[] = {"Data/System.rxdata", "Data/System.rvdata",
                                  "Data/System.rvdata2", NULL};
    for (const char **file = files; *file; ++file) {
        FILE *f = fopen((dir + "/" + *file).c_str(), "rb");
        if (!f)
            continue;
        std::string data;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0 && data.size() < (4 << 20))
            data.append(buf, n);
        fclose(f);

        for (const char *key : {"@title1_name", "@title_name"}) {
            size_t p = data.find(key);
            if (p == std::string::npos)
                continue;
            p += strlen(key);
            if (p < data.size() && data[p] == 'I')
                ++p;
            if (p >= data.size() || data[p++] != '"')
                continue;
            long len = marshalInt(data, p);
            if (len > 0 && p + len <= data.size())
                return data.substr(p, len);
        }
    }
    return "";
}

// cover/icon/thumbnail image in the game folder, else the title screen
std::string findThumbnail(const std::string &dir) {
    for (const char *base : {"cover", "icon", "thumbnail"}) {
        std::string f = findImage(dir, base);
        if (!f.empty())
            return f;
    }
    static const char *titleDirs[] = {"Graphics/Titles", "Graphics/Titles1", NULL};
    std::string sysName = systemTitleName(dir);
    for (const std::string &name : {std::string("title"), sysName}) {
        if (name.empty())
            continue;
        for (const char **td = titleDirs; *td; ++td) {
            std::string f = findImage(dir + "/" + *td, name);
            if (!f.empty())
                return f;
        }
    }
    return "";
}

std::string initialsOf(const std::string &name) {
    std::string out;
    bool wordStart = true;
    for (char c : name) {
        if (c == ' ' || c == '_' || c == '-') {
            wordStart = true;
        } else if (wordStart && isalnum((unsigned char)c) && out.size() < 2) {
            out += (char)toupper((unsigned char)c);
            wordStart = false;
        } else {
            wordStart = false;
        }
    }
    return out.empty() ? "?" : out;
}

SDL_Surface *makeThumb(const PickerGame &g, const Fonts &fonts) {
    SDL_Surface *t = SDL_CreateRGBSurfaceWithFormat(0, TILE, TILE, 32, SDL_PIXELFORMAT_RGBA32);
    if (!t)
        return NULL;

    std::string path = findThumbnail(g.dir);
    SDL_Surface *img = path.empty() ? NULL : IMG_Load(path.c_str());
    if (img) {
        SDL_Surface *conv = SDL_ConvertSurfaceFormat(img, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(img);
        SDL_FillRect(t, NULL, mapRgb(t, TILE_BG));
        if (conv) {
            // Fill the square tile with the image's center
            int side = std::min(conv->w, conv->h);
            SDL_Rect src = {(conv->w - side) / 2, (conv->h - side) / 2, side, side};
            SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_BLEND);
            SDL_BlitScaled(conv, &src, t, NULL);
            SDL_FreeSurface(conv);
        }
    } else {
        unsigned h = 5381;
        for (char c : g.name)
            h = h * 33 + (unsigned char)c;
        Rgb c = {(Uint8)(60 + h % 110), (Uint8)(60 + (h >> 8) % 110), (Uint8)(60 + (h >> 16) % 110)};
        SDL_FillRect(t, NULL, mapRgb(t, c));
        drawText(t, fonts.initials, initialsOf(g.name), TEXT, TILE / 2,
                 (TILE - TTF_FontHeight(fonts.initials)) / 2, CENTER);
    }
    roundCorners(t, TILE_RADIUS);
    return t;
}

// ---- Frame ----

void drawButtonGlyph(SDL_Surface *c, TTF_Font *f, const char *label, int cx, int cy) {
    const int r = 14;
    fillRoundRect(c, {cx - r, cy - r, 2 * r, 2 * r}, r, mapRgb(c, TEXT));
    drawText(c, f, label, BG, cx, cy - TTF_FontHeight(f) / 2, CENTER);
}

// The footer area "(glyph) text" ending at right covers, from the glyph's
// left edge; also where it can be tapped
SDL_Rect hintRect(const Fonts &fonts, const char *text, int right) {
    int w = 0;
    TTF_SizeUTF8(fonts.small, text, &w, NULL);
    int left = right - w - 24 - 14;
    return {left, FOOTER_Y, right - left, SCREEN_H - FOOTER_Y};
}

// Draws "(glyph) text" ending at right; returns where the next hint ends
int drawHint(SDL_Surface *c, const Fonts &fonts, const char *glyph, const char *text, int right) {
    SDL_Rect r = hintRect(fonts, text, right);
    int textX = r.x + 24 + 14;
    drawText(c, fonts.small, text, TEXT, textX, FOOTER_Y + 22, LEFT);
    drawButtonGlyph(c, fonts.small, glyph, textX - 24, FOOTER_Y + 22 + TTF_FontHeight(fonts.small) / 2);
    return r.x - 36;
}

// Taps get some slack around footer hints
bool hitHint(const SDL_Rect &r, int x, int y) {
    const int pad = 12;
    return x >= r.x - pad && x < r.x + r.w + pad && y >= r.y && y < r.y + r.h;
}

void drawStatus(SDL_Surface *c, const Fonts &fonts, int battery) {
    int right = SCREEN_W - 60;
    if (battery >= 0) {
        // Battery outline with level, then the percentage to its left
        SDL_Rect body = {right - 40, 40, 36, 18};
        fillRoundRect(c, body, 3, mapRgb(c, TEXT));
        SDL_Rect inner = {body.x + 2, body.y + 2, body.w - 4, body.h - 4};
        SDL_FillRect(c, &inner, mapRgb(c, BG));
        SDL_Rect level = {inner.x + 1, inner.y + 1, (inner.w - 2) * battery / 100, inner.h - 2};
        SDL_FillRect(c, &level, mapRgb(c, battery <= 15 ? Rgb{230, 60, 60} : TEXT));
        SDL_Rect nub = {body.x + body.w, body.y + 5, 3, 8};
        SDL_FillRect(c, &nub, mapRgb(c, TEXT));
        right -= 52;
        char pct[16];
        snprintf(pct, sizeof(pct), "%d%%", battery);
        right -= drawText(c, fonts.small, pct, TEXT, right, 34, RIGHT) + 24;
    }
    // Console's local time (newlib's localtime() does not know its time zone)
    char clock[16];
    u64 now;
    TimeCalendarTime cal;
    TimeCalendarAdditionalInfo info;
    if (R_SUCCEEDED(timeGetCurrentTime(TimeType_UserSystemClock, &now)) &&
        R_SUCCEEDED(timeToCalendarTimeWithMyRule(now, &cal, &info))) {
        snprintf(clock, sizeof(clock), "%02d:%02d", cal.hour, cal.minute);
    } else {
        time_t t = time(NULL);
        strftime(clock, sizeof(clock), "%H:%M", localtime(&t));
    }
    drawText(c, fonts.medium, clock, TEXT, right, 30, RIGHT);
}

// tiles are the games plus the Settings tile, which is last
void drawFrame(SDL_Surface *c, const Fonts &fonts, const std::vector<PickerGame> &games,
               const std::vector<SDL_Surface *> &thumbs, int sel, int first,
               double t, int battery, const MemoryWarning &warn) {
    bool settings = sel == (int)games.size() - 1;
    SDL_FillRect(c, NULL, mapRgb(c, BG));

    drawText(c, fonts.medium, "mkxp-z", TEXT, 60, 30, LEFT);
    drawStatus(c, fonts, battery);

    for (int i = first; i < (int)games.size(); ++i) {
        int x = ROW_X + (i - first) * (TILE + TILE_GAP);
        if (x >= SCREEN_W)
            break;
        if (i == sel) {
            // Pulsing cursor with a thin gap, like the home menu
            Rgb a = lerp(ACCENT, ACCENT_HI, 0.5 + 0.5 * sin(t * 4.0));
            fillRoundRect(c, {x - 8, TILE_Y - 8, TILE + 16, TILE + 16}, TILE_RADIUS + 6, mapRgb(c, a));
            fillRoundRect(c, {x - 3, TILE_Y - 3, TILE + 6, TILE + 6}, TILE_RADIUS + 2, mapRgb(c, BG));
            std::string label = fitText(fonts.title, games[i].name, SCREEN_W - x - 60);
            drawText(c, fonts.title, label, ACCENT, x, TILE_Y - 64, LEFT);
        }
        SDL_Rect r = {x, TILE_Y, TILE, TILE};
        if (thumbs[i])
            SDL_BlitSurface(thumbs[i], NULL, c, &r);
        else
            fillRoundRect(c, r, TILE_RADIUS, mapRgb(c, TILE_BG));
    }

    // Between the tiles and the footer
    if (warn.shown()) {
        fillRoundRect(c, {60, 500, SCREEN_W - 120, 100}, TILE_RADIUS, mapRgb(c, WARN_BG));
        drawText(c, fonts.medium, fitText(fonts.medium, warn.title, SCREEN_W - 168), WARN, 84, 514, LEFT);
        drawText(c, fonts.small, fitText(fonts.small, warn.hint, SCREEN_W - 168), TEXT, 84, 558, LEFT);
    }

    SDL_Rect line = {30, FOOTER_Y, SCREEN_W - 60, 1};
    SDL_FillRect(c, &line, mapRgb(c, LINE));
    char count[32];
    snprintf(count, sizeof(count), "%d / %d", sel + 1, (int)games.size());
    drawText(c, fonts.small, count, TEXT_DIM, 60, FOOTER_Y + 22, LEFT);
    int right = drawHint(c, fonts, "B", "Exit to hbmenu", SCREEN_W - 60);
    drawHint(c, fonts, "A", settings ? "Open" : "Start", right);
}

void present(Framebuffer &fb, SDL_Surface *c) {
    u32 stride;
    u8 *dst = (u8 *)framebufferBegin(&fb, &stride);
    for (int y = 0; y < SCREEN_H; ++y)
        memcpy(dst + y * stride, (u8 *)c->pixels + y * c->pitch, SCREEN_W * 4);
    framebufferEnd(&fb);
}

TTF_Font *openSharedFont(const PlFontData &font, int size) {
    return TTF_OpenFontRW(SDL_RWFromConstMem(font.address, font.size), 1, size);
}

// ---- Settings: controller profiles (switch_controls.h) ----

using namespace SwitchControls;

// A gear drawn per pixel, for the Settings tile
SDL_Surface *makeGearThumb() {
    SDL_Surface *t = SDL_CreateRGBSurfaceWithFormat(0, TILE, TILE, 32, SDL_PIXELFORMAT_RGBA32);
    if (!t)
        return NULL;
    SDL_FillRect(t, NULL, mapRgb(t, TILE_BG));
    const double cx = TILE / 2.0, cy = TILE / 2.0;
    const double body = 62, tooth = 84, hole = 26, teeth = 8, pi = 3.14159265358979;
    Uint32 color = mapRgb(t, TEXT_DIM);
    SDL_LockSurface(t);
    for (int y = 0; y < TILE; ++y) {
        Uint32 *row = (Uint32 *)((Uint8 *)t->pixels + y * t->pitch);
        for (int x = 0; x < TILE; ++x) {
            double dx = x - cx, dy = y - cy, rho = sqrt(dx * dx + dy * dy);
            double phase = fmod((atan2(dy, dx) + pi) * teeth / (2 * pi), 1.0);
            bool solid = rho < body || (rho < tooth && phase > 0.25 && phase < 0.75);
            if (solid && rho > hole)
                row[x] = color;
        }
    }
    SDL_UnlockSurface(t);
    roundCorners(t, TILE_RADIUS);
    return t;
}

// Shared state of the settings screens, which draw onto the picker's canvas
struct Ui {
    Framebuffer &fb;
    SDL_Surface *c;
    const Fonts &fonts;
    PadState &pad;
    bool psm;
    int battery;
    std::string toast;
    u64 toastUntil;

    // Touch. drawScreen records the list and footer hints it drew; nextFrame
    // turns a tap on the selected row or a hint into that button press, and
    // leaves a tapped row or a drag's scroll for ListNav to apply.
    struct HintArea {
        SDL_Rect rect;
        u64 button;
    };
    int listCount = 0, listTop = 0, listSel = 0;
    std::vector<HintArea> hints;
    PickerTouch::Gesture touch;
    int touchSel = -1, touchTop = -1;
};

struct Row {
    std::string left, right;
    bool dim, accent; // greyed-out row; right text in the accent color
};

struct Hint {
    const char *glyph, *text;
};

const int LIST_Y = 170, ROW_H = 46;
const int LIST_ROWS_VISIBLE = (FOOTER_Y - 30 - LIST_Y) / ROW_H;

PickerTouch::ListLayout listLayout(const Ui &ui) {
    return {LIST_Y, ROW_H, LIST_ROWS_VISIBLE, ui.listCount};
}

// Polls input for one frame; false when the app is asked to exit
bool nextFrame(Ui &ui, u64 &down, u64 &held, u64 &now) {
    if (!appletMainLoop())
        return false;
    padUpdate(&ui.pad);
    down = padGetButtonsDown(&ui.pad);
    held = padGetButtons(&ui.pad);
    now = armTicksToNs(armGetSystemTick());

    ui.touchSel = ui.touchTop = -1;
    HidTouchScreenState ts = {};
    bool touching = hidGetTouchScreenStates(&ts, 1) > 0 && ts.count > 0;
    PickerTouch::ListLayout list = listLayout(ui);
    int top = ui.listTop, sel = ui.listSel;
    bool tapped = ui.touch.update(list.scroll(), touching, touching ? (int)ts.touches[0].x : 0,
                                  touching ? (int)ts.touches[0].y : 0, FOOTER_Y, top, sel);
    if (tapped) {
        int row = PickerTouch::rowAt(list, ui.listTop, ui.touch.y0);
        if (row >= 0 && row == ui.listSel) {
            down |= HidNpadButton_A;
        } else if (row >= 0) {
            ui.touchSel = row;
        } else {
            for (const Ui::HintArea &h : ui.hints)
                if (hitHint(h.rect, ui.touch.x0, ui.touch.y0)) {
                    down |= h.button;
                    break;
                }
        }
    } else if (ui.touch.active && ui.touch.dragged) {
        ui.touchTop = top;
        ui.touchSel = sel;
    }
    u32 pct;
    if (ui.psm && R_SUCCEEDED(psmGetBatteryChargePercentage(&pct)))
        ui.battery = (int)pct;
    return true;
}

void showToast(Ui &ui, const std::string &msg) {
    ui.toast = msg;
    ui.toastUntil = armTicksToNs(armGetSystemTick()) + 2500000000ULL;
}

// Up/down with repeat while held, and touch, keeping sel within count and
// visible
struct ListNav {
    int sel = 0, top = 0;
    u64 heldSince = 0, lastRepeat = 0;

    void update(const Ui &ui, u64 down, u64 held, u64 now, int count) {
        if (ui.touchTop >= 0)
            top = ui.touchTop;
        if (ui.touchSel >= 0 && ui.touchSel < count)
            sel = ui.touchSel;
        int step = 0;
        u64 dirs = HidNpadButton_AnyUp | HidNpadButton_AnyDown;
        if (down & dirs) {
            heldSince = lastRepeat = now;
            step = (down & HidNpadButton_AnyUp) ? -1 : 1;
        } else if ((held & dirs) && now - heldSince > 400000000ULL &&
                   now - lastRepeat > 80000000ULL) {
            lastRepeat = now;
            step = (held & HidNpadButton_AnyUp) ? -1 : 1;
        }
        sel = std::max(0, std::min(count - 1, sel + step));
        keepVisible();
    }

    void keepVisible() {
        if (sel < top)
            top = sel;
        else if (sel >= top + LIST_ROWS_VISIBLE)
            top = sel - LIST_ROWS_VISIBLE + 1;
    }
};

// The button a footer hint's glyph stands for, so tapping the hint presses it
u64 hintButton(const char *glyph) {
    switch (glyph[0]) {
    case 'A': return HidNpadButton_A;
    case 'B': return HidNpadButton_B;
    case 'X': return HidNpadButton_X;
    case 'Y': return HidNpadButton_Y;
    default: return 0;
    }
}

void drawScreen(Ui &ui, const std::string &title, const std::string &subtitle,
                const std::vector<Row> &rows, int sel, int top,
                const std::vector<Hint> &hints) {
    SDL_Surface *c = ui.c;
    SDL_FillRect(c, NULL, mapRgb(c, BG));
    drawText(c, ui.fonts.medium, "mkxp-z", TEXT, 60, 30, LEFT);
    drawStatus(c, ui.fonts, ui.battery);
    drawText(c, ui.fonts.title, fitText(ui.fonts.title, title, SCREEN_W - 120), ACCENT, 60, 84, LEFT);
    if (!subtitle.empty())
        drawText(c, ui.fonts.small, fitText(ui.fonts.small, subtitle, SCREEN_W - 120),
                 TEXT_DIM, 60, 130, LEFT);

    for (int i = top; i < (int)rows.size() && i < top + LIST_ROWS_VISIBLE; ++i) {
        int y = LIST_Y + (i - top) * ROW_H;
        if (i == sel) {
            fillRoundRect(c, {50, y - 2, SCREEN_W - 100, ROW_H - 4}, 6, mapRgb(c, ACCENT));
            fillRoundRect(c, {53, y + 1, SCREEN_W - 106, ROW_H - 10}, 4, mapRgb(c, TILE_BG));
        }
        const Row &r = rows[i];
        int textY = y + (ROW_H - 4 - TTF_FontHeight(ui.fonts.medium)) / 2;
        drawText(c, ui.fonts.medium, fitText(ui.fonts.medium, r.left, 560), r.dim ? TEXT_DIM : TEXT,
                 74, textY, LEFT);
        drawText(c, ui.fonts.medium, fitText(ui.fonts.medium, r.right, 560),
                 r.accent ? ACCENT : TEXT_DIM, SCREEN_W - 74, textY, RIGHT);
    }
    if (top > 0)
        drawText(c, ui.fonts.small, "more above", TEXT_DIM, SCREEN_W / 2, LIST_Y - 26, CENTER);
    if (top + LIST_ROWS_VISIBLE < (int)rows.size())
        drawText(c, ui.fonts.small, "more below", TEXT_DIM, SCREEN_W / 2,
                 LIST_Y + LIST_ROWS_VISIBLE * ROW_H, CENTER);

    if (!ui.toast.empty() && armTicksToNs(armGetSystemTick()) < ui.toastUntil)
        drawText(c, ui.fonts.small, ui.toast, ACCENT_HI, 60, FOOTER_Y - 36, LEFT);

    SDL_Rect line = {30, FOOTER_Y, SCREEN_W - 60, 1};
    SDL_FillRect(c, &line, mapRgb(c, LINE));
    int right = SCREEN_W - 60;
    ui.hints.clear();
    for (auto it = hints.rbegin(); it != hints.rend(); ++it) {
        if (u64 button = hintButton(it->glyph))
            ui.hints.push_back({hintRect(ui.fonts, it->text, right), button});
        right = drawHint(c, ui.fonts, it->glyph, it->text, right);
    }

    ui.listCount = (int)rows.size();
    ui.listTop = top;
    ui.listSel = sel;
}

// A short list of choices; returns the chosen index or -1 for B
int runMenu(Ui &ui, const std::string &title, const std::vector<std::string> &options) {
    std::vector<Row> rows;
    for (const std::string &o : options)
        rows.push_back({o, "", false, false});
    ListNav nav;
    u64 down, held, now;
    while (nextFrame(ui, down, held, now)) {
        if (down & HidNpadButton_B)
            return -1;
        if (down & HidNpadButton_A)
            return nav.sel;
        nav.update(ui, down, held, now, (int)rows.size());
        drawScreen(ui, title, "", rows, nav.sel, nav.top, {{"A", "OK"}, {"B", "Back"}});
        present(ui.fb, ui.c);
    }
    return -1;
}

// Name from the system keyboard; false if cancelled or empty
bool askName(std::string &name) {
    char buf[64];
    if (switch_swkbd_get_text(buf, sizeof(buf)) != 1)
        return false;
    name = buf;
    name.erase(0, name.find_first_not_of(' '));
    name.erase(name.find_last_not_of(' ') + 1);
    return !name.empty();
}

// Switch input for libnx HID buttons, as captured by "Map a button"
const struct { u64 button; int input; } HID_INPUTS[] = {
    {HidNpadButton_A, In_A}, {HidNpadButton_B, In_B},
    {HidNpadButton_X, In_X}, {HidNpadButton_Y, In_Y},
    {HidNpadButton_L, In_L}, {HidNpadButton_R, In_R},
    {HidNpadButton_ZL, In_ZL}, {HidNpadButton_ZR, In_ZR},
    {HidNpadButton_Minus, In_Minus},
    {HidNpadButton_StickL, In_LStick}, {HidNpadButton_StickR, In_RStick},
    {HidNpadButton_Up, In_DUp}, {HidNpadButton_Down, In_DDown},
    {HidNpadButton_Left, In_DLeft}, {HidNpadButton_Right, In_DRight},
    {HidNpadButton_StickLUp, In_LSUp}, {HidNpadButton_StickLDown, In_LSDown},
    {HidNpadButton_StickLLeft, In_LSLeft}, {HidNpadButton_StickLRight, In_LSRight},
    {HidNpadButton_StickRUp, In_RSUp}, {HidNpadButton_StickRDown, In_RSDown},
    {HidNpadButton_StickRLeft, In_RSLeft}, {HidNpadButton_StickRRight, In_RSRight},
};

// Waits for the next Switch input; -1 if cancelled. + is captured on a short
// press, and holding it for a second cancels.
int runCapture(Ui &ui, const std::string &profile) {
    const u64 CANCEL_HOLD = 1000000000ULL;
    u64 down, held, now, plusSince = 0;
    bool released = false; // ignore the A press that opened this screen
    while (nextFrame(ui, down, held, now)) {
        if (!released) {
            released = held == 0;
        } else if (plusSince) {
            if (now - plusSince >= CANCEL_HOLD)
                return -1;
            if (!(held & HidNpadButton_Plus))
                return In_Plus;
        } else if (down & HidNpadButton_Plus) {
            plusSince = now;
        } else {
            for (const auto &h : HID_INPUTS)
                if (down & h.button)
                    return h.input;
        }
        drawScreen(ui, "Profile: " + profile, "", {}, 0, 0, {});
        drawText(ui.c, ui.fonts.title, "Press a button...", TEXT, SCREEN_W / 2, 300, CENTER);
        drawText(ui.c, ui.fonts.small, "The next button you press is the one you'll change.",
                 TEXT_DIM, SCREEN_W / 2, 370, CENTER);
        drawText(ui.c, ui.fonts.small, "Hold + for a second to cancel.", TEXT_DIM,
                 SCREEN_W / 2, 404, CENTER);
        present(ui.fb, ui.c);
    }
    return -1;
}

// Picks keys for one input. A replaces its keys with the selected key, Y adds
// or removes the selected key, X clears them. Returns whether keys changed.
bool runKeyPicker(Ui &ui, Profile &profile, int input) {
    struct Entry { SDL_Scancode key; int group; };
    std::vector<Entry> entries;
    std::vector<int> groupStart;
    const std::vector<KeyGroup> &groups = keyGroups();
    for (int g = 0; g < (int)groups.size(); ++g) {
        groupStart.push_back((int)entries.size());
        for (SDL_Scancode k : groups[g].keys)
            entries.push_back({k, g});
    }

    std::vector<SDL_Scancode> &keys = profile.keys[input];
    ListNav nav;
    for (int i = 0; i < (int)entries.size(); ++i)
        if (!keys.empty() && entries[i].key == keys[0]) {
            nav.sel = i;
            break;
        }
    nav.keepVisible();

    bool changed = false;
    u64 down, held, now;
    while (nextFrame(ui, down, held, now)) {
        if (down & HidNpadButton_B)
            break;
        SDL_Scancode key = entries[nav.sel].key;
        auto bound = std::find(keys.begin(), keys.end(), key);
        if (down & HidNpadButton_A) {
            keys = {key};
            changed = true;
            break;
        }
        if (down & HidNpadButton_Y) {
            if (bound != keys.end())
                keys.erase(bound);
            else
                keys.push_back(key);
            changed = true;
        }
        if (down & HidNpadButton_X) {
            keys.clear();
            changed = true;
        }
        if (down & (HidNpadButton_L | HidNpadButton_R)) {
            int g = entries[nav.sel].group + ((down & HidNpadButton_L) ? -1 : 1);
            g = (g + (int)groups.size()) % (int)groups.size();
            nav.sel = groupStart[g];
            nav.top = nav.sel;
            nav.keepVisible();
        }
        nav.update(ui, down, held, now, (int)entries.size());

        std::vector<Row> rows;
        for (const Entry &e : entries) {
            bool on = std::find(keys.begin(), keys.end(), e.key) != keys.end();
            rows.push_back({keyName(e.key), on ? "sent" : "", false, on});
        }
        std::string subtitle = std::string(groups[entries[nav.sel].group].name) +
                               "  (L / R: jump to another group)";
        drawScreen(ui, std::string(inputLabel(input)) + " sends: " + keysLabel(keys), subtitle,
                   rows, nav.sel, nav.top,
                   {{"A", "Set"}, {"Y", "Add / remove"}, {"X", "Clear"}, {"B", "Done"}});
        present(ui.fb, ui.c);
    }
    return changed;
}

// Lists a profile's bindings. Returns whether it was changed.
bool runEditor(Ui &ui, Profile &profile, bool readOnly) {
    ListNav nav;
    bool changed = false;
    u64 down, held, now;
    while (nextFrame(ui, down, held, now)) {
        if (down & HidNpadButton_B)
            break;
        if (down & HidNpadButton_A) {
            if (readOnly) {
                showToast(ui, "Default can't be changed. Make a copy with + New profile.");
            } else {
                int input = nav.sel == 0 ? runCapture(ui, profile.name) : nav.sel - 1;
                if (input >= 0) {
                    changed |= runKeyPicker(ui, profile, input);
                    nav.sel = input + 1;
                    nav.keepVisible();
                }
                continue;
            }
        }
        std::vector<Row> rows;
        rows.push_back({readOnly ? "Map a button (copy this profile to change it)" : "Map a button",
                        "", readOnly, false});
        for (int i = 0; i < InputCount; ++i)
            rows.push_back({inputLabel(i), keysLabel(profile.keys[i]), false, false});
        nav.update(ui, down, held, now, (int)rows.size());
        drawScreen(ui, "Profile: " + profile.name,
                   readOnly ? "Built in: the engine's own controller layout" : "", rows,
                   nav.sel, nav.top, {{"A", readOnly ? "Select" : "Change"}, {"B", "Back"}});
        present(ui.fb, ui.c);
    }
    return changed;
}

bool save(Ui &ui, const Store &store) {
    if (saveStore(SWITCH_CONTROLS_FILE, store))
        return true;
    showToast(ui, "Couldn't save " SWITCH_CONTROLS_FILE);
    return false;
}

// Asks for a name and adds a copy of source; returns its index or -1
int copyProfile(Ui &ui, Store &store, const Profile &source) {
    std::string name;
    if (!askName(name))
        return -1;
    Profile p = source;
    p.name = uniqueName(store, name);
    store.profiles.push_back(p);
    save(ui, store);
    return (int)store.profiles.size() - 1;
}

// The profile list: Default, the custom profiles, then "+ New profile"
void runControls(Ui &ui) {
    Store store = loadStore(SWITCH_CONTROLS_FILE);
    ListNav nav;
    u64 down, held, now;
    while (nextFrame(ui, down, held, now)) {
        int count = (int)store.profiles.size();
        bool isDefault = nav.sel == 0, isNew = nav.sel == count + 1;
        Profile *custom = (!isDefault && !isNew) ? &store.profiles[nav.sel - 1] : nullptr;

        if (down & HidNpadButton_B)
            break;
        if (down & HidNpadButton_A) {
            if (isNew) {
                int i = copyProfile(ui, store, defaultProfile());
                if (i >= 0) {
                    nav.sel = i + 1;
                    custom = &store.profiles[i];
                }
            }
            if (isDefault) {
                Profile copy = defaultProfile();
                runEditor(ui, copy, true);
            } else if (custom && runEditor(ui, *custom, false)) {
                save(ui, store);
            }
            continue;
        }
        if ((down & HidNpadButton_Y) && !isNew) {
            store.active = custom ? custom->name : "";
            if (save(ui, store))
                showToast(ui, (custom ? custom->name : std::string(DEFAULT_NAME)) +
                                  " is active for the next game you start.");
        }
        if ((down & HidNpadButton_X) && !isNew) {
            const Profile &source = custom ? *custom : defaultProfile();
            std::vector<std::string> opts = {"Make a copy"};
            if (custom)
                opts.insert(opts.end(), {"Rename", "Delete"});
            int choice = runMenu(ui, source.name, opts);
            if (choice == 0) {
                Profile src = source; // copyProfile may reallocate the list
                int i = copyProfile(ui, store, src);
                if (i >= 0)
                    nav.sel = i + 1;
            } else if (choice == 1) {
                std::string name;
                if (askName(name) && name != custom->name) {
                    std::string old = custom->name;
                    custom->name = uniqueName(store, name);
                    if (store.active == old)
                        store.active = custom->name;
                    save(ui, store);
                }
            } else if (choice == 2 &&
                       runMenu(ui, "Delete " + custom->name + "?", {"No", "Delete"}) == 1) {
                if (store.active == custom->name)
                    store.active = "";
                store.profiles.erase(store.profiles.begin() + (nav.sel - 1));
                nav.sel = std::min(nav.sel, (int)store.profiles.size());
                save(ui, store);
            }
            nav.keepVisible();
            continue;
        }

        const Profile *active = findProfile(store, store.active);
        std::vector<Row> rows;
        rows.push_back({DEFAULT_NAME, active ? "built in" : "ACTIVE", false, !active});
        for (const Profile &p : store.profiles)
            rows.push_back({p.name, &p == active ? "ACTIVE" : "", false, &p == active});
        rows.push_back({"+ New profile", "", true, false});
        nav.update(ui, down, held, now, (int)rows.size());

        std::vector<Hint> hints = {{"A", nav.sel == (int)rows.size() - 1 ? "Create" : "Open"}};
        if (nav.sel != (int)rows.size() - 1)
            hints.insert(hints.end(), {{"Y", "Make active"}, {"X", "More"}});
        hints.push_back({"B", "Back"});
        drawScreen(ui, "Settings: Controls",
                   "Button profiles. The active one is used by every game you start.",
                   rows, nav.sel, nav.top, hints);
        present(ui.fb, ui.c);
    }
}

} // namespace

MemoryWarning pickerMemoryWarning() {
    u64 total = 0;
    svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    // A full-memory launch gets about 3.2 GB and the Album about 400 MB.
    // Going by memory rather than applet type stays quiet when Atmosphere is
    // set up to give applets more.
    if (total == 0 || total >= (1ULL << 30))
        return {};
    unsigned mb = (unsigned)(total >> 20);
    char title[96];
    if (appletGetAppletType() == AppletType_LibraryApplet)
        snprintf(title, sizeof(title), "Opened from the Album: only %u MB, big games can crash", mb);
    else
        snprintf(title, sizeof(title), "Only %u MB of memory, big games can crash", mb);
    return {title, "For full memory, hold R while starting any game, then open mkxp-z."};
}

PickerResult pickerUiRun(const std::vector<PickerGame> &games, int &sel) {
    // The console's own font, so it looks like the system UI
    if (R_FAILED(plInitialize(PlServiceType_User)))
        return PICKER_UNAVAILABLE;
    PlFontData font;
    Fonts fonts;
    SDL_Surface *canvas = NULL;
    bool ttf = false;
    PickerResult result = PICKER_UNAVAILABLE;

    if (R_SUCCEEDED(plGetSharedFontByType(&font, PlSharedFontType_Standard)) &&
        TTF_Init() == 0) {
        ttf = true;
        fonts.small = openSharedFont(font, 22);
        fonts.medium = openSharedFont(font, 28);
        fonts.title = openSharedFont(font, 34);
        fonts.initials = openSharedFont(font, 96);
        canvas = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32, SDL_PIXELFORMAT_RGBA32);
    }

    Framebuffer fb;
    if (fonts.small && fonts.medium && fonts.title && fonts.initials && canvas &&
        R_SUCCEEDED(framebufferCreate(&fb, nwindowGetDefault(), SCREEN_W, SCREEN_H,
                                      PIXEL_FORMAT_RGBA_8888, 2))) {
        framebufferMakeLinear(&fb);
        int battery = -1;
        bool psm = R_SUCCEEDED(psmInitialize());

        // Thumbnails can take a moment to decode
        SDL_FillRect(canvas, NULL, mapRgb(canvas, BG));
        drawText(canvas, fonts.medium, "Loading games...", TEXT_DIM, SCREEN_W / 2, SCREEN_H / 2 - 20, CENTER);
        present(fb, canvas);
        std::vector<SDL_Surface *> thumbs;
        for (const PickerGame &g : games)
            thumbs.push_back(makeThumb(g, fonts));
        // The Settings tile is always last
        std::vector<PickerGame> tiles = games;
        tiles.push_back({"", "Settings"});
        thumbs.push_back(makeGearThumb());
        const int settingsTile = (int)tiles.size() - 1;

        PadState pad;
        padInitializeDefault(&pad);
        hidInitializeTouchScreen();
        Ui ui{fb, canvas, fonts, pad, psm, battery, "", 0};
        const MemoryWarning warn = pickerMemoryWarning();
        int first = std::max(0, sel - VISIBLE_TILES + 1);
        u64 heldSince = 0, lastRepeat = 0;
        const PickerTouch::Layout row{ROW_X, TILE_Y, TILE, TILE + TILE_GAP, VISIBLE_TILES,
                                      (int)tiles.size()};
        PickerTouch::Gesture touch;
        result = PICKER_QUIT;

        while (appletMainLoop()) {
            padUpdate(&pad);
            u64 down = padGetButtonsDown(&pad);
            u64 held = padGetButtons(&pad);
            u64 now = armTicksToNs(armGetSystemTick());

            // Touch: tap a tile to select it and again to start it, drag the
            // row to scroll, or tap the footer hints
            HidTouchScreenState ts = {};
            bool touching = hidGetTouchScreenStates(&ts, 1) > 0 && ts.count > 0;
            bool tapped = touch.update(row.scroll(), touching, touching ? (int)ts.touches[0].x : 0,
                                       touching ? (int)ts.touches[0].y : 0, FOOTER_Y, first, sel);
            bool start = down & HidNpadButton_A;
            bool quit = down & (HidNpadButton_Plus | HidNpadButton_B);
            if (tapped) {
                int tile = PickerTouch::tileAt(row, first, touch.x0, touch.y0);
                SDL_Rect exitHint = hintRect(fonts, "Exit to hbmenu", SCREEN_W - 60);
                SDL_Rect startHint = hintRect(fonts, sel == settingsTile ? "Open" : "Start",
                                              exitHint.x - 36);
                if (tile >= 0) {
                    if (tile == sel)
                        start = true;
                    else
                        sel = tile;
                } else if (hitHint(startHint, touch.x0, touch.y0)) {
                    start = true;
                } else if (hitHint(exitHint, touch.x0, touch.y0)) {
                    quit = true;
                }
            }

            if (quit)
                break; // back to hbmenu
            if (start) {
                if (sel != settingsTile) {
                    result = PICKER_PICKED;
                    break;
                }
                ui.battery = battery;
                runControls(ui);
                battery = ui.battery;
                continue;
            }

            // Left/right, repeating while held
            int step = 0;
            u64 dirs = HidNpadButton_AnyLeft | HidNpadButton_AnyRight;
            if (down & dirs) {
                heldSince = lastRepeat = now;
                step = (down & HidNpadButton_AnyLeft) ? -1 : 1;
            } else if ((held & dirs) && now - heldSince > 400000000ULL &&
                       now - lastRepeat > 110000000ULL) {
                lastRepeat = now;
                step = (held & HidNpadButton_AnyLeft) ? -1 : 1;
            }
            if (step)
                sel = std::max(0, std::min(settingsTile, sel + step));
            if (sel < first)
                first = sel;
            else if (sel >= first + VISIBLE_TILES)
                first = sel - VISIBLE_TILES + 1;

            u32 pct;
            if (psm && R_SUCCEEDED(psmGetBatteryChargePercentage(&pct)))
                battery = (int)pct;

            drawFrame(canvas, fonts, tiles, thumbs, sel, first, now / 1e9, battery, warn);
            present(fb, canvas); // waits for a free buffer (vsync)
        }

        for (SDL_Surface *s : thumbs)
            SDL_FreeSurface(s);
        if (psm)
            psmExit();
        framebufferClose(&fb);
    }

    if (canvas)
        SDL_FreeSurface(canvas);
    for (TTF_Font *f : {fonts.small, fonts.medium, fonts.title, fonts.initials})
        if (f)
            TTF_CloseFont(f);
    if (ttf)
        TTF_Quit();
    plExit();
    return result;
}

#endif // __SWITCH__
