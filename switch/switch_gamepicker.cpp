#ifdef __SWITCH__

// Avoid pulling in Ruby's assert.h from libnx service.h (see switch_main.cpp)
#define RUBY_ASSERT_H 1

#include <switch.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include <algorithm>
#include <string>
#include <vector>

#include "switch_pickerui.h"

// Startup game picker, run before SDL takes the display: the home-screen
// style UI (switch_pickerui.cpp), or the libnx text console if that cannot
// start. Games live in GAMES_ROOT/<folder>/ with a Game.ini each; a
// Game.ini directly in GAMES_ROOT is launched as the only game.

#define MKXPZ_ROOT "sdmc:/switch/mkxp-z"
#define GAMES_ROOT MKXPZ_ROOT "/Games"
// Folder names, most recently played first
#define HISTORY_FILE MKXPZ_ROOT "/play_history.txt"
// Single last-played folder written by earlier versions
#define LAST_GAME_FILE MKXPZ_ROOT "/last_game.txt"
#define HISTORY_MAX 100

namespace {

struct GameEntry {
    std::string folder;
    std::string title;
};

// The game's directory; folder "" is a game sitting directly in GAMES_ROOT
std::string gamePath(const std::string &folder) {
    return folder.empty() ? std::string(GAMES_ROOT) : GAMES_ROOT "/" + folder;
}

std::string displayName(const GameEntry &g) {
    if (!g.folder.empty())
        return g.folder;
    return g.title.empty() ? "Game" : g.title;
}

bool isFile(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

// Path of the folder's Game.ini (any capitalization), or "" if none
std::string findGameIni(const std::string &dir) {
    DIR *d = opendir(dir.c_str());
    if (!d)
        return "";
    std::string ini;
    while (struct dirent *e = readdir(d)) {
        if (strcasecmp(e->d_name, "Game.ini") == 0) {
            ini = dir + "/" + e->d_name;
            break;
        }
    }
    closedir(d);
    return isFile(ini) ? ini : "";
}

// Title= from the [Game] section, or "" if missing
std::string readTitle(const std::string &iniPath) {
    FILE *f = fopen(iniPath.c_str(), "r");
    if (!f)
        return "";
    char line[256];
    std::string title;
    while (fgets(line, sizeof(line), f)) {
        if (strncasecmp(line, "Title=", 6) == 0) {
            title = line + 6;
            title.erase(title.find_last_not_of("\r\n") + 1);
            break;
        }
    }
    fclose(f);
    return title;
}

std::vector<GameEntry> scanGames() {
    std::vector<GameEntry> games;
    DIR *d = opendir(GAMES_ROOT);
    if (!d)
        return games;
    while (struct dirent *e = readdir(d)) {
        if (e->d_name[0] == '.')
            continue;
        std::string ini = findGameIni(std::string(GAMES_ROOT "/") + e->d_name);
        if (!ini.empty())
            games.push_back({e->d_name, readTitle(ini)});
    }
    closedir(d);
    std::sort(games.begin(), games.end(), [](const GameEntry &a, const GameEntry &b) {
        return strcasecmp(a.folder.c_str(), b.folder.c_str()) < 0;
    });
    return games;
}

std::vector<std::string> readLines(const char *path) {
    std::vector<std::string> lines;
    FILE *f = fopen(path, "r");
    if (!f)
        return lines;
    char buf[512];
    while (fgets(buf, sizeof(buf), f)) {
        std::string s(buf);
        s.erase(s.find_last_not_of("\r\n") + 1);
        if (!s.empty())
            lines.push_back(s);
    }
    fclose(f);
    return lines;
}

std::vector<std::string> readHistory() {
    std::vector<std::string> history = readLines(HISTORY_FILE);
    if (history.empty())
        history = readLines(LAST_GAME_FILE);
    return history;
}

// Moves folder to the front of the play history
void recordPlayed(const std::string &folder) {
    std::vector<std::string> history = readHistory();
    history.erase(std::remove(history.begin(), history.end(), folder), history.end());
    history.insert(history.begin(), folder);
    if (history.size() > HISTORY_MAX)
        history.resize(HISTORY_MAX);
    if (FILE *f = fopen(HISTORY_FILE, "w")) {
        for (const std::string &h : history)
            fprintf(f, "%s\n", h.c_str());
        fclose(f);
    }
}

// Most recently played first, then never-played games alphabetically
void sortByLastPlayed(std::vector<GameEntry> &games) {
    std::vector<std::string> history = readHistory();
    auto rank = [&](const GameEntry &g) {
        auto it = std::find(history.begin(), history.end(), g.folder);
        return it == history.end() ? history.size() : (size_t)(it - history.begin());
    };
    std::stable_sort(games.begin(), games.end(), [&](const GameEntry &a, const GameEntry &b) {
        return rank(a) < rank(b);
    });
}

const int LIST_ROWS = 36;

// Set once the menu was shown, so quitting a game returns to it
bool menuShown = false;

void drawMenu(const std::vector<GameEntry> &games, int sel, int top) {
    consoleClear();
    printf("\x1b[1;2Hmkxp-z - choose a game\n\n");
    for (int row = 0; row < LIST_ROWS && top + row < (int)games.size(); ++row) {
        const GameEntry &g = games[top + row];
        bool cur = (top + row == sel);
        std::string label = displayName(g);
        if (!g.title.empty() && g.title != label)
            label += "  (" + g.title + ")";
        if (label.size() > 74)
            label.resize(74);
        printf("\x1b[%d;2H%s%s%s", 4 + row, cur ? "\x1b[30;47m> " : "  ", label.c_str(), "\x1b[0m");
    }
    printf("\x1b[43;2HUp/Down: select   A: play   B/+: exit to hbmenu");
    printf("\x1b[44;2H%s", GAMES_ROOT "/<game folder>/Game.ini");
    consoleUpdate(NULL);
}

// Shows a message until + is pressed
void showMessage(PadState &pad, const char *msg) {
    consoleClear();
    printf("\x1b[2;2H%s\x1b[43;2HPress + to exit", msg);
    consoleUpdate(NULL);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;
        consoleUpdate(NULL); // waits for vsync
    }
}

} // namespace

