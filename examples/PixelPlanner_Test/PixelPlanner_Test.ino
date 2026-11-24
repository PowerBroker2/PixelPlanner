#include <Arduino.h>
#include "TeensyTFT.h"
#include "PixelPlanner.h"
#include "test_png.h"

// TFT display
TeensyTFT display(10, 9, 255);

// --- Test pixmaps ---
Pixmap<60, 60> testmap;
Pixmap<60, 60> testmap2;

// --- New feature pixmaps ---
Pixmap<60, 60> arcmap;       ///< draw_arc examples
Pixmap<60, 60> polylinemap;  ///< draw_polyline example
Pixmap<60, 60> polygonmap;   ///< draw_polygon examples
Pixmap<60, 60> floodfillmap; ///< flood_fill example
Pixmap<60, 60> blitmap;      ///< blit_from_pixmap example
Pixmap<60, 60> pngmap;      ///< 
Pixmap<60, 60> compassmap;
Pixmap<80, 40> compassAtlas;
constexpr PixmapRect DialSprite{0, 0, 40, 40};
constexpr PixmapRect NeedleSprite{40, 0, 40, 40};
uint32_t compassFrameTime = 0;
float compassAngle = -90.0f;

constexpr int TileSize = 60;
constexpr int ZoomSize = 2 * TileSize;
using ZoomPixmap = Pixmap<ZoomSize, ZoomSize>;
ZoomPixmap zoomedTile;

struct ArcDrawing
{
    template <typename Canvas>
    void operator()(Canvas& tile) const
    {
        tile.draw_arc(30, 30, 28, 0, true, 0, 90, CYAN);
        tile.draw_arc(30, 30, 28, 6, false, 90, 270, ORANGE);
        tile.draw_arc(30, 30, 20, 3, false, 270, 360, MAGENTA, 0.7f);
        tile.draw_circle(30, 30, 3, 0, true, WHITE);
    }

    static void redraw(ZoomPixmap& tile) { tile.redrawResized(TileSize, TileSize, ArcDrawing{}); }
};

struct AlphaDrawing
{
    template <typename Canvas>
    void operator()(Canvas& tile) const
    {
        tile.draw_rect(0, 0, TileSize, TileSize, 0, true, WHITE);
        tile.draw_rect(5, 5, 30, 20, 0, true, RED, 0.6f);
        tile.draw_rect(15, 10, 30, 20, 0, true, BLUE, 0.6f);
        tile.draw_line_from_points(0, 0, 59, 59, 4, true, GREEN, 0.7f);
        tile.draw_line_from_points(0, 59, 59, 0, 4, true, YELLOW, 0.7f);
        tile.draw_circle(30, 30, 15, 0, true, MAGENTA, 0.5f);
    }

    static void redraw(ZoomPixmap& tile) { tile.redrawResized(TileSize, TileSize, AlphaDrawing{}); }
};

template <size_t W, size_t H>
void renderCompass(Pixmap<W, H>& target, float angle)
{
    target.clear();
    target.blitRegion(compassAtlas, DialSprite, {0, 0, W, H},
                      BlitMode::STRETCH, ScaleMode::BILINEAR);
    constexpr float center = 20.5f;
    BlitTransform needle(W / 40.0f * center, H / 40.0f * center);
    needle.angle = angle;
    needle.pivotX = needle.pivotY = center;
    needle.scaleX = W / 40.0f;
    needle.scaleY = H / 40.0f;
    target.blitTransformed(compassAtlas, NeedleSprite, needle, ScaleMode::BILINEAR);
    target.draw_circle(int(needle.x), int(needle.y), W / 30, 0, true, WHITE);
}

void redrawCompass(ZoomPixmap& tile)
{
    renderCompass(tile, compassAngle);
}

struct Tile
{
    int x, y;
    const Pixmap<TileSize, TileSize>* pixmap;
    ScaleMode interpolation;
    void (*redraw)(ZoomPixmap&);
    const char* modeName;
};

const Tile tiles[] = {
    {10, 10, &arcmap, ScaleMode::NEAREST, ArcDrawing::redraw, "Native redraw"},
    {80, 10, &polylinemap, ScaleMode::NEAREST, nullptr, "Nearest neighbor"},
    {150, 10, &polygonmap, ScaleMode::BILINEAR, nullptr, "Bilinear"},
    {220, 10, &blitmap, ScaleMode::AREA, nullptr, "Area"},
    {10, 80, &floodfillmap, ScaleMode::NEAREST, nullptr, "Nearest neighbor"},
    {80, 80, &testmap2, ScaleMode::BILINEAR, nullptr, "Bilinear"},
    {150, 80, &pngmap, ScaleMode::BILINEAR, nullptr, "Bilinear"},
    {150, 150, &testmap, ScaleMode::NEAREST, AlphaDrawing::redraw, "Native redraw"},
    {220, 80, &compassmap, ScaleMode::BILINEAR, redrawCompass, "Atlas + rotating compass"}
};
constexpr int TileCount = sizeof(tiles) / sizeof(tiles[0]);
int pressedTile = -1;
bool wasTouched = false;

