#pragma once
#include <math.h>
#include "CGE.h"

CGE::CGE(LPCWSTR title, const tVector2<int>& pixelSize, const tVector2<int>& screenSize, bool thirdDimension)
{
    hSTDout = GetStdHandle(STD_OUTPUT_HANDLE);
    hSTDin = GetStdHandle(STD_INPUT_HANDLE);
    consoleHwnd = GetConsoleWindow();
    CONSOLE_FONT_INFOEX consoleFontInfo;
    consoleFontInfo.cbSize = sizeof(consoleFontInfo);
    consoleFontInfo.nFont = 0;
    consoleFontInfo.dwFontSize.X = pixelSize.i;
    consoleFontInfo.dwFontSize.Y = pixelSize.j;
    consoleFontInfo.FontFamily = FF_DONTCARE;
    consoleFontInfo.FontWeight = FW_NORMAL;
    wcscpy_s(consoleFontInfo.FaceName, L"Terminal");
    SetCurrentConsoleFontEx(hSTDout, 0, &consoleFontInfo);

    CONSOLE_SCREEN_BUFFER_INFOEX screenBufferInfo;
    screenBufferInfo.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    GetConsoleScreenBufferInfoEx(hSTDout, &screenBufferInfo);
    for (int i = 0; i < 16; i++)
    {
        screenBufferInfo.ColorTable[i] = 
            colourMap.consoleColours[i].r + 
            colourMap.consoleColours[i].g * 256 + 
            colourMap.consoleColours[i].b * 65536;
    }
    SetConsoleScreenBufferInfoEx(hSTDout, &screenBufferInfo);

    COORD largestWindow = GetLargestConsoleWindowSize(hSTDout);
    if (largestWindow.X < screenSize.i)
        this->screenSize.i = largestWindow.X;
    else
        this->screenSize.i = screenSize.i;

    if (largestWindow.Y < screenSize.j)
        this->screenSize.j = largestWindow.Y;
    else
        this->screenSize.j = screenSize.j;

    SetConsoleScreenBufferSize(hSTDout, { (short)this->screenSize.i, (short)this->screenSize.j });
    windowArea = { 0, 0, (short)this->screenSize.i - 1, (short)this->screenSize.j - 1 };
    SetConsoleWindowInfo(hSTDout, TRUE, &windowArea);

    GetConsoleScreenBufferInfoEx(hSTDout, &screenBufferInfo);
    SetConsoleScreenBufferSize(hSTDout, {
        screenBufferInfo.srWindow.Right - screenBufferInfo.srWindow.Left + 1,
        screenBufferInfo.srWindow.Bottom - screenBufferInfo.srWindow.Top + 1 });

    DWORD newStyle = WS_CAPTION | DS_MODALFRAME | WS_MINIMIZEBOX | WS_SYSMENU;
    SetWindowLongW(consoleHwnd, GWL_STYLE, newStyle);
    SetWindowPos(consoleHwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);

    this->thirdDimension = thirdDimension;
    screenBuffer.InitialiseBuffer(screenSize);
    ResetBuffer();

    StartTimer();

    titleManager = new std::thread([this]() 
        {
            while (engineActive) 
            { 
                UpdateTitle();
                Sleep(1000);
            } 
        });

    SetTitle(title);
}
CGE::~CGE()
{
    delete titleManager;
    delete gameTime;
}

void CGE::Startup() { }
void CGE::Shutdown() { }
void CGE::Update() { }
void CGE::Run()
{
    while (engineActive)
    {
        ResetBuffer();

        Update();

        DrawBuffer();

        UpdateTimer();
    }
}

void CGE::StartTimer()
{
    gameTime = new Timer();
    startTime = gameTime->elapsed() * 1000;
}
void CGE::UpdateTimer()
{
    endTime = gameTime->elapsed() * 1000;
    currentDelta = endTime - startTime;
    startTime = endTime;
    frameTimes[currentFrame] = currentDelta;
    currentFrame++;
    currentFrame %= 10;
    deltaTime = 0;
    for (int i = 0; i < 10; i++)
        deltaTime += frameTimes[i];
    deltaTime *= 0.1f;
}

void CGE::SetTitle(LPCWSTR title)
{
    windowTitleLength = 0;
    while (title[windowTitleLength] != '\0' && windowTitleLength < 48)
    {
        windowTitle[windowTitleLength] = title[windowTitleLength];
        windowTitleLength++;
    }
    char fpsText[10] = "    FPS: ";
    for (int i = 0; i < 10; i++)
    {
        windowTitle[i + windowTitleLength] = fpsText[i];
    }
    windowTitleLength += 9;
    SetWindowText(consoleHwnd, windowTitle);
}
void CGE::UpdateTitle()
{
    int fps = 1000 / max(deltaTime, 1);
    char fpsText[5];
    sprintf_s(fpsText, "%d", fps);
    for (int i = 0; i < 5; i++)
    {
        windowTitle[i + windowTitleLength] = fpsText[i];
    }
    SetWindowText(consoleHwnd, windowTitle);
}

