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
    // testmap2.draw_rect(0,0,testmap2.width(),testmap2.height(),0,true,WHITE);

    // --- Rotated rectangle ---
    testmap2.draw_rect_rotated(5,5,20,10,30,15,10,2,true,CYAN);
    testmap2.draw_rect_rotated(5,5,20,10,30,15,10,2,false,RED);

    // --- Lines ---
    testmap2.draw_line_from_points(0,0,59,59,2,true,BLUE);
    testmap2.draw_line_from_points(59,0,0,59,1,false,GREEN);
    testmap2.draw_line_from_pt_slope(30,30,INFINITY,3,true,0,YELLOW);

    // --- Circles ---
    testmap2.draw_circle(30,10,8,3,true,YELLOW);
    testmap2.draw_circle(30,10,8,3,false,PURPLE);
    testmap2.draw_circle(50,50,5,0,true,ORANGE);

    // // --- Rectangle ---
    testmap2.draw_rect(10,40,20,15,3,false,CYAN);

    // --- Filled triangle ---
    testmap2.fill_triangle(5,55,25,45,45,55,MAGENTA);

    // --- Blit example ---
    uint16_t sampleArray[4] = {RED, GREEN, BLUE, YELLOW};
    testmap3.blitFromArray(sampleArray, 2, 2, BlitMode::STRETCH, ScaleMode::BILINEAR);

    // Fill white background
    testmap.draw_rect(0,0,testmap.width(),testmap.height(),0,true,WHITE);

    // Draw semi-transparent overlapping rectangles
    testmap.draw_rect(5,5,30,20,0,true,RED,0.6f);    // 60% red
    testmap.draw_rect(15,10,30,20,0,true,BLUE,0.6f);  // 60% blue overlaps red

    // Draw semi-transparent lines crossing
    testmap.draw_line_from_points(0,0,59,59,4,true,GREEN,0.7f);  // 70% green diagonal
    testmap.draw_line_from_points(0,59,59,0,4,true,YELLOW,0.7f); // 70% yellow diagonal

    // Draw semi-transparent circle on top
    testmap.draw_circle(30,30,15,0,true,MAGENTA,0.5f); // 50% magenta
}

void loop()
{
    display.clear();
    display.fillRegion(150, 150, testmap.width(),  testmap.height(),  testmap.getPixels(),  testmap.getMask());
    display.fillRegion(80,  80,  testmap3.width(), testmap3.height(), testmap3.getPixels(), testmap3.getMask());
    display.fillRegion(80,  80,  testmap2.width(), testmap2.height(), testmap2.getPixels(), testmap2.getMask());
    display.swap();
}
