#include <Arduino.h>
#include "TeensyTFT.h"
#include "PixelPlanner.h"

// TFT display
TeensyTFT display(10, 9, 255);

// Frame buffer for PixelPlanner
uint16_t frame[TeensyTFT::PHYS_WIDTH * TeensyTFT::PHYS_HEIGHT];
PixelPlanner scene(frame, TeensyTFT::PHYS_WIDTH, TeensyTFT::PHYS_HEIGHT);

// Area sizes
const int S1 = 32, S2 = 24;

// Pixel data
uint16_t redSquare[S1*S1];
uint16_t blueCircle[S2*S2];

// Red square motion
float redX = 50, redY = 50;
float redVX = 1.2, redVY = 0.9;

// Blue circle motion
float blueX = 200, blueY = 100;
float blueVX = -0.8, blueVY = 1.1;

// Area indices
int areaRed, areaBlue;

void setup() {
    Serial.begin(115200);
    display.begin(80000000, 1);

    // Red square pixels
    for(int i=0;i<S1*S1;i++) redSquare[i]=ILI9341_RED;

    // Blue circle pixels
    for(int row=0;row<S2;row++) {
        for(int col=0;col<S2;col++) {
            int dx=col-S2/2, dy=row-S2/2;
            blueCircle[row*S2+col] = (dx*dx+dy*dy<=(S2/2)*(S2/2)) ? ILI9341_BLUE : 0x0000;
        }
    }

    // Add areas to scene
    areaRed = scene.addArea(S1, S1, (int)redX, (int)redY, AreaAnchor::Center, 0, redSquare, true, 1);
    areaBlue = scene.addArea(S2, S2, (int)blueX, (int)blueY, AreaAnchor::Center, 0, blueCircle, true, 0);
}

// Check if two rectangles overlap
bool checkOverlap(float x1, float y1, int w1, int h1,
                  float x2, float y2, int w2, int h2) 
{
    float left1 = x1 - w1/2.0f;
    float right1 = x1 + w1/2.0f;
    float top1 = y1 - h1/2.0f;
    float bottom1 = y1 + h1/2.0f;

    float left2 = x2 - w2/2.0f;
    float right2 = x2 + w2/2.0f;
    float top2 = y2 - h2/2.0f;
    float bottom2 = y2 + h2/2.0f;

    return !(left1 >= right2 || right1 <= left2 || top1 >= bottom2 || bottom1 <= top2);
}

void loop() {
    int halfWR = S1/2, halfHR = S1/2;
    int halfWB = S2/2, halfHB = S2/2;

    // Bounce red square
    if(redX + redVX < halfWR || redX + redVX > display.width() - halfWR) redVX = -redVX;
    if(redY + redVY < halfHR || redY + redVY > display.height() - halfHR) redVY = -redVY;
    redX += redVX; redY += redVY;
    scene.updateAreaLocation(areaRed, (int)redX, (int)redY, AreaAnchor::Center);

    // Bounce blue circle
    if(blueX + blueVX < halfWB || blueX + blueVX > display.width() - halfWB) blueVX = -blueVX;
    if(blueY + blueVY < halfHB || blueY + blueVY > display.height() - halfHB) blueVY = -blueVY;
    blueX += blueVX; blueY += blueVY;
    scene.updateAreaLocation(areaBlue, (int)blueX, (int)blueY, AreaAnchor::Center);

    // Set visibility based on overlap
    bool overlap = checkOverlap(redX, redY, S1, S1, blueX, blueY, S2, S2);
    scene.setAreaVisible(areaRed, !overlap);
    scene.setAreaVisible(areaBlue, !overlap);

    // Compose scene into buffer and draw
    scene.compose(ILI9341_BLACK);
    display.fillRegion(0, 0, display.width(), display.height(), frame);
    display.swap();
}