void CGE::DrawBuffer()
{
   WriteConsoleOutput(hSTDout, screenBuffer.charBuffer, { (short)screenSize.i, (short)screenSize.j }, { 0, 0 }, &windowArea);
}
void CGE::ResetBuffer()
{
    if (thirdDimension) screenBuffer.ResetBuffer3D(screenSize);
    else screenBuffer.ResetBuffer2D(screenSize);
}
void CGE::SetBuffer(Colour colour)
{
    colour.a = 255;
    if (thirdDimension)
    {
        screenBuffer.SetPixelBuffer(screenSize.i * screenSize.j, colour);
        screenBuffer.ResetEdgeBuffer(screenSize.j);
        screenBuffer.ResetDepthBuffer(screenSize.i * screenSize.j);
    }
    else
    { 
        screenBuffer.SetPixelBuffer(screenSize.i * screenSize.j, colour);
        screenBuffer.ResetEdgeBuffer(screenSize.j);
    }

    CHAR_INFO pixel;
    pixel.Attributes = colourMap.colourCube[colour.r + colour.g * 256 + colour.b * 65536] & 0xFF;
    switch (colourMap.colourCube[colour.r + colour.g * 256 + colour.b * 65536] >> 8)
    {
    case 0:
        pixel.Char.UnicodeChar = L'\x2588';
        break;
    case 1:
        pixel.Char.UnicodeChar = L'\x2593';
        break;
    case 2:
        pixel.Char.UnicodeChar = L'\x2592';
        break;
    case 3:
        pixel.Char.UnicodeChar = L'\x2591';
        break;
    }

    screenBuffer.SetCharBuffer(screenSize.i * screenSize.j, pixel);
}

void CGE::SetPixel(const tVector2<int>& position, const Colour& colour)
{
    if (colour.a == 0)
        return;

    if (position.i < 0 || position.i >= screenSize.i)
        return;
    if (position.j < 0 || position.j >= screenSize.j)
        return;

    Colour newColour;
    if (colour.a == 255)
        newColour = colour;
    else
        newColour = colour + screenBuffer.pixelBuffer[screenSize.i * position.j + position.i];

    screenBuffer.pixelBuffer[screenSize.i * position.j + position.i] = newColour;
    CHAR_INFO& pixel = screenBuffer.charBuffer[screenSize.i * (screenSize.j - position.j - 1) + position.i];
    pixel.Attributes = colourMap.colourCube[newColour.r + newColour.g * 256 + newColour.b * 65536] & 0xFF;
    switch (colourMap.colourCube[newColour.r + newColour.g * 256 + newColour.b * 65536] >> 8)
    {
    case 0:
        pixel.Char.UnicodeChar = L'\x2588';
        break;
    case 1:
        pixel.Char.UnicodeChar = L'\x2593';
        break;
    case 2:
        pixel.Char.UnicodeChar = L'\x2592';
        break;
    case 3:
        pixel.Char.UnicodeChar = L'\x2591';
        break;
    }
}
void CGE::SetPixel(const Point2D& point)
{
    if (point.colour.a == 0)
        return;

    Colour newColour;
    if (point.colour.a == 255)
        newColour = point.colour;
    else
        newColour = point.colour + screenBuffer.pixelBuffer[(int)(screenSize.i * point.position.j + point.position.i)];

    screenBuffer.pixelBuffer[(int)(screenSize.i * point.position.j + point.position.i)] = newColour;
    CHAR_INFO& pixel = screenBuffer.charBuffer[(int)(screenSize.i * (screenSize.j - point.position.j - 1) + point.position.i)];
    pixel.Attributes = colourMap.colourCube[newColour.r + newColour.g * 256 + newColour.b * 65536] & 0xFF;
    switch (colourMap.colourCube[newColour.r + newColour.g * 256 + newColour.b * 65536] >> 8)
    {
    case 0:
        pixel.Char.UnicodeChar = L'\x2588';
        break;
    case 1:
        pixel.Char.UnicodeChar = L'\x2593';
        break;
    case 2:
        pixel.Char.UnicodeChar = L'\x2592';
        break;
    case 3:
        pixel.Char.UnicodeChar = L'\x2591';
        break;
    }
}

