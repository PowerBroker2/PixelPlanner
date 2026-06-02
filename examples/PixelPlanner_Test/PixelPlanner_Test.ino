#include <Arduino.h>
#include "TeensyTFT.h"
#include "PixelPlanner.h"
#include "test_png.h"

// TFT display
TeensyTFT display(10, 9, 255);

// --- Test pixmaps ---
Pixmap<60, 60> testmap;
Pixmap<60, 60> testmap2;
Pixmap<60, 30> testmap3;

// --- New feature pixmaps ---
Pixmap<60, 60> arcmap;       ///< draw_arc examples
Pixmap<60, 60> polylinemap;  ///< draw_polyline example
Pixmap<60, 60> polygonmap;   ///< draw_polygon examples
Pixmap<60, 60> floodfillmap; ///< flood_fill example
Pixmap<60, 60> blitmap;      ///< blit_from_pixmap example
Pixmap<60, 60> pngmap;      ///< 

void setup()
{
    Serial.begin(115200);
    display.begin();

    // =========================================================
    // --- Existing examples (unchanged) ---
    // =========================================================

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

    // --- Blit from array ---
    uint16_t sampleArray[4] = { RED, GREEN, BLUE, YELLOW };
    testmap3.blitFromArray(sampleArray, 2, 2, BlitMode::STRETCH, ScaleMode::BILINEAR);

    // --- Alpha blending ---
    testmap.draw_rect(0,  0,  testmap.width(), testmap.height(), 0, true, WHITE);
    testmap.draw_rect(5,  5,  30, 20, 0, true, RED,     0.6f);
    testmap.draw_rect(15, 10, 30, 20, 0, true, BLUE,    0.6f);
    testmap.draw_line_from_points(0,  0,  59, 59, 4, true, GREEN,   0.7f);
    testmap.draw_line_from_points(0,  59, 59, 0,  4, true, YELLOW,  0.7f);
    testmap.draw_circle(30, 30, 15, 0, true, MAGENTA, 0.5f);

    // =========================================================
    // --- New feature examples ---
    // =========================================================

    // --- draw_arc ---
    // Solid pie slice: 0°–90° (top-right quadrant)
    arcmap.draw_arc(30, 30, 28, 0, true,  0.0f,   90.0f,  CYAN,    1.0f);
    // Ring segment:   90°–270° (left half), 6px thick
    arcmap.draw_arc(30, 30, 28, 6, false, 90.0f,  270.0f, ORANGE,  1.0f);
    // Thin ring segment spanning the bottom quarter, semi-transparent
    arcmap.draw_arc(30, 30, 20, 3, false, 270.0f, 360.0f, MAGENTA, 0.7f);
    // Small solid dot at centre to anchor visually
    arcmap.draw_circle(30, 30, 3, 0, true, WHITE);

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
    polylinemap.draw_polyline(zigzag, 7, 2, YELLOW);

    // Second polyline — gentle S-curve approximation
    float scurve[] = {
         5.0f, 50.0f,
        20.0f, 40.0f,
        40.0f, 20.0f,
        55.0f, 10.0f
    };
    polylinemap.draw_polyline(scurve, 4, 3, CYAN, 0.8f);

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
    polygonmap.draw_polygon(arrow, 4, 0, true, ORANGE, 0.8f);

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

    // Blit again into bottom-right, stretched to fill, at 60% opacity
    blitmap.blit_from_pixmap(srcmap, BlitMode::BOTTOM_RIGHT, ScaleMode::BILINEAR, 0.6f);

    pngmap.flood_fill(0, 0, INDIAN_RED);
    pngmap.blitFromArray(test_png_pixels, TEST_PNG_WIDTH, TEST_PNG_HEIGHT, BlitMode::FIT, ScaleMode::BILINEAR);
}

void loop()
{
    display.clear();

    display.fillRegion(150, 150, testmap.width(),      testmap.height(),      testmap.getPixels(),      testmap.getMask());
    display.fillRegion(80,  80,  testmap3.width(),     testmap3.height(),     testmap3.getPixels(),     testmap3.getMask());
    display.fillRegion(80,  80,  testmap2.width(),     testmap2.height(),     testmap2.getPixels(),     testmap2.getMask());
    display.fillRegion(10,  10,  arcmap.width(),       arcmap.height(),       arcmap.getPixels(),       arcmap.getMask());
    display.fillRegion(80,  10,  polylinemap.width(),  polylinemap.height(),  polylinemap.getPixels(),  polylinemap.getMask());
    display.fillRegion(150, 10,  polygonmap.width(),   polygonmap.height(),   polygonmap.getPixels(),   polygonmap.getMask());
    display.fillRegion(10,  80,  floodfillmap.width(), floodfillmap.height(), floodfillmap.getPixels(), floodfillmap.getMask());
    display.fillRegion(220, 10,  blitmap.width(),      blitmap.height(),      blitmap.getPixels(),      blitmap.getMask());
    display.fillRegion(150, 80,  pngmap.width(),       pngmap.height(),       pngmap.getPixels(),       pngmap.getMask());

    display.swap();
}