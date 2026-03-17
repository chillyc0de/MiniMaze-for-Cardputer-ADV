/**
 * ============================================================================
 * MiniMaze Game for M5Stack Cardputer ADV
 * Copyright (c) 2026 chillyc0de
 * Licensed under the MIT License.
 * * This software incorporates components from the following third-party works:
 * - M5Cardputer: Copyright (c) M5Stack (SDK)
 * See the NOTICE.txt and LICENSE file in the repository for full license texts.
 * ============================================================================
 */

#include <M5Cardputer.h>
#include <Preferences.h>
#include <SD.h>
#include <queue>

#define DEFAULT_BRIGHTNESS 130
#define DEFAULT_VOLUME 80
#define DEFAULT_SOUND_ES false
#define DEFAULT_SOUND_KBD true
#define DEFAULT_SOUND_GM true
#define DEFAULT_SOUND_SCR true
#define DEFAULT_MAZE_PRESET_INDEX 0
#define DEFAULT_SPAWN_PLAYERS_COUNT 1
#define DEFAULT_SPAWN_EXITS_COUNT 1

const char *SCREENSHOTS_DIR_PATH = "/by_chillyc0de/MiniMaze/screenshots/";
const char *FIRMWARE_VERSION = "v1.0.0";

const int SCREEN_WIDTH = 240;
const int SCREEN_HEIGHT = 135;

const uint16_t UI_BG = 0x0000;
const uint16_t UI_FG = 0xFFFF;
const uint16_t UI_ACCENT = 0xE204;
const uint16_t UI_MUTED = 0x39E7;
const uint16_t UI_DANGER = 0xF800;
const uint16_t UI_VALID = 0x07E0;

const int MAX_MAZE_WIDTH = 47;
const int MAX_MAZE_HEIGHT = 27;

const uint16_t MAZE_EMPTY = 0x0000;
const uint16_t MAZE_PLAYER = 0xF800;
const uint16_t MAZE_WALL = 0x52AA;
const uint16_t MAZE_EXIT = 0x07E0;

// --- СТРУКТУРЫ ДЛЯ ИГРЫ ---
struct MazeConfig {
    int w;
    int h;
    int s;
};
const std::vector<MazeConfig> MAZE_PRESETS = {
    {21, 11, 11},
    {27, 15, 9},
    {33, 19, 7},
    {47, 27, 5},
};
enum MazeCell : uint8_t {
    CELL_EMPTY,
    CELL_WALL,
    CELL_PLAYER,
    CELL_EXIT,
};

const std::vector<String> userGuideLines = {
    "============= MINI MAZE =============",
    " Created by chillyc0de.",
    " Assisted by Google Gemini (LLM).",
    " Maze mini-game",
    " for M5Stack Cardputer ADV.",
    "",
    "------- 1. GETTING  STARTED -------",
    " Navigate the player through",
    " the maze to find the exit.",
    "",
    "----------- 2. CONTROLS -----------",
    " Movement: [Arrows].",
    " Auto-Play: Press [Ctrl] to toggle",
    " smart pathfinding assistance.",
    " Screenshot: Press [BtnGO].",
    " Exit/Menu: Press [Esc].",
    "",
    "--------- 3. GAME MODES -----------",
    " Use the Settings menu to adjust:",
    " * Maze Size: Tiny to Ultra.",
    " * Walls: Hidden or Visible.",
    " * Targets: Player/Exit counts.",
    "",
    "--------- 4. SCREENSHOTS ----------",
    " Capture your progress to SD.",
    " Saved as .bmp to SD card.",
    " Path: /by_chillyc0de/MiniMaze/",
    " screenshots/",
    "",
    "------- 5. LICENSE & RIGHTS -------",
    " Provided 'AS IS' under MIT License.",
    " Use at your own risk. The author",
    " is not responsible for any bugs",
    " or SD card data loss.",
};

enum ExternalState : uint8_t {
    STATE_SPLASH_SCREEN,
    STATE_GUIDE,
    STATE_MAIN_MENU,
    STATE_GAME_AREA,
    STATE_SETTINGS_MENU,
    STATE_GAME_SETUP,
    STATE_BRIGHTNESS_SETUP,
    STATE_VOLUME_SETUP,
    STATE_SOUND_SETUP,
};

struct InternalState {
    ExternalState currentExternalState = STATE_SPLASH_SCREEN;
    bool requiresRedraw = true;

    // Guide
    int guideScrollY = 0;
    int guideScrollX = 0;

    // Main menu
    int mainMenuSelectedIndex = 0;
    int mainMenuScrollOffset = 0;

    // Game setup
    int gameSetupFieldIdx = 0;
    int mazePresetIndex = DEFAULT_MAZE_PRESET_INDEX;
    int spawnPlayersCount = DEFAULT_SPAWN_PLAYERS_COUNT;
    int spawnExitsCount = DEFAULT_SPAWN_EXITS_COUNT;

    // Game area
    int mazeWidth = MAZE_PRESETS[mazePresetIndex].w;
    int mazeHeight = MAZE_PRESETS[mazePresetIndex].h;
    int mazeCellSize = MAZE_PRESETS[mazePresetIndex].s;
    MazeCell mazeCells[MAX_MAZE_WIDTH][MAX_MAZE_HEIGHT];
    bool showWalls = true;
    bool autoPlay = false;

    // Settings menu
    int settingsMenuSelectedIndex = 0;
    int settingsMenuScrollOffset = 0;

    // Brightness setup
    int brightnessSetupBrightnessCounter = DEFAULT_BRIGHTNESS;

    // Volume setup
    int volumeSetupVolumeCounter = DEFAULT_VOLUME;

    // Sound setup
    int soundSetupFieldIdx = 0;
    bool soundSetupExternalState = DEFAULT_SOUND_ES;
    bool soundSetupKeyboard = DEFAULT_SOUND_KBD;
    bool soundSetupGame = DEFAULT_SOUND_GM;
    bool soundSetupScreenshot = DEFAULT_SOUND_SCR;
};

LGFX_Sprite displaySprite(&M5.Lcd);
Preferences systemPreferences;
InternalState internalState;

// --- ЗВУКОВАЯ ИНДИКАЦИЯ ---
void playKeyTone(char kChar, Keyboard_Class::KeysState kState = {}) {
    if (internalState.volumeSetupVolumeCounter == 0) return;

    float frequency = 1000;
    const uint32_t duration = 40;

    // --- Комбинации Fn ---
    if (kState.fn) {
        if (kChar == ';') frequency = 1400;      // UP
        else if (kChar == '.') frequency = 1350; // DOWN
        else if (kChar == ',') frequency = 1300; // LEFT
        else if (kChar == '/') frequency = 1250; // RIGHT
        else if (kChar == '`') frequency = 1500; // ESCAPE
        else if (kState.del) frequency = 1200;   // DELETE

        M5.Speaker.tone(frequency, duration);
        return;
    }

    // Клавиши состояния
    if (kState.enter) frequency = 1200;
    else if (kState.tab) frequency = 1100;
    else if (kState.del) frequency = 1000;

    // Пробел
    else if (kChar == ' ') frequency = 900;

    // Символы
    else {
        switch (tolower(kChar)) {
        case 'a':
        case 'b':
        case 'c':
            frequency = 1000;
            break;
        case 'd':
        case 'e':
        case 'f':
            frequency = 1050;
            break;
        case 'g':
        case 'h':
        case 'i':
            frequency = 1100;
            break;
        case 'j':
        case 'k':
        case 'l':
            frequency = 1150;
            break;
        case 'm':
        case 'n':
        case 'o':
            frequency = 1200;
            break;
        case 'p':
        case 'q':
        case 'r':
        case 's':
            frequency = 1250;
            break;
        case 't':
        case 'u':
        case 'v':
            frequency = 1300;
            break;
        case 'w':
        case 'x':
        case 'y':
        case 'z':
            frequency = 1350;
            break;
        case '0':
        case '1':
        case '2':
            frequency = 900;
            break;
        case '3':
        case '4':
        case '5':
            frequency = 950;
            break;
        case '6':
        case '7':
        case '8':
            frequency = 1000;
            break;
        case '9':
            frequency = 1050;
            break;
            // Спецсимволы
        default:
            frequency = 950;
            break;
        }
    }

    M5.Speaker.tone(frequency, duration);
}

void playExternalStateTone(ExternalState externalState) {
    if (internalState.volumeSetupVolumeCounter == 0) return;

    switch (externalState) {
    case STATE_SPLASH_SCREEN:
        M5.Speaker.tone(700, 80);
        delay(80);
        M5.Speaker.tone(900, 80);
        break;
    case STATE_GUIDE:
        M5.Speaker.tone(600, 100);
        delay(100);
        M5.Speaker.tone(800, 100);
        break;
    case STATE_MAIN_MENU:
        M5.Speaker.tone(1000, 50);
        delay(50);
        M5.Speaker.tone(1000, 50);
        break;
    case STATE_GAME_AREA:
        M5.Speaker.tone(600, 50);
        delay(50);
        M5.Speaker.tone(750, 50);
        delay(50);
        M5.Speaker.tone(900, 50);
        delay(50);
        M5.Speaker.tone(1200, 100);
        break;
    case STATE_SETTINGS_MENU:
        M5.Speaker.tone(750, 60);
        delay(60);
        M5.Speaker.tone(900, 60);
        delay(60);
        M5.Speaker.tone(1050, 60);
        break;
    case STATE_GAME_SETUP:
        M5.Speaker.tone(800, 80);
        delay(80);
        M5.Speaker.tone(1000, 80);
        break;
    case STATE_BRIGHTNESS_SETUP:
        M5.Speaker.tone(750, 60);
        delay(60);
        M5.Speaker.tone(1300, 60);
        break;
    case STATE_VOLUME_SETUP:
        M5.Speaker.tone(750, 60);
        delay(60);
        M5.Speaker.tone(550, 60);
        break;
    case STATE_SOUND_SETUP:
        M5.Speaker.tone(750, 60);
        delay(60);
        M5.Speaker.tone(850, 40);
        delay(40);
        M5.Speaker.tone(750, 60);
        break;
    }
}