void CGE::DrawLine(tVector2<int> position1, tVector2<int> position2, const Colour& colour)
{
    short dx = abs(position2.i - position1.i);
    short sx = position1.i < position2.i ? 1 : -1;
    short dy = -abs(position2.j - position1.j);
    short sy = position1.j < position2.j ? 1 : -1;
    short err = dx + dy;
    while (true)
    {
        SetPixel(position1, colour);
        if ((int)position1.i == (int)position2.i && (int)position1.j == (int)position2.j) break;
        short e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            position1.i += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            position1.j += sy;
        }
    }
}
void CGE::DrawLine(Line line, const Colour& colour)
{
    line.point[0] = (tVector2<int>)line.point[0];
    line.point[1] = (tVector2<int>)line.point[1];
    short dx = abs(line.point[1].i - line.point[0].i);
    short sx = line.point[0].i < line.point[1].i ? 1 : -1;
    short dy = -abs(line.point[1].j - line.point[0].j);
    short sy = line.point[0].j < line.point[1].j ? 1 : -1;
    short err = dx + dy;
    while (true)
    {
        SetPixel(line.point[0], colour);
        if ((int)line.point[0].i == (int)line.point[1].i && (int)line.point[0].j == (int)line.point[1].j) break;
        short e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            line.point[0].i += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            line.point[0].j += sy;
        }
    }
} 
void CGE::DrawLineEx(Vector2 position1, Vector2 position2, const Colour& colour, int thickness)
{
    if (thickness < 1)
        return;

    if (thickness == 1)
    {
        DrawLine(position1, position2, colour);
        return;
    }

    tVector2<int> p[2];
    p[0] = position1;
    p[1] = position2;

    if (p[0].i == p[1].i)
    {
        int B, T, L, R, H;
        H = thickness * 0.5f;
        if (p[0].j < p[1].j)
        {
            B = p[0].j; T = p[1].j;
        }
        else
        {
        B = p[1].j; T = p[0].j;
        }
        L = p[0].i - H;
        R = L + thickness;

        for (int h = B; h <= T; h++)
        {
            for (int w = L; w < R; w++)
                SetPixel({ w, h }, colour);
        }

        return;
    }

    if (p[0].j == p[1].j)
    {
        int B, T, L, R, H;
        H = thickness * 0.5f;
        if (p[0].i < p[1].i)
        {
            L = p[0].i; R = p[1].i;
        }
        else
        {
            L = p[1].i; R = p[0].i;
        }
        B = p[0].j - H;
        T = B + thickness;

        for (int h = B; h < T; h++)
        {
            for (int w = L; w <= R; w++)
                SetPixel({ w, h }, colour);
        }

        return;
    }

    Vector2 P = (position2 - position1).Normalise() * (0.5f * thickness);
    Vector2 tP = { -P.j, P.i };
    Vector2 TL = position1 + tP;
    Vector2 TR = position2 + tP;
    Vector2 BL = position1 - tP;
    Vector2 BR = position2 - tP;

    DrawTriangle(TL, TR, BR, colour);
    DrawTriangle(TL, BR, BL, colour);
}
void CGE::DrawLineEx(Line line, const Colour& colour, int thickness)
{
    DrawLineEx(line.point[0], line.point[1], colour, thickness);
}

void CGE::DrawEdge(Edge2D edge)
{

}
void CGE::DrawEdgeEx(Edge2D edge, int thickness)
{

}

void CGE::DrawCircle(const Vector2& position, float radius, const Colour& colour)
{
    int x = 0, y = radius;
    radius = radius * radius + 1;

    SetPixel({ (int)(position.i), (int)(position.j + y) }, colour);
    SetPixel({ (int)(position.i + y), (int)(position.j) }, colour);
    SetPixel({ (int)(position.i), (int)(position.j - y) }, colour);
    SetPixel({ (int)(position.i - y), (int)(position.j) }, colour);

    while (true)
    {
        x++; if (x * x + y * y > radius) y--;

        SetPixel({ (int)(position.i + x), (int)(position.j + y) }, colour);
        SetPixel({ (int)(position.i - x), (int)(position.j - y) }, colour);
        SetPixel({ (int)(position.i - x), (int)(position.j + y) }, colour);
        SetPixel({ (int)(position.i + x), (int)(position.j - y) }, colour);

        if (x > y) break;

        SetPixel({ (int)(position.i + y), (int)(position.j + x) }, colour);
        SetPixel({ (int)(position.i - y), (int)(position.j - x) }, colour);
        SetPixel({ (int)(position.i - y), (int)(position.j + x) }, colour);
        SetPixel({ (int)(position.i + y), (int)(position.j - x) }, colour);
    }
}
void CGE::DrawCircle(const Circle& circle, const Colour& colour)
{
    DrawCircle(circle.position, circle.radius, colour);
}
void CGE::DrawCircleLine(const Vector2& position, float radius, const Colour& colour, int thickness)
{
    //Early out code
    //Needs work

    int D, U;
    int L, R;

    D = 0;
    U = radius - 0.5f * thickness;

    for (D; D < U; D++)
    {
        L = sqrtf((radius - 0.5f * thickness) * (radius - 0.5f * thickness) - D * D);
        R = sqrtf((radius + 0.5f * thickness) * (radius + 0.5f * thickness) - D * D);

        for (L; L <= R; L++)
        {
            SetPixel({ (int)(position.i + L), (int)(position.j + D) }, colour);
            SetPixel({ (int)(position.i + L), (int)(position.j - D) }, colour);
            SetPixel({ (int)(position.i - L), (int)(position.j + D) }, colour);
            SetPixel({ (int)(position.i - L), (int)(position.j - D) }, colour);
        }
    }

    U = radius + 0.5f * thickness;

    for (D; D <= U; D++)
    {
        L = 0;
        R = sqrtf((radius + 0.5f * thickness) * (radius + 0.5f * thickness) - D * D);

        for (L; L <= R; L++)
        {
            SetPixel({ (int)(position.i + L), (int)(position.j + D) }, colour);
            SetPixel({ (int)(position.i + L), (int)(position.j - D) }, colour);
            SetPixel({ (int)(position.i - L), (int)(position.j + D) }, colour);
            SetPixel({ (int)(position.i - L), (int)(position.j - D) }, colour);
        }
    }

    
}
void CGE::DrawCircleLine(const Circle& circle, const Colour& colour, int thickness)
{
    DrawCircleLine(circle.position, circle.radius, colour, thickness);
}

