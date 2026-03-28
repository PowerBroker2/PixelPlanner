#include <Arduino.h>
#include "TeensyTFT.h"
#include "PixelPlanner.h"

// TFT display
TeensyTFT display(10, 9, 255);

// --- Test pixmap ---
Pixmap<60,60> testmap;
Pixmap<60,60> testmap2;
Pixmap<60,30> testmap3;

void setup()
{
    Serial.begin(115200);
    display.begin(80000000,1);

    // --- Clear Pixmap to white ---
    // testmap2.draw_rect(0,0,testmap2.width(),testmap2.height(),0,true,ILI9341_WHITE);

    // --- Rotated rectangle ---
    testmap2.draw_rect_rotated(5,5,20,10,30,15,10,2,true,ILI9341_CYAN);
    testmap2.draw_rect_rotated(5,5,20,10,30,15,10,2,false,ILI9341_RED);

    // --- Lines ---
    testmap2.draw_line_from_points(0,0,59,59,2,true,ILI9341_BLUE);
    testmap2.draw_line_from_points(59,0,0,59,1,false,ILI9341_GREEN);
    testmap2.draw_line_from_pt_slope(30,30,INFINITY,3,true,0,ILI9341_YELLOW);

    // --- Circles ---
    testmap2.draw_circle(30,10,8,3,true,ILI9341_YELLOW);
    testmap2.draw_circle(30,10,8,3,false,ILI9341_PURPLE);
    testmap2.draw_circle(50,50,5,0,true,ILI9341_ORANGE);

    // // --- Rectangle ---
    testmap2.draw_rect(10,40,20,15,3,false,ILI9341_CYAN);

    // --- Filled triangle ---
    testmap2.fill_triangle(5,55,25,45,45,55,ILI9341_MAGENTA);

    // --- Blit example ---
    uint16_t sampleArray[4] = {ILI9341_RED, ILI9341_GREEN, ILI9341_BLUE, ILI9341_YELLOW};
    testmap3.blitFromArray(sampleArray, 2, 2, BlitMode::STRETCH, ScaleMode::BILINEAR);

    // Fill white background
    testmap.draw_rect(0,0,testmap.width(),testmap.height(),0,true,ILI9341_WHITE);

    // Draw semi-transparent overlapping rectangles
    testmap.draw_rect(5,5,30,20,0,true,ILI9341_RED,0.6f);    // 60% red
    testmap.draw_rect(15,10,30,20,0,true,ILI9341_BLUE,0.6f);  // 60% blue overlaps red

    // Draw semi-transparent lines crossing
    testmap.draw_line_from_points(0,0,59,59,4,true,ILI9341_GREEN,0.7f);  // 70% green diagonal
    testmap.draw_line_from_points(0,59,59,0,4,true,ILI9341_YELLOW,0.7f); // 70% yellow diagonal

    // Draw semi-transparent circle on top
    testmap.draw_circle(30,30,15,0,true,ILI9341_MAGENTA,0.5f); // 50% magenta
}

void loop()
{
    display.clear();
    display.fillRegion(150, 150, testmap.width(),  testmap.height(),  testmap.getPixels(),  testmap.getMask());
    display.fillRegion(80,  80,  testmap3.width(), testmap3.height(), testmap3.getPixels(), testmap3.getMask());
    display.fillRegion(80,  80,  testmap2.width(), testmap2.height(), testmap2.getPixels(), testmap2.getMask());
    display.swap();
}