void playMazeCellTone(MazeCell cellType) {
    if (internalState.volumeSetupVolumeCounter == 0 || !internalState.soundSetupGame) return;

    switch (cellType) {
    case CELL_EMPTY:
    case CELL_WALL:
    case CELL_PLAYER:
        M5.Speaker.tone(1800, 20);
        break;
    case CELL_EXIT:
        M5.Speaker.tone(1000, 80);
        delay(80);
        M5.Speaker.tone(1300, 80);
        delay(80);
        M5.Speaker.tone(1600, 80);
        delay(80);
        M5.Speaker.tone(2000, 150);
        break;
    }
}

// --- АЛГОРИТМЫ ИГРЫ ---
void generateMaze() {
    // Очистка поля
    for (int x = 0; x < internalState.mazeWidth; x++)
        for (int y = 0; y < internalState.mazeHeight; y++)
            internalState.mazeCells[x][y] = CELL_EMPTY;

    std::vector<std::pair<int, int>> stack;
    stack.push_back({1, 1});
    internalState.mazeCells[1][1] = CELL_WALL;

    // DFS Генерация стен
    std::vector<int> neighbors;
    neighbors.reserve(4);
    while (!stack.empty()) {
        int cX = stack.back().first;
        int cY = stack.back().second;
        neighbors.clear();

        if (cX >= 3 && internalState.mazeCells[cX - 2][cY] == 0) neighbors.push_back(0);
        if (cX <= internalState.mazeWidth - 4 && internalState.mazeCells[cX + 2][cY] == 0) neighbors.push_back(1);
        if (cY >= 3 && internalState.mazeCells[cX][cY - 2] == 0) neighbors.push_back(2);
        if (cY <= internalState.mazeHeight - 4 && internalState.mazeCells[cX][cY + 2] == 0) neighbors.push_back(3);

        if (!neighbors.empty()) {
            int dir = neighbors[random(neighbors.size())];
            int nX = cX, nY = cY, pX = cX, pY = cY;
            if (dir == 0) {
                nX -= 2;
                pX -= 1;
            } else if (dir == 1) {
                nX += 2;
                pX += 1;
            } else if (dir == 2) {
                nY -= 2;
                pY -= 1;
            } else if (dir == 3) {
                nY += 2;
                pY += 1;
            }
            internalState.mazeCells[pX][pY] = CELL_WALL;
            internalState.mazeCells[nX][nY] = CELL_WALL;
            stack.push_back({nX, nY});
        } else stack.pop_back();
    }
}

void spawnPlayers() {
    std::vector<std::pair<int, int>> emptyCells;
    for (int x = 0; x < internalState.mazeWidth; x++) {
        for (int y = 0; y < internalState.mazeHeight; y++) {
            if (internalState.mazeCells[x][y] == CELL_EMPTY) {
                emptyCells.push_back({x, y});
            }
        }
    }

    // Защита от зависания
    int targetSpawnCount = internalState.spawnPlayersCount;
    if (targetSpawnCount > emptyCells.size()) targetSpawnCount = emptyCells.size();

    // Спавн игроков
    for (int i = 0; i < targetSpawnCount; i++) {
        // Случайный индекс из вектора
        int randomIndex = random(emptyCells.size());

        // Размещаем игрока
        int pX = emptyCells[randomIndex].first;
        int pY = emptyCells[randomIndex].second;
        internalState.mazeCells[pX][pY] = CELL_PLAYER;

        // Быстро удаляем выбранную ячейку из вектора, чтобы не выбрать её повторно
        // (заменяем текущую ячейку последней в списке и удаляем последнюю)
        emptyCells[randomIndex] = emptyCells.back();
        emptyCells.pop_back();
    }
}

void spawnExits() {
    std::vector<std::pair<int, int>> emptyCells;
    for (int x = 0; x < internalState.mazeWidth; x++) {
        for (int y = 0; y < internalState.mazeHeight; y++) {
            if (internalState.mazeCells[x][y] == CELL_EMPTY) {
                emptyCells.push_back({x, y});
            }
        }
    }

    // Защита от зависания
    int targetSpawnCount = internalState.spawnExitsCount;
    if (targetSpawnCount > emptyCells.size()) targetSpawnCount = emptyCells.size();

    // Спавн выходов
    for (int i = 0; i < targetSpawnCount; i++) {
        int randomIndex = random(emptyCells.size());

        // Размещаем выход
        int eX = emptyCells[randomIndex].first;
        int eY = emptyCells[randomIndex].second;
        internalState.mazeCells[eX][eY] = CELL_EXIT;

        // Быстро удаляем выбранную ячейку из вектора, чтобы не выбрать её повторно
        // (заменяем текущую ячейку последней в списке и удаляем последнюю)
        emptyCells[randomIndex] = emptyCells.back();
        emptyCells.pop_back();
    }
}

void solveMaze() {
    // Создаем карту расстояний (заполняем огромным числом, означающим "пути нет")
    // Используем статический одномерный вектор для экономии памяти и избежания фрагментации кучи
    static std::vector<uint16_t> dist;
    dist.assign(internalState.mazeWidth * internalState.mazeHeight, 65535);
    static std::queue<std::pair<int, int>> q;
    while (!q.empty()) q.pop(); // Очистка старой очереди

    int playersCount = 0;
    int exitsCount = 0;

    // Находим все выходы (источники волны) и игроков
    for (int x = 0; x < internalState.mazeWidth; x++) {
        for (int y = 0; y < internalState.mazeHeight; y++) {
            if (internalState.mazeCells[x][y] == CELL_EXIT) {
                dist[y * internalState.mazeWidth + x] = 0; // Расстояние до выхода = 0
                q.push({x, y});
                exitsCount++;
            } else if (internalState.mazeCells[x][y] == CELL_PLAYER) {
                playersCount++;
            }
        }
    }

    // Если нет выходов или игроков — играть не во что
    if (exitsCount == 0 || playersCount == 0) return;

    // Смещения для движения (Вверх, Вниз, Влево, Вправо)
    int dx[] = {0, 0, -1, 1};
    int dy[] = {-1, 1, 0, 0};

    // Пускаем "волну" (BFS) для заполнения карты расстояний
    while (!q.empty()) {
        std::pair<int, int> current = q.front();
        int cx = current.first;
        int cy = current.second;
        q.pop();

        for (int i = 0; i < 4; i++) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            // Проверяем границы лабиринта
            if (nx >= 0 && nx < internalState.mazeWidth && ny >= 0 && ny < internalState.mazeHeight) {
                // Волна проходит через всё, кроме стен
                if (internalState.mazeCells[nx][ny] != CELL_WALL) {
                    // Если нашли более короткий путь в эту ячейку — обновляем
                    if (dist[ny * internalState.mazeWidth + nx] > dist[cy * internalState.mazeWidth + cx] + 1) {
                        dist[ny * internalState.mazeWidth + nx] = dist[cy * internalState.mazeWidth + cx] + 1;
                        q.push({nx, ny});
                    }
                }
            }
        }
    }

    // Двигаем игроков по градиенту (к ячейкам с меньшим числом)
    // Сначала собираем их координаты, чтобы не сдвинуть одного игрока дважды за тик
    std::vector<std::pair<int, int>> players;
    for (int x = 0; x < internalState.mazeWidth; x++) {
        for (int y = 0; y < internalState.mazeHeight; y++) {
            if (internalState.mazeCells[x][y] == CELL_PLAYER) {
                players.push_back({x, y});
            }
        }
    }

    bool hasEatenExit = false;

    for (std::pair<int, int> &p : players) {
        int px = p.first;
        int py = p.second;
        int currentDist = dist[py * internalState.mazeWidth + px];

        if (currentDist == 65535) continue; // Игрок заперт стенами, пути к выходу нет

        // Ищем лучшую соседнюю ячейку
        int bestNx = -1, bestNy = -1;
        int minDist = currentDist;

        for (int i = 0; i < 4; i++) {
            int nx = px + dx[i];
            int ny = py + dy[i];

            if (nx >= 0 && nx < internalState.mazeWidth && ny >= 0 && ny < internalState.mazeHeight) {
                // Игрок не может наступить на стену или на ДРУГОГО игрока
                if (internalState.mazeCells[nx][ny] != CELL_WALL &&
                    internalState.mazeCells[nx][ny] != CELL_PLAYER) {

                    if (dist[ny * internalState.mazeWidth + nx] < minDist) {
                        minDist = dist[ny * internalState.mazeWidth + nx];
                        bestNx = nx;
                        bestNy = ny;
                    }
                }
            }
        }

        // Если нашли куда шагнуть
        if (bestNx != -1) {
            MazeCell targetCell = internalState.mazeCells[bestNx][bestNy];

            // Оставляем след
            internalState.mazeCells[px][py] = CELL_EMPTY;

            if (targetCell == CELL_EXIT) {
                internalState.mazeCells[bestNx][bestNy] = CELL_EMPTY; // Игрок исчезает, выход съеден
                playersCount--;
                exitsCount--;
                hasEatenExit = true;
            } else {
                // Игрок переходит на новую клетку
                internalState.mazeCells[bestNx][bestNy] = CELL_PLAYER;
            }
        }
    }

    // Озвучка и проверка состояний
    if (hasEatenExit) playMazeCellTone(CELL_EXIT);
    else playMazeCellTone(CELL_EMPTY);

    // Если все игроки дошли до выходов, или выходы закончились — спавним новых
    if (playersCount == 0) spawnPlayers();
    if (exitsCount == 0) spawnExits();
}