template <size_t W, size_t H>
void drawTile(int x, int y, const Pixmap<W, H>& pixmap)
{
    display.fillRegion(x, y, W, H, pixmap.getPixels(), pixmap.getMask());
}

void drawZoomedTile(int tileIndex)
{
    const auto& tile = tiles[tileIndex];
    const int x = constrain(tile.x - TileSize / 2, 0, int(display.width()) - ZoomSize);
    const int y = constrain(tile.y - TileSize / 2, 0, int(display.height()) - ZoomSize);
    if (tile.redraw)
        tile.redraw(zoomedTile);
    else
    {
        zoomedTile.clear();
        zoomedTile.blitResized(*tile.pixmap, 0, 0, ZoomSize, ZoomSize, tile.interpolation);
    }
    drawTile(x, y, zoomedTile);
}

void drawScene()
{
    display.clear();
    for (int i = 0; i < TileCount; ++i)
    {
        const auto& tile = tiles[i];
        if (i != pressedTile)
            drawTile(tile.x, tile.y, *tile.pixmap);
    }

    if (pressedTile >= 0)
    {
        drawZoomedTile(pressedTile);
        Serial.println(tiles[pressedTile].modeName);
    }
    display.swap();
}

int tileAt(int x, int y)
{
    for (int i = 0; i < TileCount; ++i)
    {
        const auto& tile = tiles[i];
        if (x >= tile.x && x < tile.x + TileSize && y >= tile.y && y < tile.y + TileSize)
            return i;
    }
    return -1;
}