// Writes the chosen game directory to out. Returns false if the user quit.
extern "C" bool switch_pick_game(char *out, size_t outSize) {
    std::vector<GameEntry> games = scanGames();

    // Old layout: the game itself sits directly in GAMES_ROOT. It's listed
    // (folder "") only when no per-game subfolders exist; otherwise a stray
    // Games/Game.ini would hide the real games (TODO #1). The picker is shown
    // even for a single game, so Settings stays reachable.
    if (games.empty()) {
        std::string rootIni = findGameIni(GAMES_ROOT);
        if (!rootIni.empty())
            games.push_back({"", readTitle(rootIni)});
    }

    menuShown = true;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // The most recently played game comes first and starts selected
    sortByLastPlayed(games);
    int sel = 0;

    if (!games.empty()) {
        std::vector<PickerGame> uiGames;
        for (const GameEntry &g : games)
            uiGames.push_back({gamePath(g.folder), displayName(g)});
        PickerResult r = pickerUiRun(uiGames, sel);
        if (r == PICKER_QUIT)
            return false;
        if (r == PICKER_PICKED) {
            recordPlayed(games[sel].folder);
            snprintf(out, outSize, "%s", gamePath(games[sel].folder).c_str());
            return true;
        }
    }

    // Fallback: text console
    PadState pad;
    padInitializeDefault(&pad);
    consoleInit(NULL);

    if (games.empty()) {
        showMessage(pad, "No games found.\n\n Put each game in its own folder:\n "
                         GAMES_ROOT "/<game folder>/Game.ini");
        consoleExit(NULL);
        return false;
    }

    int top = std::max(0, sel - LIST_ROWS + 1);

    bool picked = false;
    drawMenu(games, sel, top);
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        int prev = sel;

        if (down & (HidNpadButton_Plus | HidNpadButton_B))
            break;
        if (down & HidNpadButton_A) {
            picked = true;
            break;
        }
        if (down & HidNpadButton_AnyUp)
            sel = (sel + (int)games.size() - 1) % (int)games.size();
        if (down & HidNpadButton_AnyDown)
            sel = (sel + 1) % (int)games.size();

        if (sel != prev) {
            if (sel < top)
                top = sel;
            else if (sel >= top + LIST_ROWS)
                top = sel - LIST_ROWS + 1;
            drawMenu(games, sel, top);
        }
        consoleUpdate(NULL); // waits for vsync
    }
    consoleExit(NULL);

    if (!picked)
        return false;

    recordPlayed(games[sel].folder);
    snprintf(out, outSize, "%s", gamePath(games[sel].folder).c_str());
    return true;
}

// Call when a game picked from the menu exits. mkxp-z cannot restart Ruby
// in-process, so ask the homebrew loader to start this .nro again, which
// shows the picker. argv0 is this .nro's path as passed by the loader.
extern "C" void switch_return_to_picker(const char *argv0) {
    if (!menuShown)
        return;
    if (!envHasNextLoad()) {
        printf("mkxp-z: loader cannot relaunch, not returning to game picker\n");
        return;
    }
    const char *self = (argv0 && strncmp(argv0, "sdmc:/", 6) == 0)
        ? argv0 : MKXPZ_ROOT "/mkxp-z.nro";
    std::string args = std::string("\"") + self + "\"";
    Result rc = envSetNextLoad(self, args.c_str());
    printf("mkxp-z: relaunch %s for game picker: 0x%x\n", self, rc);
}

#endif // __SWITCH__