// --- ПЕРЕКЛЮЧЕНИЕ ЭКРАНОВ ---
void switchExternalState(ExternalState externalState) {
    internalState.currentExternalState = externalState;
    internalState.requiresRedraw = true;

    // Звуковая индикация
    if (internalState.soundSetupExternalState) playExternalStateTone(externalState);
}

// --- СТАТИЧНЫЕ МЕНЮ ---
struct MenuOption {
    String label;
    std::function<void()> action;
};

const std::vector<MenuOption> mainMenuOptions = {
    {
        "Play game",
        []() {
            generateMaze();
            spawnPlayers();
            spawnExits();
            switchExternalState(STATE_GAME_AREA);
        },
    },
    {
        "Settings",
        []() {
            switchExternalState(STATE_SETTINGS_MENU);
        },
    },
};

const std::vector<MenuOption> settingsMenuOptions = {
    {
        "Game setup",
        []() {
            switchExternalState(STATE_GAME_SETUP);
        },
    },
    {
        "Brightness setup",
        []() {
            switchExternalState(STATE_BRIGHTNESS_SETUP);
        },
    },
    {
        "Volume setup",
        []() {
            switchExternalState(STATE_VOLUME_SETUP);
        },
    },
    {
        "Sound setup",
        []() {
            switchExternalState(STATE_SOUND_SETUP);
        },
    },
};

// --- ФУНКЦИИ ДЛЯ РАБОТЫ С ДАННЫМИ ---
void ensureDirectoryExists(const char *dirPath, bool isFilePath = false) {
    String fullPath = String(dirPath);

    // Если это путь к файлу, отрезаем имя файла
    if (isFilePath) {
        int lastSlash = fullPath.lastIndexOf('/');
        if (lastSlash != -1) fullPath = fullPath.substring(0, lastSlash);
    }

    // Убираем последний "/"
    if (fullPath.endsWith("/")) fullPath.remove(fullPath.length() - 1);

    String currentPath = "";
    // Не считаем ведущий "/"
    int currentIdx = fullPath.startsWith("/") ? 1 : 0;
    while (currentIdx < fullPath.length()) {
        int slashIdx = fullPath.indexOf('/', currentIdx);
        // Если слэшей больше нет, берем до конца строки
        if (slashIdx == -1) slashIdx = fullPath.length();

        currentPath = fullPath.substring(0, slashIdx);
        if (!SD.exists(currentPath)) SD.mkdir(currentPath);

        currentIdx = slashIdx + 1;
    }
}

// --- ОТРИСОВКА ВСПОМОГАТЕЛЬНЫХ ЭЛЕМЕНТОВ ---
void drawHeader(String title, String rightText = "") {
    displaySprite.fillRect(0, 0, SCREEN_WIDTH, 24, UI_ACCENT);
    displaySprite.setTextColor(UI_FG);

    if (rightText == "") {
        displaySprite.setTextDatum(middle_center);
        displaySprite.drawString(title, SCREEN_WIDTH / 2, 12, &fonts::Font2);
    } else {
        displaySprite.setTextDatum(middle_left);
        displaySprite.drawString(title, 10, 12, &fonts::Font2);
        displaySprite.setTextDatum(middle_right);
        displaySprite.drawString(rightText, SCREEN_WIDTH - 10, 12, &fonts::Font2);
    }
}

void drawFooter(const std::vector<String> &lines) {
    int numLines = lines.size();

    int lineHeight = 12;
    int padding = 6;

    int footerHeight = (numLines * lineHeight) + padding;

    displaySprite.fillRect(0, SCREEN_HEIGHT - footerHeight, SCREEN_WIDTH, footerHeight, UI_MUTED);
    displaySprite.setTextColor(UI_FG);
    displaySprite.setTextDatum(middle_left);

    int startY = (SCREEN_HEIGHT - footerHeight) + 9;
    for (int i = 0; i < numLines; i++) {
        int yPos = startY + (i * lineHeight);
        displaySprite.drawString(lines[i], 2, yPos, &fonts::Font0);
    }
}

void drawScrollbar(int current, int visible, int total, int yStart, int height) {
    if (total <= visible) return;
    int barHeight = max(10, (visible * height) / total);
    int maxTop = total - visible;
    int barY = yStart + (current * (height - barHeight)) / maxTop;
    displaySprite.fillRect(SCREEN_WIDTH - 4, yStart, 4, height, UI_BG);
    displaySprite.fillRect(SCREEN_WIDTH - 3, barY, 2, barHeight, UI_ACCENT);
}

void drawProgressBar(float progress, uint16_t fgColor = UI_ACCENT, uint16_t bgColor = UI_BG) {
    // progress от 0.0 до 1.0
    progress = max(0.0f, min(progress, 1.0f));

    // Обводка прогресс бара
    int w = SCREEN_WIDTH;
    int h = 12; // Высота бара
    int x = 0;
    int y = SCREEN_HEIGHT - h - 18;
    displaySprite.drawRect(x, y, w, h, UI_FG);

    // Внутреннее пространство
    displaySprite.fillRect(x + 1, y + 1, w - 2, h - 2, bgColor);

    // Заливка полосы прогресса
    int fillW = (int)((w - 4) * progress);
    if (fillW > 0) displaySprite.fillRect(x + 2, y + 2, fillW, h - 4, fgColor);
}

struct MessageLine {
    String text;
    const lgfx::IFont *fontPtr;
    MessageLine(const char *t, const lgfx::IFont *f = &fonts::Font4) : text(t), fontPtr(f) {}
    MessageLine(String t, const lgfx::IFont *f = &fonts::Font4) : text(t), fontPtr(f) {}
};
void drawMessage(const std::vector<MessageLine> &lines, uint16_t fgColor = UI_FG, uint16_t bgColor = UI_BG) {
    int numLines = std::min((int)lines.size(), 3);
    int lineHeight = 30; // Расстояние между центрами строк

    displaySprite.fillSprite(bgColor);
    displaySprite.setTextColor(fgColor);
    displaySprite.setTextDatum(middle_center);

    int startY = (SCREEN_HEIGHT / 2) - ((numLines - 1) * lineHeight / 2);
    for (int i = 0; i < numLines; i++) {
        int yPos = startY + (i * lineHeight);
        displaySprite.drawString(lines[i].text, SCREEN_WIDTH / 2, yPos, lines[i].fontPtr);
    }
    displaySprite.pushSprite(0, 0);
    internalState.requiresRedraw = false;
}