void CGE::DrawOval(const Vector2& position, const Vector2& size, float rotation, const Colour& colour)
{
    
}
void CGE::DrawOval(const Oval& oval, float rotation, const Colour& colour)
{

}
void CGE::DrawOvalLine(const Vector2& position, const Vector2& size, float rotation, const Colour& colour, int thickness)
{
    if (!rotation)
    {
        if (thickness == 1)
        {

        }
        else
        {

        }
    }
    else
    {
        if (thickness == 1)
        {
            float s0 = sinf(-rotation);
            float c0 = cosf(rotation);
            float s2 = s0 * s0;
            float c2 = 1.0f - s2;
            float a = size.i;
            float b = size.j;
            float a2 = size.i * size.i;
            float b2 = size.j * size.j;
            float at = 1.0f / a2;
            float bt = 1.0f / b2;
            float u = c2 * at + s2 * bt;
            float v = s2 * at + c2 * bt;
            float w = 2.0f * c0 * s0 * (at - bt);
            float s = -(2.0f * u + w) / (2.0f * v + w);
            float t = (2.0f * u - w) / (2.0f * v - w);

            Vector2 L;
            L.i = -sqrtf(v) * a * b;
            L.j = -w * L.i * 0.5f / v;

            Vector2 R = L * -1;

            Vector2 U;
            U.j = sqrtf(u) * a * b;
            U.i = -w * L.j * 0.5f / u + 1;

            Vector2 incline;
            incline.i = -1.0f / sqrtf(u + v * s * s + w * s);
            incline.j = s * incline.i;

            incline.i = incline.i;
            incline.j = incline.j;

            Vector2 decline;
            decline.i = 1.0f / sqrtf(u + v * t * t + w * t);
            decline.j = t * decline.i;

            decline.i = decline.i;
            decline.j = decline.j;

            float x1 = L.i + 1;
            float y1 = L.j;

            float x2 = R.i;
            float y2 = R.j;

            SetPixel({ (int)(x1 + position.i), (int)(y1 + position.j) }, colour);
            SetPixel({ (int)(-x1 + position.i), (int)(-y1 + position.j) }, colour);
            SetPixel({ (int)(x2 + position.i), (int)(y2 + position.j) }, colour);
            SetPixel({ (int)(-x2 + position.i), (int)(-y2 + position.j) }, colour);
            SetPixel({ (int)(U.i + position.i), (int)(U.j + 1 + position.j) }, colour);
            SetPixel({ (int)(-U.i + position.i), (int)(-U.j - 1 + position.j) }, colour);

            while (y1 < incline.j)
            {
                SetPixel({ (int)(x1 + position.i), (int)(y1 + position.j) }, colour);
                SetPixel({ (int)(-x1 + position.i), (int)(-y1 + position.j) }, colour);
                y1++;
                if (u * x1 * x1 + v * y1 * y1 + w * x1 * y1 > 1) x1++;
            }
            while (y2 < decline.j)
            {
                SetPixel({ (int)(x2 + position.i), (int)(y2 + position.j) }, colour);
                SetPixel({ (int)(-x2 + position.i), (int)(-y2 + position.j) }, colour);
                y2++;
                if (u * x2 * x2 + v * y2 * y2 + w * x2 * y2 > 1) x2--;
            }
            while (x1 < U.i)
            {
                SetPixel({ (int)(x1 + position.i), (int)(y1 + position.j) }, colour);
                SetPixel({ (int)(-x1 + position.i), (int)(-y1 + position.j) }, colour);
                x1++;
                if (u * (x1) * (x1)+v * (y1 - 1) * (y1 - 1) + w * (x1) * (y1 - 1) < 1) y1++;
            }
            while (x2 > U.i)
            {
                SetPixel({ (int)(x2 + position.i), (int)(y2 + position.j) }, colour);
                SetPixel({ (int)(-x2 + position.i), (int)(-y2 + position.j) }, colour);
                x2--;
                if (u * x2 * x2 + v * (y2 - 1) * (y2 - 1) + w * x2 * (y2 - 1) < 1) y2++;
            }
        }
        else
        {
            float c = cosf(rotation);
            float s = sinf(-rotation);
            float s2 = s * s;
            float c2 = 1.0f - s2;
            float a1 = size.i - 0.5f * thickness;
            float a2 = size.i + 0.5f * thickness;
            float b1 = size.j - 0.5f * thickness;
            float b2 = size.j + 0.5f * thickness;
            float a12 = a1 * a1;
            float a22 = a2 * a2;
            float b12 = b1 * b1;
            float b22 = b2 * b2;
            float a1t = 1.0f / a12;
            float a2t = 1.0f / a22;
            float b1t = 1.0f / b12;
            float b2t = 1.0f / b22;
            float u1 = c2 * a1t + s2 * b1t;
            float u2 = c2 * a2t + s2 * b2t;
            float v1 = s2 * a1t + c2 * b1t;
            float v2 = s2 * a2t + c2 * b2t;
            float w1 = 2.0f * c * s * (a1t - b1t);
            float w2 = 2.0f * c * s * (a2t - b2t);

            float U1 = a1 * b1 * sqrtf(u1);
            float U2 = a2 * b2 * sqrtf(u2);

            float k1 = -w1 / u1 * 0.5f;
            float k2 = -w2 / u2 * 0.5f;

            float l1 = 1.0f / (a1 * b1 * u1);
            float l2 = 1.0f / (a2 * b2 * u2);

            float m1 = b12 * c2 + a12 * s2;
            float m2 = b22 * c2 + a22 * s2;

            float L, R;

            //SetPixel({ (int)position.i, (int)(U1 + position.j) }, RED);
            //SetPixel({ (int)position.i, (int)(U2 + position.j) }, RED);

            for (float h = U1; h < U2; h++)
            {
                L = k1 * h - a2 * b2 * sqrtf(m2 - h * h) / m2;
                R = k1 * h + a2 * b2 * sqrtf(m2 - h * h) / m2;
                for (L; L < (int)R + 1; L++)
                {
                    SetPixel({ (int)(L + position.i), (int)(h + position.j) }, colour);
                }
            }

            
        }
    }
}
void CGE::DrawOvalLine(const Oval& oval, float rotation, const Colour& colour, int thickness)
{

}