void setup()
{
    Serial.begin(115200);
    if (!display.begin())
    {
        for (;;)
        {
            Serial.println("Display initialization failed. Check wiring and RAM2.");
            delay(1000);
        }
    }

    // =========================================================
    // --- Existing examples (unchanged) ---
    // =========================================================

    // Scale the RGB565 image into the upper half of this tile first.
    const uint16_t sampleArray[] = {RED, GREEN, BLUE, YELLOW};
    testmap2.blitFromArray(sampleArray, 2, 2, BlitMode::STRETCH, ScaleMode::BILINEAR,
                           1.0f, 0, 0, TileSize, TileSize / 2);

    // --- Rotated rectangle ---
    testmap2.draw_rect_rotated(5, 5, 20, 10, 30, 15, 10, 2, true,  CYAN);
    testmap2.draw_rect_rotated(5, 5, 20, 10, 30, 15, 10, 2, false, RED);

    // --- Lines ---
    testmap2.draw_line_from_points(0,  0,  59, 59, 2, true,  BLUE);
    testmap2.draw_line_from_points(59, 0,  0,  59, 1, false, GREEN);
    testmap2.draw_line_from_pt_slope(30, 30, INFINITY, 3, true, 0, YELLOW);

    // --- Circles ---
    testmap2.draw_circle(30, 10, 8, 3, true,  YELLOW);
    testmap2.draw_circle(30, 10, 8, 3, false, PURPLE);
    testmap2.draw_circle(50, 50, 5, 0, true,  ORANGE);

    // --- Rectangle ---
    testmap2.draw_rect(10, 40, 20, 15, 3, false, CYAN);

    // --- Filled triangle ---
    testmap2.fill_triangle(5, 55, 25, 45, 45, 55, MAGENTA);

    // Use the same drawing commands for the normal and enlarged tiles.
    testmap.redrawResized(TileSize, TileSize, AlphaDrawing{});
    arcmap.redrawResized(TileSize, TileSize, ArcDrawing{});

    // --- draw_polyline ---
    // Zigzag across the pixmap
    float zigzag[] = {
         0.0f, 30.0f,
        10.0f,  5.0f,
        20.0f, 55.0f,
        30.0f,  5.0f,
        40.0f, 55.0f,
        50.0f,  5.0f,
        59.0f, 30.0f
    };
    Pixmap<60, 60, true> pathLayer;
    pathLayer.setClipRect({3, 8, 54, 44});
    StrokeStyle zigzagStyle;
    zigzagStyle.width = 2.5f;
    zigzagStyle.join = StrokeJoin::MITER;
    zigzagStyle.miterLimit = 2.0f;
    zigzagStyle.antialias = Antialiasing::COVERAGE;
    pathLayer.strokePath(zigzag, 7, false, zigzagStyle, YELLOW);

    // Second polyline — gentle S-curve approximation
    float scurve[] = {
         5.0f, 50.0f,
        20.0f, 40.0f,
        40.0f, 20.0f,
        55.0f, 10.0f
    };
    StrokeStyle curveStyle;
    curveStyle.width = 4.0f;
    curveStyle.join = StrokeJoin::ROUND;
    curveStyle.cap = StrokeCap::ROUND;
    curveStyle.antialias = Antialiasing::COVERAGE;
    pathLayer.strokePath(scurve, 4, false, curveStyle, CYAN, 0.8f);

    polylinemap.draw_rect(2, 7, 56, 46, 0, true, RGB565(22, 30, 43));
    polylinemap.draw_rect(2, 7, 56, 46, 1, false, WHITE);
    polylinemap.blit_from_pixmap(pathLayer);

    // --- draw_polygon ---
    // Outlined pentagon
    float pentagon[] = {
        30.0f,  5.0f,
        55.0f, 22.0f,
        46.0f, 52.0f,
        14.0f, 52.0f,
         5.0f, 22.0f
    };
    polygonmap.draw_polygon(pentagon, 5, 2, false, GREEN);

    // Filled concave arrow head pointing right (tests even-odd fill)
    float arrow[] = {
        10.0f, 20.0f,
        40.0f, 30.0f,
        10.0f, 40.0f,
        20.0f, 30.0f
    };
    polygonmap.draw_polygon(arrow, 4, 0, true, ORANGE, 0.8f, Antialiasing::COVERAGE);

    // --- flood_fill ---
    // Draw a closed shape, then flood fill its interior
    floodfillmap.draw_rect(0,  0,  floodfillmap.width(), floodfillmap.height(), 0, true, BLACK);
    floodfillmap.draw_circle(30, 30, 25, 2, false, WHITE);       // white ring border
    floodfillmap.flood_fill(30, 30, BLUE);                        // fill interior blue
    floodfillmap.draw_circle(30, 30, 10, 2, false, YELLOW);      // inner ring
    floodfillmap.flood_fill(30, 30, CYAN);                        // re-fill inner area cyan
    floodfillmap.draw_circle(30, 30,  3, 0, true,  RED);         // solid centre dot

    // --- blit_from_pixmap ---
    // Draw a small source pixmap then composite it onto blitmap
    Pixmap<30, 30> srcmap;
    srcmap.draw_rect(0, 0, 30, 30, 0, true, PURPLE);
    srcmap.draw_circle(15, 15, 12, 4, false, YELLOW);
    srcmap.draw_line_from_points(0, 0, 29, 29, 2, false, WHITE);

    // White background so blending is visible
    blitmap.draw_rect(0, 0, blitmap.width(), blitmap.height(), 0, true, WHITE);

    // Blit source into top-left at full opacity
    blitmap.blit_from_pixmap(srcmap, BlitMode::TOP_LEFT,    ScaleMode::NEAREST,  1.0f);

    // Blit again into the bottom-right at 60% opacity.
    blitmap.blit_from_pixmap(srcmap, BlitMode::BOTTOM_RIGHT, ScaleMode::BILINEAR, 0.6f);

    pngmap.flood_fill(0, 0, INDIAN_RED);
    pngmap.blitFromArray(test_png_pixels, TEST_PNG_WIDTH, TEST_PNG_HEIGHT, BlitMode::FIT, ScaleMode::BILINEAR);

    // Store the dial and the transparent needle side by side in one sprite atlas.
    compassAtlas.draw_circle(20, 20, 19, 0, true, RGB565(22, 30, 43));
    compassAtlas.draw_circle(20, 20, 19, 1, false, CYAN);
    for (int tick = 0; tick < 12; ++tick)
    {
        const float angle = tick * 30.0f * DEG_TO_RAD;
        const float dx = cosf(angle), dy = sinf(angle);
        compassAtlas.draw_line_from_points(20 + 13 * dx, 20 + 13 * dy,
                                           20 + 16 * dx, 20 + 16 * dy,
                                           1, false, tick == 9 ? YELLOW : WHITE);
    }
    compassAtlas.fill_triangle(60, 17, 75, 20, 60, 23, RED);
    compassAtlas.fill_triangle(60, 17, 50, 20, 60, 23, WHITE);
    renderCompass(compassmap, compassAngle);
    drawScene();
    Serial.println("Press a tile to enlarge it. Send c to calibrate touch.");
}

void loop()
{
    if (!wasTouched && Serial.read() == 'c')
    {
        Serial.println("Hold the stylus on each crosshair, then lift.");
        Serial.println(display.calibrateTouch() ? "Calibration applied." : "Calibration unchanged.");
    }

    const bool touched = display.touched();
    if (touched != wasTouched)
    {
        wasTouched = touched;
        const int nextTile = touched ? tileAt(display.touchX(), display.touchY()) : -1;
        if (nextTile != pressedTile)
        {
            pressedTile = nextTile;
            compassAngle = -90.0f;
            compassFrameTime = millis();
            drawScene();
        }
    }
    if (pressedTile >= 0 && tiles[pressedTile].redraw == redrawCompass)
    {
        const uint32_t now = millis();
        const uint32_t elapsed = now - compassFrameTime;
        if (elapsed >= 33)
        {
            compassFrameTime = now;
            compassAngle = fmodf(compassAngle + elapsed * 0.12f, 360.0f);
            // The opaque dial covers every needle position, so redraw just this tile.
            drawZoomedTile(pressedTile);
            display.swap();
        }
    }
}