// --- ОТРИСОВКА ЭКРАНОВ ---
void renderSplashScreen() {
    displaySprite.fillSprite(UI_BG);
    drawFooter({"   [Any]: Guide      [Enter]: Game"});

    int iconSize = 30;
    int gap = 10;
    int textW = 110;
    int padding = 10;

    // Вычисляем общую ширину рамки
    int rectW = iconSize + gap + textW + (padding * 2);
    int rectH = 40;
    int rectX = (SCREEN_WIDTH - rectW) / 2;
    int rectY = 25;

    // Рисуем двойную рамку
    displaySprite.drawRoundRect(rectX, rectY, rectW, rectH, 8, 0xF800);
    displaySprite.drawRoundRect(rectX - 1, rectY - 1, rectW + 2, rectH + 2, 8, 0xF800);

    int ix = rectX + padding;
    int iy = rectY + (rectH - iconSize) / 2;
    displaySprite.drawRect(ix, iy, iconSize, iconSize, UI_FG);
    displaySprite.fillRect(ix + 1, iy + 1, iconSize - 2, iconSize - 2, 0x52AA);

    // Параметры анимации
    ulong animTime = millis() / 250; // Чуть замедлим для четкости
    int stage = animTime % 7;
    int sz = 5; // Размер ячейки
    int ox = ix + 3;
    int oy = iy + 3;

    // Левый вертикальный путь
    displaySprite.fillRect(ox, oy, sz, sz * 5, 0x0000);
    // Нижний горизонтальный путь
    displaySprite.fillRect(ox, oy + sz * 4, sz * 5, sz, 0x0000);
    // Правый вертикальный путь к выходу
    displaySprite.fillRect(ox + sz * 4, oy, sz, sz * 5, 0x0000);
    // Верхний горизонтальный «мостик»
    displaySprite.fillRect(ox + sz, oy, sz * 4, sz, 0x0000);

    // Выход в углу
    displaySprite.fillRect(ox + sz * 4, oy, sz, sz, 0x07E0);

    // Игрок (ipx, ipy) - координаты соответствуют черным дорожкам
    int ipx = 0, ipy = 0;
    switch (stage) {
    case 0:
        ipx = 0;
        ipy = 0;
        break; // Верхний левый угол
    case 1:
        ipx = 0;
        ipy = 2;
        break; // Вниз по левой стороне
    case 2:
        ipx = 0;
        ipy = 4;
        break; // Нижний левый угол
    case 3:
        ipx = 2;
        ipy = 4;
        break; // Вправо по низу
    case 4:
        ipx = 4;
        ipy = 4;
        break; // Нижний правый угол
    case 5:
        ipx = 4;
        ipy = 2;
        break; // Вверх к выходу
    case 6:
        ipx = 4;
        ipy = 1;
        break; // Почти у цели
    }
    displaySprite.fillRect(ox + ipx * sz, oy + ipy * sz, sz, sz, 0xF800);

    // Текст (внутри рамки справа от иконки)
    displaySprite.setTextColor(UI_FG);
    displaySprite.setTextDatum(middle_left);
    displaySprite.drawString("MiniMaze", ix + iconSize + gap, rectY + (rectH / 2) + 2, &fonts::Font4);

    // Координаты подписей
    int margin = 12;
    int labelX = SCREEN_WIDTH - margin;
    int labelY = SCREEN_HEIGHT - margin - 20;

    displaySprite.setTextColor(0x39E7);
    displaySprite.setTextDatum(bottom_left);
    displaySprite.drawString(FIRMWARE_VERSION, margin, labelY, &fonts::Font0);

    displaySprite.setTextColor(0xFD20);
    displaySprite.setTextDatum(bottom_right);
    displaySprite.drawString("by chillyc0de", labelX, labelY, &fonts::Font2);

    // Пиксельный перец
    int px = labelX - 105;
    int py = labelY - 14;
    int s = 3; // Размер блока перца

    // Тело перца (блоки)
    displaySprite.fillRect(px, py, s * 2, s * 2, 0xF800);                     // Основание
    displaySprite.fillRect(px + s, py + s * 1.5, s * 2.5, s * 2, 0xF800);     // Середина
    displaySprite.fillRect(px + s * 2, py + s * 3, s * 2.5, s * 1.5, 0xD000); // Изгиб
    displaySprite.fillRect(px + s * 4, py + s * 4, s * 1.5, s, 0xA000);       // Кончик

    // Хвостик перца
    displaySprite.fillRect(px, py - s, s * 1.5, s, 0x03E0);
    displaySprite.fillRect(px - s, py - s * 2, s, s, 0x03E0);

    // Пиксельный огонь (8 частиц)
    for (int i = 0; i < 8; i++) {
        ulong t = millis() + (i * 200); // Тайминг
        float duration = 600.0 + (i * 100.0);
        float anim = (float)(t % (int)duration) / duration;

        int lift, stepX, particleW;
        int fCol;

        // Центрируем "сопло" относительно основания перца
        int nozzleX = px + s;

        // Логика поведения частиц
        if (i < 3) // ТРИ ЦЕНТРАЛЬНЫЕ ЧАСТИЦЫ
        {
            lift = anim * 28;                          // Высокий подъем
            stepX = (int)(sin(t * 0.02) * (anim * 2)); // Минимальное отклонение внизу, чуть больше вверху
            particleW = s + 1;                         // Самые толстые

            // Белый -> Оранжевый -> Красный
            if (anim < 0.2) fCol = 0xFFFF;
            else if (anim < 0.6) fCol = 0xFD20;
            else fCol = 0xFA60;
        } else if (i < 6) // БОКОВЫЕ ЯЗЫКИ
        {
            lift = anim * 22;
            // Расширение конусом: чем выше (больше anim), тем сильнее уход в сторону
            int side = (i % 2 == 0) ? 1 : -1;
            stepX = side * (s + (anim * 8));
            particleW = s;

            fCol = (anim < 0.4) ? 0xFD20 : 0xF800;
        } else // ИСКРЫ И ДЫМ
        {
            lift = anim * 35;
            stepX = (int)(sin(t * 0.01)) * 6;
            particleW = 1;

            if (anim < 0.5) fCol = 0xFA60;
            else fCol = 0x4208; // Дым
        }

        int fX = nozzleX + stepX;
        int fY = py - lift;

        if (anim < 0.9) {
            displaySprite.fillRect(fX, fY, particleW, particleW, fCol);

            // ЭФФЕКТ СВЕТОВОГО ПЯТНА
            if (anim < 0.1) displaySprite.fillRect(nozzleX - 1, py, (s * 2), 2, 0xFD20);
        }
    }
}

void renderGuideScreen() {
    displaySprite.fillSprite(UI_BG);
    drawHeader("GUIDE");
    drawScrollbar(internalState.guideScrollY / 18, (SCREEN_HEIGHT - 44) / 18, userGuideLines.size(), 26, SCREEN_HEIGHT - 44);
    drawFooter({"   [Esc]: Back      [Arrows]: Scroll"});

    displaySprite.setClipRect(0, 26, SCREEN_WIDTH - 5, SCREEN_HEIGHT - 44);

    displaySprite.setTextColor(UI_FG);
    displaySprite.setTextDatum(top_left);
    displaySprite.setFont(&fonts::Font2);

    for (int i = 0; i < userGuideLines.size(); i++) {
        int yPos = 30 + (i * 18) - internalState.guideScrollY;
        if (yPos > -18 && yPos < SCREEN_HEIGHT) displaySprite.drawString(userGuideLines[i], 5 - internalState.guideScrollX, yPos);
    }
    displaySprite.clearClipRect();
}

void renderMainMenuScreen() {
    displaySprite.fillSprite(UI_BG);
    drawHeader("MAIN MENU");
    drawScrollbar(internalState.mainMenuScrollOffset, 4, mainMenuOptions.size(), 32, 4 * 20);
    drawFooter({" [Esc]: Guide [Arrows]:Mov [Enter]: Sel"});

    displaySprite.setTextColor(UI_FG);
    displaySprite.setTextDatum(middle_center);

    for (int i = 0; i < 4; i++) {
        int itemIdx = internalState.mainMenuScrollOffset + i;
        if (itemIdx >= mainMenuOptions.size()) break; // Если пунктов меньше 4, выходим раньше

        bool isSel = (itemIdx == internalState.mainMenuSelectedIndex);
        int yPos = 32 + (i * 20); // i от 0 до 3 (позиция на экране)

        displaySprite.fillRect(20, yPos, 200, 18, isSel ? UI_ACCENT : UI_BG);
        displaySprite.drawString(mainMenuOptions[itemIdx].label, SCREEN_WIDTH / 2, yPos + 9, &fonts::Font2);
    }
}

void renderGameAreaScreen() {
    displaySprite.fillSprite(UI_BG);

    const int oX = (SCREEN_WIDTH - (internalState.mazeWidth * internalState.mazeCellSize)) / 2;
    const int oY = (SCREEN_HEIGHT - (internalState.mazeHeight * internalState.mazeCellSize)) / 2;
    for (int x = 0; x < internalState.mazeWidth; x++) {
        for (int y = 0; y < internalState.mazeHeight; y++) {
            uint16_t cellColor = MAZE_EMPTY;
            switch (internalState.mazeCells[x][y]) {
            // Пустота
            case CELL_EMPTY:
                break;
            // Стена
            case CELL_WALL:
                if (internalState.showWalls) cellColor = MAZE_WALL;
                break;
            // Игрок
            case CELL_PLAYER:
                cellColor = MAZE_PLAYER;
                break;
            // Выход
            case CELL_EXIT:
                cellColor = MAZE_EXIT;
                break;
            }

            displaySprite.fillRect(oX + (x * internalState.mazeCellSize), oY + (y * internalState.mazeCellSize), internalState.mazeCellSize - 1, internalState.mazeCellSize - 1, cellColor);
        }
    }
}

void renderSettingsMenuScreen() {
    displaySprite.fillSprite(UI_BG);
    drawHeader("SETTINGS");
    drawScrollbar(internalState.settingsMenuScrollOffset, 4, settingsMenuOptions.size(), 32, 4 * 20);
    drawFooter({" [Esc]: Back [Arrows]: Mov [Enter]: Sel"});

    displaySprite.setTextColor(UI_FG);
    displaySprite.setTextDatum(middle_center);

    for (int i = 0; i < 4; i++) {
        int itemIdx = internalState.settingsMenuScrollOffset + i;
        if (itemIdx >= settingsMenuOptions.size()) break; // Если пунктов меньше 4, выходим раньше

        bool isSel = (itemIdx == internalState.settingsMenuSelectedIndex);
        int yPos = 32 + (i * 20); // i от 0 до 3 (позиция на экране)

        displaySprite.fillRect(20, yPos, 200, 18, isSel ? UI_ACCENT : UI_BG);
        displaySprite.drawString(settingsMenuOptions[itemIdx].label, SCREEN_WIDTH / 2, yPos + 9, &fonts::Font2);
    }
}