void CGE::DrawRect(const Vector2& position, const Vector2& size, float rotation, const Colour& colour)
{
    if (!rotation)
    {
        Vector2 halfSize = size * 0.5f;
        int minX = position.i - halfSize.i, maxX = position.i + halfSize.i;
        int minY = position.j - halfSize.j, maxY = position.j + halfSize.j;
        (minX < 0) ? minX = 0 : minX; (maxX > screenSize.i) ? maxX = screenSize.i : maxX;
        (minY < 0) ? minY = 0 : minY; (maxY > screenSize.j) ? maxY = screenSize.j : maxY;

        for (int h = minY; h < maxY; h++)
            for (int w = minX; w < maxX; w++) SetPixel({ w, h }, colour);
    }
    else
    {
        Vector2 half = size * 0.5f;
        Vector2 TL(-half.i,  half.j);
        Vector2 TR( half.i,  half.j);
        Vector2 BL(-half.i, -half.j);
        Vector2 BR( half.i, -half.j);

        Matrix2 rotMat = Matrix2::CreateRotation(rotation);
        TL = rotMat * TL + position;
        TR = rotMat * TR + position;
        BL = rotMat * BL + position;
        BR = rotMat * BR + position;
        
        DrawTriangle(TL, TR, BR, colour);
        DrawTriangle(TL, BR, BL, colour);
    }
}
void CGE::DrawRect(const Rect& rect, float rotation, const Colour& colour)
{
    if (!rotation)
    {
        Vector2 halfSize = rect.size * 0.5f;
        int minX = rect.position.i - halfSize.i, maxX = rect.position.i + halfSize.i;
        int minY = rect.position.j - halfSize.j, maxY = rect.position.j + halfSize.j;
        (minX < 0) ? minX = 0 : minX; (maxX > screenSize.i) ? maxX = screenSize.i : maxX;
        (minY < 0) ? minY = 0 : minY; (maxY > screenSize.j) ? maxY = screenSize.j : maxY;

        for (int h = minY; h < maxY; h++)
            for (int w = minX; w < maxX; w++) SetPixel({ w, h }, colour);
    }
    else
    {
        Vector2 half = rect.size * 0.5f;
        tVector2<int> TL(-half.i, half.j);
        tVector2<int> TR(half.i, half.j);
        tVector2<int> BL(-half.i, -half.j);
        tVector2<int> BR(half.i, -half.j);

        Matrix2 rotMat = Matrix2::CreateRotation(rotation);
        TL = rotMat * TL + rect.position;
        TR = rotMat * TR + rect.position;
        BL = rotMat * BL + rect.position;
        BR = rotMat * BR + rect.position;

        DrawTriangle(TL, TR, BR, colour);
        DrawTriangle(TL, BR, BL, colour);
    }
}
void CGE::DrawRectLine(const Vector2& position, const Vector2& size, float rotation, const Colour& colour, int thickness)
{
    //Thickness limit early out

    if (rotation == 0)
    {
        if (thickness == 1)
        {
            int D, U, L, R;
            Vector2 H = size * 0.5f;

            D = position.j - H.j;
            U = position.j + H.j;
            L = position.i - H.i;
            R = position.i + H.i;

            for (int h = D + 1; h < U; h++)
            {
                SetPixel({ L, h }, colour);
                SetPixel({ R, h }, colour);
            }
            for (int w = L; w <= R; w++)
            {
                SetPixel({ w, U }, colour);
                SetPixel({ w, D }, colour);
            }
        }
        else
        {
            int D = position.j - (size.j + thickness) * 0.5f;
            int U = D + thickness - 1;
            int L = position.i - (size.i + thickness) * 0.5f;
            int R = position.i + (size.i + thickness) * 0.5f;
            for (int h = D; h <= U; h++)
            {
                for (int w = L; w <= R; w++)
                {
                    SetPixel({ w, h }, colour);
                    SetPixel({ w, h + (int)size.j }, colour);
                }
            }

            D = U + 1;
            U = D + size.j - thickness - 1;
            L = position.i - (size.i + thickness) * 0.5f;
            R = L + thickness - 1;
            for (int h = D; h <= U; h++)
            {
                for (int w = L; w <= R; w++)
                {
                    SetPixel({ w, h }, colour);
                    SetPixel({ w + (int)size.i + 1, h }, colour);
                }
            }
        }
    }
    else if (false) // ((int)(sin(4.0f * rotation) * 100) == 0)
    {
        if (thickness == 1)
        {
            int D, U, L, R;
            Vector2 H;

            if ((int)(rotation * 100) == 157 || (int)(rotation * 100) == 235)
            {
                H.i = size.j * 0.5f;
                H.j = size.i * 0.5f;
            }
            else
            {
                H = size * 0.5f;
            }

            D = position.j - H.j;
            U = position.j + H.j;
            L = position.i - H.i;
            R = position.i + H.i;

            for (int h = D + 1; h < U; h++)
            {
                SetPixel({ L, h }, colour);
                SetPixel({ R, h }, colour);
            }
            for (int w = L; w <= R; w++)
            {
                SetPixel({ w, U }, colour);
                SetPixel({ w, D }, colour);
            }
        }
        else
        {
            Vector2 H;

            if ((int)(rotation * 100) == 156 || (int)(rotation * 100) == 471)
            {
                H.i = size.j;
                H.j = size.i;
            }
            else
            {
                H = size;
            }

            int D = position.j - (H.j + thickness) * 0.5f;
            int U = D + thickness - 1;
            int L = position.i - (H.i + thickness) * 0.5f;
            int R = position.i + (H.i + thickness) * 0.5f;

            for (int h = D; h <= U; h++)
            {
                for (int w = L; w <= R; w++)
                {
                    SetPixel({ w, h }, colour);
                    SetPixel({ w, h + (int)H.j }, colour);
                }
            }

            D = U + 1;
            U = D + H.j - thickness - 1;
            L = position.i - (H.i + thickness) * 0.5f;
            R = L + thickness - 1;
            for (int h = D; h <= U; h++)
            {
                for (int w = L; w <= R; w++)
                {
                    SetPixel({ w, h }, colour);
                    SetPixel({ w + (int)H.i + 1, h }, colour);
                }
            }
        }
    }
    else
    {
        if (thickness == 1)
        {
            Vector2 halfSize = size * 0.5f;
            Vector2 TL = { -halfSize.i,  halfSize.j };
            Vector2 TR = { halfSize.i,  halfSize.j };
            Vector2 BL = { -halfSize.i, -halfSize.j };
            Vector2 BR = { halfSize.i, -halfSize.j };
            Matrix2 rotMat = Matrix2::CreateRotation(rotation);
            
            TL = rotMat * TL + position;
            TR = rotMat * TR + position;
            BL = rotMat * BL + position;
            BR = rotMat * BR + position;
            Vector2 offset = (TR - BR).Normalise();

            DrawLine(TL, TR, colour);
            DrawLine(TR - offset, BR + offset, colour);
            DrawLine(BR, BL, colour);
            DrawLine(BL + offset, TL - offset, colour);
        }
        else
        {
            Vector2 halfS = size * 0.5f;
            float   halfT = 0.5f * thickness;
            Vector2 inner[4];
            inner[0] = { -halfS.i + halfT,  halfS.j - halfT };
            inner[1] = {  halfS.i - halfT,  halfS.j - halfT };
            inner[2] = {  halfS.i - halfT, -halfS.j + halfT };
            inner[3] = { -halfS.i + halfT, -halfS.j + halfT };
            Vector2 outer[4];
            outer[0] = { -halfS.i - halfT,  halfS.j + halfT };
            outer[1] = {  halfS.i + halfT,  halfS.j + halfT };
            outer[2] = {  halfS.i + halfT, -halfS.j - halfT };
            outer[3] = { -halfS.i - halfT, -halfS.j - halfT };
            Matrix2 rotMat = Matrix2::CreateRotation(rotation);;
            for (int i = 0; i < 4; i++)
            {
                inner[i] = rotMat * inner[i] + position;
                outer[i] = rotMat * outer[i] + position;
            }

            DrawTriangle(outer[0], outer[1], inner[1], colour);
            DrawTriangle(outer[0], inner[0], inner[1], colour);
            DrawTriangle(outer[1], outer[2], inner[2], colour);
            DrawTriangle(outer[1], inner[1], inner[2], colour);
            DrawTriangle(outer[2], outer[3], inner[3], colour);
            DrawTriangle(outer[2], inner[2], inner[3], colour);
            DrawTriangle(outer[3], outer[0], inner[0], colour);
            DrawTriangle(outer[3], inner[3], inner[0], colour);
        }
    }
}
void CGE::DrawRectLine(const Rect& rect, float rotation, const Colour& colour, int thickness)
{

}

