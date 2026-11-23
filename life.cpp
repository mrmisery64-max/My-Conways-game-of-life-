// Conway's Game of Life — single-file Windows app (no terminal).
// Build: clang++ -std=c++17 -o life.exe life.cpp
//
// Mouse:  left-drag = draw cells, right-drag = erase cells
// Keys:   Space = play/pause, S = step one generation, C = clear
// RULES
// Any live cell with fewer than two live neighbours dies, as if by underpopulation.
// Any live cell with two or three live neighbours lives on to the next generation.
// Any live cell with more than three live neighbours dies, as if by overpopulation.
// Any dead cell with exactly three live neighbours becomes a live cell, as if by reproduction.


#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

const int W = 80, H = 50;   // grid size in cells
const int CELL = 12;        // cell size in pixels
const int BAR = 40;         // toolbar height in pixels
const int SPEEDS[] = {1, 2, 5, 10, 20, 40};  // generations per second
const int NSPEEDS = sizeof(SPEEDS) / sizeof(SPEEDS[0]);

enum { ID_PLAY = 1, ID_STEP, ID_SPEED, ID_CLEAR, TIMER_ID = 1 };

std::vector<char> grid(W * H), nextGrid(W * H);
bool running = false;
int speedIdx = 2;
int generation = 0;
char drawValue = 1;          // 1 while drawing, 0 while erasing
int lastX = 0, lastY = 0;    // last mouse position while dragging
HWND btnPlay, btnSpeed;
HBRUSH aliveBrush, deadBrush, lineBrush;

char& cell(int x, int y) { return grid[y * W + x]; }

void step() {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int n = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (dx || dy)
                        n += cell((x + dx + W) % W, (y + dy + H) % H);  // edges wrap
            nextGrid[y * W + x] = (n == 3 || (n == 2 && cell(x, y)));
        }
    grid.swap(nextGrid);
    ++generation;
}

// Set the cell under pixel (px, py), if it's inside the grid.
void setCellAt(int px, int py) {
    if (px < 0 || py < BAR) return;
    int x = px / CELL, y = (py - BAR) / CELL;
    if (x < W && y < H) cell(x, y) = drawValue;
}

// Fill every cell along the mouse path so fast drags don't leave gaps.
void drawLine(int x0, int y0, int x1, int y1) {
    int n = std::max(std::abs(x1 - x0), std::abs(y1 - y0)) / (CELL / 2) + 1;
    for (int i = 0; i <= n; ++i)
        setCellAt(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n);
}

void updateUI(HWND hwnd) {
    SetWindowTextA(btnPlay, running ? "Pause" : "Play");
    SetWindowTextA(btnSpeed, ("Speed: " + std::to_string(SPEEDS[speedIdx]) + "/s").c_str());
    KillTimer(hwnd, TIMER_ID);
    if (running) SetTimer(hwnd, TIMER_ID, 1000 / SPEEDS[speedIdx], nullptr);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);

    // Draw into an off-screen bitmap first so the window doesn't flicker.
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);

    // Toolbar background and status text
    RECT bar = {0, 0, rc.right, BAR};
    FillRect(mem, &bar, GetSysColorBrush(COLOR_BTNFACE));
    int alive = 0;
    for (char c : grid) alive += c;
    std::string info = "Gen " + std::to_string(generation) + "    Cells " + std::to_string(alive) +
                       "        Left-drag: draw    Right-drag: erase    Space: play/pause";
    SelectObject(mem, GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(mem, TRANSPARENT);
    TextOutA(mem, 330, 13, info.c_str(), (int)info.size());

    // Grid: fill with line color, then draw each cell one pixel smaller.
    RECT area = {0, BAR, rc.right, rc.bottom};
    FillRect(mem, &area, lineBrush);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            RECT r = {x * CELL, BAR + y * CELL, x * CELL + CELL - 1, BAR + y * CELL + CELL - 1};
            FillRect(mem, &r, cell(x, y) ? aliveBrush : deadBrush);
        }

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        auto button = [&](const char* text, int id, int x, int w) {
            HWND b = CreateWindowA("BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   x, 7, w, 26, hwnd, (HMENU)(INT_PTR)id, nullptr, nullptr);
            SendMessageA(b, WM_SETFONT, (WPARAM)font, TRUE);
            return b;
        };
        btnPlay  = button("Play",  ID_PLAY,  8,   70);
                   button("Step",  ID_STEP,  84,  70);
        btnSpeed = button("Speed", ID_SPEED, 160, 90);
                   button("Clear", ID_CLEAR, 256, 64);
        updateUI(hwnd);
        return 0;
    }
    case WM_COMMAND:  // button clicks (and keyboard shortcuts, which send the same IDs)
        switch (LOWORD(wp)) {
        case ID_PLAY:  running = !running; break;
        case ID_STEP:  step(); break;
        case ID_SPEED: speedIdx = (speedIdx + 1) % NSPEEDS; break;
        case ID_CLEAR: std::fill(grid.begin(), grid.end(), 0); generation = 0; break;
        }
        SetFocus(hwnd);  // keep keyboard focus on the main window, not the button
        updateUI(hwnd);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_SPACE) SendMessageA(hwnd, WM_COMMAND, ID_PLAY, 0);
        if (wp == 'S')      SendMessageA(hwnd, WM_COMMAND, ID_STEP, 0);
        if (wp == 'C')      SendMessageA(hwnd, WM_COMMAND, ID_CLEAR, 0);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        SetCapture(hwnd);
        drawValue = (msg == WM_LBUTTONDOWN);
        lastX = GET_X_LPARAM(lp);
        lastY = GET_Y_LPARAM(lp);
        setCellAt(lastX, lastY);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_MOUSEMOVE:
        if (GetCapture() == hwnd) {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            drawLine(lastX, lastY, x, y);
            lastX = x;
            lastY = y;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        ReleaseCapture();
        return 0;
    case WM_TIMER:
        step();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_ERASEBKGND:
        return 1;  // paint() covers everything
    case WM_PAINT:
        paint(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show) {
    aliveBrush = CreateSolidBrush(RGB(80, 220, 120));
    deadBrush  = CreateSolidBrush(RGB(24, 24, 28));
    lineBrush  = CreateSolidBrush(RGB(45, 45, 52));

    // Start with a glider so there's something to watch.
    cell(2, 1) = cell(3, 2) = cell(1, 3) = cell(2, 3) = cell(3, 3) = 1;

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "LifeWindow";
    RegisterClassA(&wc);

    DWORD style = (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX) | WS_CLIPCHILDREN;
    RECT r = {0, 0, W * CELL, H * CELL + BAR};
    AdjustWindowRect(&r, style, FALSE);
    HWND hwnd = CreateWindowA("LifeWindow", "Game of Life", style, CW_USEDEFAULT, CW_USEDEFAULT,
                              r.right - r.left, r.bottom - r.top, nullptr, nullptr, inst, nullptr);
    ShowWindow(hwnd, show);

    MSG m;
    while (GetMessageA(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    return 0;
}