void renderGameSetupScreen() {
    // Получаем текущие размеры для понятного отображения
    String presetStr = String(MAZE_PRESETS[internalState.mazePresetIndex].w) + "x" +
        String(MAZE_PRESETS[internalState.mazePresetIndex].h) + "x" +
        String(MAZE_PRESETS[internalState.mazePresetIndex].s);

    String fields[3] = {
        "Maze preset: < " + presetStr + " >",
        "Spawn players: < " + String(internalState.spawnPlayersCount) + " >",
        "Spawn exits: < " + String(internalState.spawnExitsCount) + " >",
    };

    displaySprite.fillSprite(UI_BG);
    drawHeader("GAME SETUP");
    drawFooter({"   [Tab]: Switch    [Arrows]: Change", "   [Esc]: Cancel    [Enter]: Confirm"});

    displaySprite.setTextDatum(top_left);
    displaySprite.setFont(&fonts::Font2);

    for (int i = 0; i < 3; i++) {
        displaySprite.setTextColor(internalState.gameSetupFieldIdx == i ? UI_VALID : UI_FG);
        displaySprite.drawString(fields[i], 10, 40 + (i * 15));
    }
}

void renderBrightnessSetupScreen() {
    displaySprite.fillSprite(UI_BG);
    drawHeader("BRIGHTNESS SETUP");
    drawProgressBar((float)internalState.brightnessSetupBrightnessCounter / 255.0f, UI_VALID);
    drawFooter({"[Esc]: Back  [Arrows]: Led [Enter]: Set"});

    // Обводка палитры
    int w = SCREEN_WIDTH;
    int h = 66;
    int x = 0;
    int y = 31;
    displaySprite.drawRect(x, y, w, h, UI_FG);

    uint16_t testColors[] = {
        UI_FG,
        UI_ACCENT,
        UI_VALID,
        UI_DANGER,
        UI_MUTED,
        UI_BG,
    };

    int numCols = 6;
    int numRows = 2;
    int innerW = w - 2;
    int innerH = h - 2;
    int rowH = innerH / numRows;

    for (int row = 0; row < numRows; row++) {
        for (int col = 0; col < numCols; col++) {
            // Расчет горизонтальных границ внутри рамки
            int x1 = (col * innerW) / numCols;
            int x2 = ((col + 1) * innerW) / numCols;
            int currentBarW = x2 - x1;

            // Смещение индекса для шахматного порядка
            int colorIndex = (col + (row * 3)) % 6;

            // Позиция Y с учетом отступа от рамки (+1)
            int currY = y + 1 + (row * rowH);

            // Рисуем цветной блок
            // x + 1 — это отступ от левого края рамки
            displaySprite.fillRect(x + 1 + x1, currY, currentBarW, rowH, testColors[colorIndex]);
        }
    }
}

void renderVolumeSetupScreen() {
    displaySprite.fillSprite(UI_BG);
    drawHeader("VOLUME SETUP");
    drawProgressBar((float)internalState.volumeSetupVolumeCounter / 255.0f, UI_DANGER);
    drawFooter({"[Esc]: Back  [Arrows]: Vol [Enter]: Set"});

    // Обводка графика
    int w = SCREEN_WIDTH;
    int h = 66;
    int x = 0;
    int y = 31;
    displaySprite.drawRect(x, y, w, h, UI_FG);

    // График
    int numBars = 30;
    float barStep = (float)(w - 4) / numBars;
    int barGap = 2;
    int barW = (int)barStep - barGap;
    float volMultiplier = internalState.volumeSetupVolumeCounter / 255.0f;
    for (int i = 0; i < numBars; i++) {
        float noise = (sin(millis() / 150.0f + i * 0.8f) + 1.0f) / 2.0f;
        int maxH = h - 6;
        int barH = (int)(maxH * volMultiplier * noise);

        if (internalState.volumeSetupVolumeCounter > 0 && barH < 2) barH = 2;

        int bx = x + 2 + (int)(i * barStep);
        int by = y + h - barH - 2;

        // Столбик
        displaySprite.fillRect(bx, by, barW, barH, UI_DANGER);

        // Пик
        displaySprite.fillRect(bx, by - 2, barW, 1, UI_FG);
    }
}

void renderSoundSetupScreen() {
    String fields[4] = {
        "X-Screen: < " + String(internalState.soundSetupExternalState ? "ON" : "OFF") + " >",
        "Keyboard: < " + String(internalState.soundSetupKeyboard ? "ON" : "OFF") + " >",
        "Game: < " + String(internalState.soundSetupGame ? "ON" : "OFF") + " >",
        "Screenshot: < " + String(internalState.soundSetupScreenshot ? "ON" : "OFF") + " >",
    };

    displaySprite.fillSprite(UI_BG);
    drawHeader("SOUND SETUP");
    drawFooter({"   [Tab]: Switch    [Arrows]: Change", "   [Esc]: Cancel    [Enter]: Confirm"});

    displaySprite.setTextDatum(top_left);
    displaySprite.setFont(&fonts::Font2);

    for (int i = 0; i < 4; i++) {
        displaySprite.setTextColor(internalState.soundSetupFieldIdx == i ? UI_VALID : UI_FG);
        displaySprite.drawString(fields[i], 10, 34 + (i * 15));
    }
}

// --- ОБРАБОТЧИКИ ЭКРАНОВ ---
void handleSplashInput(Keyboard_Class::KeysState kState, char kChar, bool isChange) {
    // Any key
    if (isChange) {
        switchExternalState(kState.enter ? STATE_MAIN_MENU : STATE_GUIDE);
    }
}

void handleGuideInput(Keyboard_Class::KeysState kState, char kChar, bool isChange) {
    // Esc
    if (isChange && kChar == '`') {
        switchExternalState(STATE_MAIN_MENU);
        return;
    }

    // Расчет вертикального максимума
    int fontHeight = 18; // Высота символа Font0
    int maxScrollY = max(0, (int)(userGuideLines.size() * fontHeight) - 85);

    // Расчет горизонтального максимума
    int maxChars = 0;
    for (const auto &line : userGuideLines)
        if (line.length() > maxChars) maxChars = line.length();

    int fontWidth = 10; // Ширина символа Font0
    // Вычисляем ширину самой длинной строки минус ширина экрана с учетом полей
    int maxScrollX = max(0, (int)(maxChars * fontWidth) - (SCREEN_WIDTH - 20));

    int step = 10;
    // Up
    if (kChar == ';') internalState.guideScrollY = max(0, internalState.guideScrollY - step);
    // Down
    else if (kChar == '.') internalState.guideScrollY = min(maxScrollY, internalState.guideScrollY + step);
    // Left
    else if (kChar == ',') internalState.guideScrollX = max(0, internalState.guideScrollX - step);
    // Right
    else if (kChar == '/') internalState.guideScrollX = min(maxScrollX, internalState.guideScrollX + step);

    internalState.requiresRedraw = true;
}

void handleMainMenuInput(Keyboard_Class::KeysState kState, char kChar, bool isChange) {
    // Esc
    if (isChange && kChar == '`') {
        switchExternalState(STATE_GUIDE);
        return;
    }
    // Up or Left
    if (kChar == ';' || kChar == ',') {
        if (internalState.mainMenuSelectedIndex > 0) {
            internalState.mainMenuSelectedIndex--;
        } else {
            // Прыжок с первого на последний
            internalState.mainMenuSelectedIndex = mainMenuOptions.size() - 1;
        }

        // Корректировка скролла в видимой области (4 пункта)
        if (internalState.mainMenuSelectedIndex < internalState.mainMenuScrollOffset) {
            // Обычный скролл вверх
            internalState.mainMenuScrollOffset = internalState.mainMenuSelectedIndex;
        } else if (internalState.mainMenuSelectedIndex >= internalState.mainMenuScrollOffset + 4) {
            // Если перепрыгнули в самый конец, показываем последние 4 элемента
            int newOffset = (int)mainMenuOptions.size() - 4;
            internalState.mainMenuScrollOffset = (newOffset > 0) ? newOffset : 0;
        }
        internalState.requiresRedraw = true;
        return;
    }
    // Down or Right
    if (kChar == '.' || kChar == '/') {
        if (internalState.mainMenuSelectedIndex < (int)mainMenuOptions.size() - 1) {
            internalState.mainMenuSelectedIndex++;
        } else {
            // Прыжок с последнего на первый
            internalState.mainMenuSelectedIndex = 0;
        }

        // Корректировка скролла в видимой области (4 пункта)
        if (internalState.mainMenuSelectedIndex >= internalState.mainMenuScrollOffset + 4) {
            // Обычный скролл вниз
            internalState.mainMenuScrollOffset = internalState.mainMenuSelectedIndex - 3;
        } else if (internalState.mainMenuSelectedIndex < internalState.mainMenuScrollOffset) {
            // Если перепрыгнули в самое начало, сбрасываем смещение
            internalState.mainMenuScrollOffset = 0;
        }
        internalState.requiresRedraw = true;
        return;
    }
    // Enter
    if (isChange && kState.enter) {
        mainMenuOptions[internalState.mainMenuSelectedIndex].action();
        return;
    }
}