void CGE::DrawRectangle(const Rectangle2D& rectangle, float rotation)
{

}
void CGE::DrawRectangleLine(const Triangle& triangle, float rotation, int thickness, const Colour& colour)
{

}
void CGE::DrawRectangleTexture(const Triangle& source, const Triangle& dest, const Texture& texture, float sourceRot, float destRot)
{

}
void CGE::DrawRectangleTexture(const vTriangle2D& triangle, const Texture& texture, float rotation)
{

}

void CGE::DrawTriangle(const Vector2& p0, const Vector2& p1, const Vector2& p2, const Colour& colour)
{
    if (colour.r == 0 && colour.g == 0 && colour.b == 0 && colour.a == 0)
        return;

    int A, B, M;
    tVector2<int> p[3];

    p[0] = (Vector2)((tVector2<int>)p0);
    p[1] = (Vector2)((tVector2<int>)p1);
    p[2] = (Vector2)((tVector2<int>)p2);

    bool hAlligned = false;

    if (p[0].j == p[1].j)
    {
        hAlligned = true;
        A = 0; B = 1; M = 2;
    }
    if (p[1].j == p[2].j)
    {
        if (hAlligned)
            return;

        hAlligned = true;
        A = 1; B = 2; M = 0;
    }
    if (p[2].j == p[0].j)
    {
        if (hAlligned)
            return;

        hAlligned = true;
        A = 2; B = 0; M = 1;
    }
    if (hAlligned)
    {
        if (p[A].i == p[B].i)
            return;

        if (p[A].i > p[B].i)
        {
            A = B;
            B = 3 - A - M;
        }

        float mA = (float)(p[A].i - p[M].i) / (p[A].j - p[M].j);
        float mB = (float)(p[B].i - p[M].i) / (p[B].j - p[M].j);

        float L, R;
        int D, U;

        if (p[M].j < p[A].j)
        {
            D = p[M].j + 0.5f; U = p[A].j + 0.5f;
        }
        else
        {
            D = p[A].j + 0.5f; U = p[M].j + 0.5f;
        }

        L = p[M].i - mA * p[M].j;
        R = p[M].i - mB * p[M].j;

        for (D; D < U; D++)
        {
            for (int w = mA * D + L; w < (int)(mB * D + R); w++)
                SetPixel({ w, D }, colour);
        }

        return;
    }

    A = 0; B = 0; M = 0;

    for (int i = 0; i < 3; i++)
    {
        if (p[i].j > p[A].j)
            A = i;
        if (p[i].j < p[B].j)
            B = i;
    }

    M = 3 - A - B;

    float mA = (float)(p[A].i - p[M].i) / (p[A].j - p[M].j);
    float mB = (float)(p[B].i - p[M].i) / (p[B].j - p[M].j);
    float mAB = (float)(p[A].i - p[B].i) / (p[A].j - p[B].j);

    tVector2<float> midAB(mAB * (p[M].j - p[A].j) + p[A].i, p[M].j);
    float L, R;
    int D, U;

    if (midAB.i < p[M].i)
    {
        D = p[B].j + 0.5f; U = p[M].j + 0.5f;
        L = p[B].i - p[B].j * mAB;
        R = p[M].i - p[M].j * mB;

        for (D; D < U; D++)
        {
            for (int w = D * mAB + L; w < (int)(D * mB + R); w++)
                SetPixel({ w, D }, colour);
        }

        D = p[M].j + 0.5f; U = p[A].j + 0.5f;
        R = p[M].i - p[M].j * mA;

        for (D; D < U; D++)
        {
            for (int w = D * mAB + L; w < (int)(D * mA + R); w++)
                SetPixel({ w, D }, colour);
        }
    }
    else if (midAB.i > p[M].i)
    {
        D = p[B].j + 0.5f; U = p[M].j + 0.5f;
        L = p[M].i - p[M].j * mB;
        R = p[B].i - p[B].j * mAB;

        for (D; D < U; D++)
        {
            for (int w = D * mB + L; w < (int)(D * mAB + R); w++)
                SetPixel({ w, D }, colour);
        }

        D = p[M].j + 0.5f; U = p[A].j + 0.5f;
        L = p[M].i - p[M].j * mA;

        for (D; D < U; D++)
        {
            for (int w = D * mA + L; w < (int)(D * mAB + R); w++)
                SetPixel({ w, D }, colour);
        }
    }
}
void CGE::DrawTriangle(const Triangle& triangle, float rotation, const Colour& colour)
{
    if (colour.r == 0 && colour.g == 0 && colour.b == 0 && colour.a == 0)
        return;

    int A, B, M;
    tVector2<int> p[3];

    if (!rotation)
    {
        p[0] = triangle.point[0] + triangle.position;
        p[1] = triangle.point[1] + triangle.position;
        p[2] = triangle.point[2] + triangle.position;
    }
    else
    {
        Matrix2 rotMat = Matrix2::CreateRotation(rotation);
        p[0] = rotMat * triangle.point[0] + triangle.position;
        p[1] = rotMat * triangle.point[1] + triangle.position;
        p[2] = rotMat * triangle.point[2] + triangle.position;
    }

    bool vAlligned = false;
    bool hAlligned = false;

    if (p[0].i == p[1].i)
    {
        vAlligned = true;
        A = 0; B = 1; M = 2;
    }
    if (p[1].i == p[2].i)
    {
        if (vAlligned)
            return;

        vAlligned = true;
        A = 1; B = 2; M = 0;
    }
    if (p[2].i == p[0].i)
    {
        if (vAlligned)
            return;

        vAlligned = true;
        A = 2; B = 0; M = 1;
    }
    if (vAlligned)
    {
        if (p[A].j == p[B].j)
            return;

        if (p[A].j < p[B].j)
        {
            A = B;
            B = 3 - A - M;
        }

        float mA = (float)(p[A].j - p[M].j) / (p[A].i - p[M].i);
        float mB = (float)(p[B].j - p[M].j) / (p[B].i - p[M].i);
        int L, R, U, D;

        if (p[M].i < p[A].i)
        {
            L = p[M].i; R = p[A].i;
        }
        else
        {
            L = p[A].i; R = p[M].i;
        }

        U = p[M].j - mA * p[M].i;
        D = p[M].j - mB * p[M].i;

        for (int w = L; w <= R; w++)
        {
            for (int h = mB * w + D; h <= mA * w + U; h++)
                SetPixel({ w, h }, colour);
        }

        return;
    }

    if (p[0].j == p[1].j)
    {
        hAlligned = true;
        A = 0; B = 1; M = 2;
    }
    if (p[1].j == p[2].j)
    {
        if (hAlligned)
            return;

        hAlligned = true;
        A = 1; B = 2; M = 0;
    }
    if (p[2].j == p[0].j)
    {
        if (hAlligned)
            return;

        hAlligned = true;
        A = 2; B = 0; M = 1;
    }
    if (hAlligned)
    {
        if (p[A].i == p[B].i)
            return;

        if (p[A].i > p[B].i)
        {
            A = B;
            B = 3 - A - M;
        }

        float mA = (float)(p[A].i - p[M].i) / (p[A].j - p[M].j);
        float mB = (float)(p[B].i - p[M].i) / (p[B].j - p[M].j);
        int L, R, U, D;

        if (p[M].j < p[A].j)
        {
            D = p[M].j; U = p[A].j;
        }
        else
        {
            D = p[A].j; U = p[M].j;
        }

        L = p[M].i - mA * p[M].j;
        R = p[M].i - mB * p[M].j;

        for (int h = D; h <= U; h++)
        {
            for (int w = mA * h + L; w <= mB * h + R; w++)
                SetPixel({ w, h }, colour);
        }

        return;
    }

    A = 0; B = 0; M = 0;

    for (int i = 0; i < 3; i++)
    {
        if (p[i].j > p[A].j)
            A = i;
        if (p[i].j < p[B].j)
            B = i;
    }

    M = 3 - A - B;
    
    float mA =  (float)(p[A].i - p[M].i) / (p[A].j - p[M].j);
    float mB =  (float)(p[B].i - p[M].i) / (p[B].j - p[M].j);
    float mAB = (float)(p[A].i - p[B].i) / (p[A].j - p[B].j);

    tVector2<int> midAB(mAB * (p[M].j - p[A].j) + p[A].i, p[M].j);
    int L, R, U, D;

    if (midAB.i < p[M].i)
    {
        D = p[B].j; U = p[M].j;
        L = p[B].i - p[B].j * mAB;
        R = p[M].i - p[M].j * mB;
        
        for (int h = D; h < U; h++)
        {
            for (int w = h * mAB + L; w <= h * mB + R; w++)
                SetPixel({ w, h }, colour);
        }
        
        D = p[M].j; U = p[A].j;
        R = p[M].i - p[M].j * mA;

        for (int h = D; h <= U; h++)
        {
            for (int w = h * mAB + L; w <= h * mA + R; w++)
                SetPixel({ w, h }, colour);
        }
    }
    else if (midAB.i > p[M].i)
    {
        D = p[B].j; U = p[M].j;
        L = p[M].i - p[M].j * mB; 
        R = p[B].i - p[B].j * mAB;

        for (int h = D; h < U; h++)
        {
            for (int w = h * mB + L; w <= h * mAB + R; w++)
                SetPixel({ w, h }, colour);
        }

        D = p[M].j; U = p[A].j;
        L = p[M].i - p[M].j * mA;

        for (int h = D; h <= U; h++)
        {
            for (int w = h * mA + L; w <= h * mAB + R; w++)
                SetPixel({ w, h }, colour);
        }
    }
}
void CGE::DrawTriangle(const Triangle2D& triangle, float rotation)
{

}
void CGE::DrawTriangleLine(const Triangle& triangle, float rotation, const Colour& colour, int thickness)
{

}
void CGE::DrawTriangleTexture(const Triangle& source, const Triangle& dest, const Texture& texture, float sourceRot, float destRot)
{

}
void CGE::DrawTriangleTexture(const vTriangle2D& triangle, const Texture& texture, float rotation)
{

}