void handleGameAreaInput(Keyboard_Class::KeysState kState, char kChar, bool isChange) {
    const char lkChar = tolower(kChar);

    // Esc
    if (isChange && kChar == '`') {
        switchExternalState(STATE_MAIN_MENU);
        return;
    }
    // Tab or Fn
    if (isChange && (kState.tab || kState.fn)) {
        // Переключение видимости стен
        internalState.showWalls = !internalState.showWalls;
        internalState.requiresRedraw = true;
        return;
    }
    // Ctrl or Opt
    if (isChange && (kState.ctrl || kState.opt)) {
        // Переключение в автопрохождение
        internalState.autoPlay = !internalState.autoPlay;
        internalState.requiresRedraw = true;
        return;
    }
    // Del or '\'
    if (isChange && (kState.del || lkChar == '\\')) {
        // Перегенерация лабиринта с новым спавном
        generateMaze();
        spawnPlayers();
        spawnExits();
        internalState.requiresRedraw = true;
        return;
    }
    // '-' or '['
    if (isChange && (lkChar == '-' || lkChar == '[')) {
        // Переключение по кругу назад
        internalState.mazePresetIndex = (internalState.mazePresetIndex == 0)
            ? MAZE_PRESETS.size() - 1
            : internalState.mazePresetIndex - 1;
        internalState.mazeWidth = MAZE_PRESETS[internalState.mazePresetIndex].w;
        internalState.mazeHeight = MAZE_PRESETS[internalState.mazePresetIndex].h;
        internalState.mazeCellSize = MAZE_PRESETS[internalState.mazePresetIndex].s;

        // Генерация лабиринта с новыми размерами
        generateMaze();
        spawnPlayers();
        spawnExits();
        internalState.requiresRedraw = true;
        return;
    }
    // '=' or ']'
    if (isChange && (lkChar == '=' || lkChar == ']')) {
        // Переключение по кругу вперед
        internalState.mazePresetIndex = (internalState.mazePresetIndex + 1) % MAZE_PRESETS.size();
        internalState.mazeWidth = MAZE_PRESETS[internalState.mazePresetIndex].w;
        internalState.mazeHeight = MAZE_PRESETS[internalState.mazePresetIndex].h;
        internalState.mazeCellSize = MAZE_PRESETS[internalState.mazePresetIndex].s;

        // Генерация лабиринта с новыми размерами
        generateMaze();
        spawnPlayers();
        spawnExits();
        internalState.requiresRedraw = true;
        return;
    }
    // Enter or ' '
    if (isChange && (kState.enter || lkChar == ' ')) {
        // Спавн игроков и выходов
        spawnPlayers();
        spawnExits();
        internalState.requiresRedraw = true;
        return;
    }

    int nX = 0, nY = 0;
    // При автопрожождении не работает ручное управление
    if (!internalState.autoPlay) {
        // Up
        if (lkChar == ';' || strchr("567tyu", lkChar)) nY--;
        // Down
        else if (lkChar == '.' || strchr("fghcvb", lkChar)) nY++;
        // Left
        else if (lkChar == ',' || strchr("werasd", lkChar)) nX--;
        // Right
        else if (lkChar == '/' || strchr("iopjkl", lkChar)) nX++;
    }

    // Если есть движение
    if (nX != 0 || nY != 0) {
        // Сначала собираем координаты всех игроков, чтобы не двигать одного и того же дважды
        std::vector<std::pair<int, int>> playerCells;
        int exitsCount = 0;
        for (int x = 0; x < internalState.mazeWidth; x++) {
            for (int y = 0; y < internalState.mazeHeight; y++) {
                if (internalState.mazeCells[x][y] == CELL_EXIT) exitsCount++;
                if (internalState.mazeCells[x][y] == CELL_PLAYER) playerCells.push_back({x, y});
            }
        }
        int playersCount = playerCells.size();

        // Двигаем найденных игроков
        MazeCell highestCellType = CELL_EMPTY;
        for (auto &pos : playerCells) {
            int x = pos.first;
            int y = pos.second;

            // Защита от выхода за границы массива
            if (x + nX < 0 || x + nX >= internalState.mazeWidth || y + nY < 0 || y + nY >= internalState.mazeHeight) continue;

            // Целевая ячейка для движения
            switch (internalState.mazeCells[x + nX][y + nY]) {
            // Пустота
            case CELL_EMPTY:
                // Опустошаем пройденную ячейку и переходим на новую
                internalState.mazeCells[x][y] = CELL_EMPTY;
                internalState.mazeCells[x + nX][y + nY] = CELL_PLAYER;
                break;
            // Стена
            case CELL_WALL:
                // Нет движения
                highestCellType = max(highestCellType, CELL_WALL);
                break;
            // Другой игрок
            case CELL_PLAYER:
                // Нет движения
                highestCellType = max(highestCellType, CELL_PLAYER);
                break;
            // Выход
            case CELL_EXIT:
                // Убираем игрока и выход
                internalState.mazeCells[x][y] = CELL_EMPTY;
                internalState.mazeCells[x + nX][y + nY] = CELL_EMPTY;
                playersCount--;
                exitsCount--;
                highestCellType = max(highestCellType, CELL_EXIT);
                break;
            }
        }

        // Только один звук для всех игроков
        switch (highestCellType) {
        case CELL_EMPTY:
            playMazeCellTone(CELL_EMPTY);
            break;
        case CELL_WALL:
            playMazeCellTone(CELL_WALL);
            break;
        case CELL_PLAYER:
            playMazeCellTone(CELL_PLAYER);
            break;
        case CELL_EXIT:
            playMazeCellTone(CELL_EXIT);
            break;
        }

        // Спавн новых игроков и выходов
        if (playersCount == 0) spawnPlayers();
        if (exitsCount == 0) spawnExits();

        internalState.requiresRedraw = true;
        return;
    }
}

void handleSettingsMenuInput(Keyboard_Class::KeysState kState, char kChar, bool isChange) {
    // Esc
    if (isChange && kChar == '`') {
        switchExternalState(STATE_MAIN_MENU);
        return;
    }
    // Up or Left
    if (kChar == ';' || kChar == ',') {
        if (internalState.settingsMenuSelectedIndex > 0) {
            internalState.settingsMenuSelectedIndex--;
        } else {
            // Прыжок с первого на последний
            internalState.settingsMenuSelectedIndex = settingsMenuOptions.size() - 1;
        }

        // Корректировка скролла в видимой области (4 пункта)
        if (internalState.settingsMenuSelectedIndex < internalState.settingsMenuScrollOffset) {
            // Обычный скролл вверх
            internalState.settingsMenuScrollOffset = internalState.settingsMenuSelectedIndex;
        } else if (internalState.settingsMenuSelectedIndex >= internalState.settingsMenuScrollOffset + 4) {
            // Если перепрыгнули в самый конец, показываем последние 4 элемента
            int newOffset = (int)settingsMenuOptions.size() - 4;
            internalState.settingsMenuScrollOffset = (newOffset > 0) ? newOffset : 0;
        }
        internalState.requiresRedraw = true;
        return;
    }
    // Down or Right
    if (kChar == '.' || kChar == '/') {
        if (internalState.settingsMenuSelectedIndex < (int)settingsMenuOptions.size() - 1) {
            internalState.settingsMenuSelectedIndex++;
        } else {
            // Прыжок с последнего на первый
            internalState.settingsMenuSelectedIndex = 0;
        }

        // Корректировка скролла в видимой области (4 пункта)
        if (internalState.settingsMenuSelectedIndex >= internalState.settingsMenuScrollOffset + 4) {
            // Обычный скролл вниз
            internalState.settingsMenuScrollOffset = internalState.settingsMenuSelectedIndex - 3;
        } else if (internalState.settingsMenuSelectedIndex < internalState.settingsMenuScrollOffset) {
            // Если перепрыгнули в самое начало, сбрасываем смещение
            internalState.settingsMenuScrollOffset = 0;
        }
        internalState.requiresRedraw = true;
        return;
    }
    // Enter
    if (isChange && kState.enter) {
        settingsMenuOptions[internalState.settingsMenuSelectedIndex].action();
        return;
    }
}

void handleGameSetupInput(Keyboard_Class::KeysState kState, char kChar, bool isChange) {
    // Esc
    if (isChange && kChar == '`') {

        internalState.mazePresetIndex = systemPreferences.getInt("MM/mz_pr_i", DEFAULT_MAZE_PRESET_INDEX);
        internalState.mazeWidth = MAZE_PRESETS[internalState.mazePresetIndex].w;
        internalState.mazeHeight = MAZE_PRESETS[internalState.mazePresetIndex].h;
        internalState.mazeCellSize = MAZE_PRESETS[internalState.mazePresetIndex].s;

        internalState.spawnPlayersCount = systemPreferences.getInt("MM/spwn_p", DEFAULT_SPAWN_PLAYERS_COUNT);
        internalState.spawnExitsCount = systemPreferences.getInt("MM/spwn_e", DEFAULT_SPAWN_EXITS_COUNT);

        switchExternalState(STATE_SETTINGS_MENU);
        return;
    }
    // Tab
    if (kState.tab) {
        internalState.gameSetupFieldIdx = (internalState.gameSetupFieldIdx + 1) % 3;
        internalState.requiresRedraw = true;
        return;
    }
    // Up or Left
    if (kChar == ';' || kChar == ',') {
        switch (internalState.gameSetupFieldIdx) {
        case 0: // Пресет размеров лабиринта (по кругу назад)
            internalState.mazePresetIndex = (internalState.mazePresetIndex == 0)
                ? MAZE_PRESETS.size() - 1
                : internalState.mazePresetIndex - 1;
            internalState.mazeWidth = MAZE_PRESETS[internalState.mazePresetIndex].w;
            internalState.mazeHeight = MAZE_PRESETS[internalState.mazePresetIndex].h;
            internalState.mazeCellSize = MAZE_PRESETS[internalState.mazePresetIndex].s;
            break;
        case 1: // Количество спавна игроков (минимум 1)
            if (internalState.spawnPlayersCount > 1) internalState.spawnPlayersCount--;
            break;
        case 2: // Количество спавна выходов (минимум 1)
            if (internalState.spawnExitsCount > 1) internalState.spawnExitsCount--;
            break;
        }
        internalState.requiresRedraw = true;
        return;
    }
    // Down or Right
    if (kChar == '.' || kChar == '/') {
        switch (internalState.gameSetupFieldIdx) {
        case 0: // Пресет размеров лабиринта (по кругу вперед)
            internalState.mazePresetIndex = (internalState.mazePresetIndex + 1) % MAZE_PRESETS.size();
            internalState.mazeWidth = MAZE_PRESETS[internalState.mazePresetIndex].w;
            internalState.mazeHeight = MAZE_PRESETS[internalState.mazePresetIndex].h;
            internalState.mazeCellSize = MAZE_PRESETS[internalState.mazePresetIndex].s;
            break;
        case 1: // Количество спавна игроков (максимум 50)
            if (internalState.spawnPlayersCount < 50) internalState.spawnPlayersCount++;
            break;
        case 2: // Количество спавна выходов (максимум 50)
            if (internalState.spawnExitsCount < 50) internalState.spawnExitsCount++;
            break;
        }
        internalState.requiresRedraw = true;
        return;
    }
    // Enter
    if (isChange && kState.enter) {

        systemPreferences.putInt("MM/mz_pr_i", internalState.mazePresetIndex);
        systemPreferences.putInt("MM/spwn_p", internalState.spawnPlayersCount);
        systemPreferences.putInt("MM/spwn_e", internalState.spawnExitsCount);

        // Синхронизируем рабочие переменные лабиринта с новым пресетом,
        // чтобы при следующем Play Game сгенерировался правильный размер
        internalState.mazeWidth = MAZE_PRESETS[internalState.mazePresetIndex].w;
        internalState.mazeHeight = MAZE_PRESETS[internalState.mazePresetIndex].h;
        internalState.mazeCellSize = MAZE_PRESETS[internalState.mazePresetIndex].s;

        switchExternalState(STATE_SETTINGS_MENU);
        return;
    }
}

void handleBrightnessSetupInput(Keyboard_Class::KeysState kState, char kChar, bool isChange) {
    // Esc
    if (isChange && kChar == '`') {

        internalState.brightnessSetupBrightnessCounter = systemPreferences.getInt("MM/brght", DEFAULT_BRIGHTNESS);

        M5.Display.setBrightness(internalState.brightnessSetupBrightnessCounter);
        switchExternalState(STATE_SETTINGS_MENU);
        return;
    }
    // Up or Right
    if (kChar == ';' || kChar == '/') {
        internalState.brightnessSetupBrightnessCounter = min(255, internalState.brightnessSetupBrightnessCounter + 5);
        M5.Display.setBrightness(internalState.brightnessSetupBrightnessCounter); // Предпросмотр
        internalState.requiresRedraw = true;
        return;
    }
    // Down or Left
    if (kChar == '.' || kChar == ',') {
        internalState.brightnessSetupBrightnessCounter = max(0, internalState.brightnessSetupBrightnessCounter - 5);
        M5.Display.setBrightness(internalState.brightnessSetupBrightnessCounter); // Предпросмотр
        internalState.requiresRedraw = true;
        return;
    }
    // Enter
    if (isChange && kState.enter) {

        systemPreferences.putInt("MM/brght", internalState.brightnessSetupBrightnessCounter);

        switchExternalState(STATE_SETTINGS_MENU);
        return;
    }
}

void handleVolumeSetupInput(Keyboard_Class::KeysState kState, char kChar, bool isChange) {
    // Esc
    if (isChange && kChar == '`') {

        internalState.volumeSetupVolumeCounter = systemPreferences.getInt("MM/vol", DEFAULT_VOLUME);

        M5.Speaker.setVolume(internalState.volumeSetupVolumeCounter);
        switchExternalState(STATE_SETTINGS_MENU);
        return;
    }
    // Up or Right
    if (kChar == ';' || kChar == '/') {
        internalState.volumeSetupVolumeCounter = min(255, internalState.volumeSetupVolumeCounter + 5);
        M5.Speaker.setVolume(internalState.volumeSetupVolumeCounter); // Предпросмотр

        // Тестовый звук
        if (internalState.volumeSetupVolumeCounter > 0) {
            // Восходящий звук
            M5.Speaker.tone(880, 60);
            delay(30);
            M5.Speaker.tone(1200, 60);
        }

        internalState.requiresRedraw = true;
        return;
    }
    // Down or Left
    if (kChar == '.' || kChar == ',') {
        internalState.volumeSetupVolumeCounter = max(0, internalState.volumeSetupVolumeCounter - 5);
        M5.Speaker.setVolume(internalState.volumeSetupVolumeCounter); // Предпросмотр

        // Тестовый звук
        if (internalState.volumeSetupVolumeCounter > 0) {
            // Нисходящий звук
            M5.Speaker.tone(1200, 60);
            delay(30);
            M5.Speaker.tone(880, 60);
        }

        internalState.requiresRedraw = true;
        return;
    }
    // Enter
    if (isChange && kState.enter) {

        systemPreferences.putInt("MM/vol", internalState.volumeSetupVolumeCounter);

        switchExternalState(STATE_SETTINGS_MENU);
        return;
    }
}

void handleSoundSetupInput(Keyboard_Class::KeysState kState, char kChar, bool isChange) {
    // Esc
    if (isChange && kChar == '`') {

        internalState.soundSetupExternalState = systemPreferences.getBool("MM/snd_es", DEFAULT_SOUND_ES);
        internalState.soundSetupKeyboard = systemPreferences.getBool("MM/snd_kbd", DEFAULT_SOUND_KBD);
        internalState.soundSetupGame = systemPreferences.getBool("MM/snd_gm", DEFAULT_SOUND_GM);
        internalState.soundSetupScreenshot = systemPreferences.getBool("MM/snd_scr", DEFAULT_SOUND_SCR);

        switchExternalState(STATE_SETTINGS_MENU);
        return;
    }
    // Tab
    if (kState.tab) {
        internalState.soundSetupFieldIdx = (internalState.soundSetupFieldIdx + 1) % 4;
        internalState.requiresRedraw = true;
        return;
    }
    // Up or Left
    if (kChar == ';' || kChar == ',') {
        switch (internalState.soundSetupFieldIdx) {
        case 0: // Переходы
            internalState.soundSetupExternalState = !internalState.soundSetupExternalState;
            break;
        case 1: // Клавиатура
            internalState.soundSetupKeyboard = !internalState.soundSetupKeyboard;
            break;
        case 2: // Игра
            internalState.soundSetupGame = !internalState.soundSetupGame;
            break;
        case 3: // Скриншот
            internalState.soundSetupScreenshot = !internalState.soundSetupScreenshot;
            break;
        }
        internalState.requiresRedraw = true;
        return;
    }
    // Down or Right
    if (kChar == '.' || kChar == '/') {
        switch (internalState.soundSetupFieldIdx) {
        case 0: // Переходы
            internalState.soundSetupExternalState = !internalState.soundSetupExternalState;
            break;
        case 1: // Клавиатура
            internalState.soundSetupKeyboard = !internalState.soundSetupKeyboard;
            break;
        case 2: // Игра
            internalState.soundSetupGame = !internalState.soundSetupGame;
            break;
        case 3: // Скриншот
            internalState.soundSetupScreenshot = !internalState.soundSetupScreenshot;
            break;
        }
        internalState.requiresRedraw = true;
        return;
    }
    // Enter
    if (isChange && kState.enter) {

        systemPreferences.putBool("MM/snd_es", internalState.soundSetupExternalState);
        systemPreferences.putBool("MM/snd_kbd", internalState.soundSetupKeyboard);
        systemPreferences.putBool("MM/snd_gm", internalState.soundSetupGame);
        systemPreferences.putBool("MM/snd_scr", internalState.soundSetupScreenshot);

        switchExternalState(STATE_SETTINGS_MENU);
        return;
    }
}

// --- ГЛАВНЫЙ ОБРАБОТЧИК КЛАВИАТУРЫ ---
void processKeyboardEvents() {
    // Если нажатая клавиша изменилась
    bool isChange = M5Cardputer.Keyboard.isChange();
    // Если нажата любая клавиша
    bool isPressed = M5Cardputer.Keyboard.isPressed();

    if (!isPressed) return; // Нет нажатия, нечего обрабатывать

    // Модификатор
    auto kState = M5Cardputer.Keyboard.keysState();
    // Символ
    char kChar = kState.word.size() > 0 ? kState.word[0] : 0;

    // Динамическая задержка скролла
    int repeatDelay = 150;
    switch (internalState.currentExternalState) {
    case STATE_GUIDE:
        repeatDelay = 50;
        break;
    case STATE_GAME_AREA:
        repeatDelay = 230;
        break;
    case STATE_BRIGHTNESS_SETUP:
    case STATE_VOLUME_SETUP:
        repeatDelay = 170;
        break;
    case STATE_GAME_SETUP:
    case STATE_SOUND_SETUP:
    case STATE_MAIN_MENU:
    case STATE_SETTINGS_MENU:
        repeatDelay = 250;
        break;
    }

    static ulong lastKeyPressTime = 0;
    if (isChange || (millis() - lastKeyPressTime > repeatDelay)) {
        lastKeyPressTime = millis();

        // Звуковая индикация вне игрового поля
        if (internalState.currentExternalState != STATE_GAME_AREA && internalState.soundSetupKeyboard) playKeyTone(kChar, kState);

        switch (internalState.currentExternalState) {
        case STATE_SPLASH_SCREEN:
            handleSplashInput(kState, kChar, isChange);
            break;
        case STATE_GUIDE:
            handleGuideInput(kState, kChar, isChange);
            break;
        case STATE_MAIN_MENU:
            handleMainMenuInput(kState, kChar, isChange);
            break;
        case STATE_GAME_AREA:
            handleGameAreaInput(kState, kChar, isChange);
            break;
        case STATE_SETTINGS_MENU:
            handleSettingsMenuInput(kState, kChar, isChange);
            break;
        case STATE_GAME_SETUP:
            handleGameSetupInput(kState, kChar, isChange);
            break;
        case STATE_BRIGHTNESS_SETUP:
            handleBrightnessSetupInput(kState, kChar, isChange);
            break;
        case STATE_VOLUME_SETUP:
            handleVolumeSetupInput(kState, kChar, isChange);
            break;
        case STATE_SOUND_SETUP:
            handleSoundSetupInput(kState, kChar, isChange);
            break;
        }
    }
}

// --- ГЛАВНЫЙ ОТРИСОВЩИК ЭКРАНА ---
void renderUserInterface() {
    if (!internalState.requiresRedraw) return;
    displaySprite.clearClipRect();

    switch (internalState.currentExternalState) {
    case STATE_SPLASH_SCREEN:
        renderSplashScreen();
        break;
    case STATE_GUIDE:
        renderGuideScreen();
        break;
    case STATE_MAIN_MENU:
        renderMainMenuScreen();
        break;
    case STATE_GAME_AREA:
        renderGameAreaScreen();
        break;
    case STATE_SETTINGS_MENU:
        renderSettingsMenuScreen();
        break;
    case STATE_GAME_SETUP:
        renderGameSetupScreen();
        break;
    case STATE_BRIGHTNESS_SETUP:
        renderBrightnessSetupScreen();
        break;
    case STATE_VOLUME_SETUP:
        renderVolumeSetupScreen();
        break;
    case STATE_SOUND_SETUP:
        renderSoundSetupScreen();
        break;
    }

    displaySprite.pushSprite(0, 0);
    internalState.requiresRedraw = false;
}

// --- ОБРАБОТЧИК СКРИНШОТОВ ---
void processScreenshotEvent() {
    if (!M5Cardputer.BtnA.isPressed()) return;

    static ulong lastScreenshotTime = 0;
    if (millis() - lastScreenshotTime < 1000) return;
    lastScreenshotTime = millis();

    ensureDirectoryExists(SCREENSHOTS_DIR_PATH);

    // Инициализация индекса (поиск максимума) при первом вызове
    static int nextFileIndex = 0;
    if (nextFileIndex == 0) {
        int maxIndex = 0;
        String dirPath = SCREENSHOTS_DIR_PATH;
        if (dirPath.endsWith("/")) dirPath.remove(dirPath.length() - 1);

        File scrDir = SD.open(dirPath);
        if (scrDir && scrDir.isDirectory()) {
            File scrFile = scrDir.openNextFile();
            while (scrFile) {
                // Используем полное имя файла из объекта File
                const char *fileName = scrFile.name();

                // Ищем "scr_" в имени
                const char *found = strstr(fileName, "scr_");
                if (found) {
                    // Смещаем указатель на длину строки "scr_"
                    // И берем число сразу после "scr_"
                    int idx = atoi(found + 4);
                    if (idx > maxIndex) maxIndex = idx;
                }
                scrFile.close();
                scrFile = scrDir.openNextFile();
            }
            scrDir.close();
        }
        nextFileIndex = maxIndex + 1;
    }

    // Формируем финальное имя и путь
    char fileName[32];
    char filePath[128];
    snprintf(fileName, sizeof(fileName), "scr_%04u.bmp", nextFileIndex);
    snprintf(filePath, sizeof(filePath), "%s%s", SCREENSHOTS_DIR_PATH, fileName);

    // Запись файла
    File file = SD.open(filePath, FILE_WRITE);
    if (!file) return;

    uint32_t fileSize = 54 + (SCREEN_WIDTH * SCREEN_HEIGHT * 3);
    uint8_t header[54] = {
        'B',
        'M',
        (uint8_t)(fileSize),
        (uint8_t)(fileSize >> 8),
        (uint8_t)(fileSize >> 16),
        (uint8_t)(fileSize >> 24),
        0,
        0,
        0,
        0,
        54,
        0,
        0,
        0,
        40,
        0,
        0,
        0,
        240,
        0,
        0,
        0,
        121,
        255,
        255,
        255,
        1,
        0,
        24,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    };
    file.write(header, 54);

    uint8_t lineBuffer[SCREEN_WIDTH * 3];
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        int pos = 0;
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int color = displaySprite.readPixel(x, y);
            lineBuffer[pos++] = (color & 0x001F) << 3; // B
            lineBuffer[pos++] = (color & 0x07E0) >> 3; // G
            lineBuffer[pos++] = (color & 0xF800) >> 8; // R
        }
        file.write(lineBuffer, sizeof(lineBuffer));
    }
    file.close();

    // Инкремент для следующего файла
    nextFileIndex++;

    drawMessage({"SCREENSHOT", "SAVED", fileName});
    // Звуковая индикация
    if (internalState.volumeSetupVolumeCounter > 0 && internalState.soundSetupScreenshot) {
        M5.Speaker.tone(1000, 60);
        delay(40);
        M5.Speaker.tone(900, 60);
        delay(40);
        M5.Speaker.tone(800, 60);
    }
    delay(600);

    // Принудительное стирание сообщения с экрана
    internalState.requiresRedraw = true;
}

void setup() {
    M5Cardputer.begin(M5.config(), true);
    M5.Lcd.setRotation(1);
    displaySprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);

    systemPreferences.begin("by_chillyc0de");
    internalState.brightnessSetupBrightnessCounter = systemPreferences.getInt("MM/brght", DEFAULT_BRIGHTNESS);
    M5.Display.setBrightness(internalState.brightnessSetupBrightnessCounter);

    internalState.volumeSetupVolumeCounter = systemPreferences.getInt("MM/vol", DEFAULT_VOLUME);
    M5.Speaker.setVolume(internalState.volumeSetupVolumeCounter);

    internalState.soundSetupExternalState = systemPreferences.getBool("MM/snd_es", DEFAULT_SOUND_ES);
    internalState.soundSetupKeyboard = systemPreferences.getBool("MM/snd_kbd", DEFAULT_SOUND_KBD);
    internalState.soundSetupGame = systemPreferences.getBool("MM/snd_gm", DEFAULT_SOUND_GM);
    internalState.soundSetupScreenshot = systemPreferences.getBool("MM/snd_scr", DEFAULT_SOUND_SCR);

    internalState.mazePresetIndex = systemPreferences.getInt("MM/mz_pr_i", DEFAULT_MAZE_PRESET_INDEX);
    internalState.mazeWidth = MAZE_PRESETS[internalState.mazePresetIndex].w;
    internalState.mazeHeight = MAZE_PRESETS[internalState.mazePresetIndex].h;
    internalState.mazeCellSize = MAZE_PRESETS[internalState.mazePresetIndex].s;

    internalState.spawnPlayersCount = systemPreferences.getInt("MM/spwn_p", DEFAULT_SPAWN_PLAYERS_COUNT);
    internalState.spawnExitsCount = systemPreferences.getInt("MM/spwn_e", DEFAULT_SPAWN_EXITS_COUNT);

    SPI.begin(40, 39, 14, 12);
    // Проверка наличия SD
    if (!SD.begin(12, SPI, 15000000)) { // 25000000
        drawMessage({"SD NOT FOUND", {"Insert SD and reboot", &fonts::Font2}});
        while (true) delay(10000);
    }

    switchExternalState(STATE_SPLASH_SCREEN);
}

void loop() {
    switch (internalState.currentExternalState) {
    case STATE_SPLASH_SCREEN:
        // Анимация на экране загрузки
        static ulong lastSplashAnimationTime = 0;
        // Частота кадров 1к/33мс = 30к/с
        if (millis() - lastSplashAnimationTime > 33) {
            lastSplashAnimationTime = millis();
            internalState.requiresRedraw = true;
        }
        break;
    case STATE_GAME_AREA:
        // Пошаговое автопрохождение
        static ulong lastAutoPlayTime = 0;
        // Частота шагов 1ш/100мс = 10ш/с
        if (internalState.autoPlay && (millis() - lastAutoPlayTime > 100)) {
            solveMaze();
            lastAutoPlayTime = millis();
            internalState.requiresRedraw = true;
        }
        break;
    case STATE_VOLUME_SETUP:
        // Анимация спектра на экране настройки звука
        static ulong lastSpectrumAnimationTime = 0;
        // Частота кадров 1к/16мс = 62.5к/с
        if (millis() - lastSpectrumAnimationTime > 16) {
            lastSpectrumAnimationTime = millis();
            internalState.requiresRedraw = true;
        }
        break;
    }

    M5Cardputer.update();

    // Захват экрана перед главным обработчиком клавиатуры
    processScreenshotEvent();

    processKeyboardEvents();
    renderUserInterface();
}