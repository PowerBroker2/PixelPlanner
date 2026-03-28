#pragma once
#include <stdint.h>

// RGB888 → RGB565
constexpr uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) |
           ((g & 0xFC) << 3) |
           (b >> 3);
}

// ======================================================
// PRIMARY COLORS
// ======================================================
constexpr uint16_t RED         = RGB565(255,0,0);
constexpr uint16_t GREEN       = RGB565(0,255,0);
constexpr uint16_t BLUE        = RGB565(0,0,255);

// ======================================================
// SECONDARY COLORS
// ======================================================
constexpr uint16_t YELLOW      = RGB565(255,255,0);
constexpr uint16_t CYAN        = RGB565(0,255,255);
constexpr uint16_t MAGENTA     = RGB565(255,0,255);

// ======================================================
// BASICS
// ======================================================
constexpr uint16_t BLACK       = RGB565(0,0,0);
constexpr uint16_t WHITE       = RGB565(255,255,255);
constexpr uint16_t GRAY        = RGB565(128,128,128);

// ======================================================
// RED / ORANGE FAMILY
// ======================================================
constexpr uint16_t DARK_RED        = RGB565(139,0,0);
constexpr uint16_t FIREBRICK       = RGB565(178,34,34);
constexpr uint16_t CRIMSON         = RGB565(220,20,60);
constexpr uint16_t INDIAN_RED      = RGB565(205,92,92);
constexpr uint16_t LIGHT_CORAL     = RGB565(240,128,128);
constexpr uint16_t SALMON          = RGB565(250,128,114);
constexpr uint16_t DARK_SALMON     = RGB565(233,150,122);
constexpr uint16_t CORAL           = RGB565(255,127,80);
constexpr uint16_t TOMATO          = RGB565(255,99,71);
constexpr uint16_t ORANGE_RED      = RGB565(255,69,0);
constexpr uint16_t DARK_ORANGE     = RGB565(255,140,0);
constexpr uint16_t ORANGE          = RGB565(255,165,0);
constexpr uint16_t GOLD            = RGB565(255,215,0);
constexpr uint16_t PEACH           = RGB565(255,218,185);
constexpr uint16_t PAPAYA_WHIP     = RGB565(255,239,213);
constexpr uint16_t MOCASSIN        = RGB565(255,228,181);

// ======================================================
// YELLOW / GREEN TRANSITION
// ======================================================
constexpr uint16_t KHAKI           = RGB565(240,230,140);
constexpr uint16_t DARK_KHAKI      = RGB565(189,183,107);
constexpr uint16_t OLIVE           = RGB565(128,128,0);
constexpr uint16_t OLIVE_DRAB      = RGB565(107,142,35);
constexpr uint16_t YELLOW_GREEN    = RGB565(154,205,50);
constexpr uint16_t LIME_GREEN      = RGB565(50,205,50);
constexpr uint16_t LAWN_GREEN      = RGB565(124,252,0);
constexpr uint16_t CHARTREUSE      = RGB565(127,255,0);

// ======================================================
// GREEN FAMILY
// ======================================================
constexpr uint16_t DARK_GREEN      = RGB565(0,100,0);
constexpr uint16_t FOREST_GREEN    = RGB565(34,139,34);
constexpr uint16_t GREEN_YELLOW    = RGB565(173,255,47);
constexpr uint16_t SPRING_GREEN    = RGB565(0,255,127);
constexpr uint16_t MEDIUM_SPRING   = RGB565(0,250,154);
constexpr uint16_t SEA_GREEN       = RGB565(46,139,87);
constexpr uint16_t MEDIUM_SEA      = RGB565(60,179,113);
constexpr uint16_t LIGHT_GREEN     = RGB565(144,238,144);
constexpr uint16_t PALE_GREEN      = RGB565(152,251,152);

// ======================================================
// CYAN / TEAL FAMILY
// ======================================================
constexpr uint16_t TEAL            = RGB565(0,128,128);
constexpr uint16_t DARK_CYAN       = RGB565(0,139,139);
constexpr uint16_t LIGHT_CYAN      = RGB565(224,255,255);
constexpr uint16_t PALE_TURQUOISE  = RGB565(175,238,238);
constexpr uint16_t TURQUOISE       = RGB565(64,224,208);
constexpr uint16_t MEDIUM_TURQ     = RGB565(72,209,204);
constexpr uint16_t DARK_TURQ       = RGB565(0,206,209);
constexpr uint16_t AQUAMARINE      = RGB565(127,255,212);

// ======================================================
// BLUE FAMILY
// ======================================================
constexpr uint16_t NAVY            = RGB565(0,0,128);
constexpr uint16_t DARK_BLUE       = RGB565(0,0,139);
constexpr uint16_t MEDIUM_BLUE     = RGB565(0,0,205);
constexpr uint16_t ROYAL_BLUE      = RGB565(65,105,225);
constexpr uint16_t DODGER_BLUE     = RGB565(30,144,255);
constexpr uint16_t DEEP_SKY_BLUE   = RGB565(0,191,255);
constexpr uint16_t SKY_BLUE        = RGB565(135,206,235);
constexpr uint16_t LIGHT_SKY       = RGB565(135,206,250);
constexpr uint16_t STEEL_BLUE      = RGB565(70,130,180);
constexpr uint16_t LIGHT_STEEL     = RGB565(176,196,222);
constexpr uint16_t POWDER_BLUE     = RGB565(176,224,230);

// ======================================================
// PURPLE / MAGENTA FAMILY
// ======================================================
constexpr uint16_t INDIGO          = RGB565(75,0,130);
constexpr uint16_t PURPLE          = RGB565(128,0,128);
constexpr uint16_t DARK_MAGENTA    = RGB565(139,0,139);
constexpr uint16_t BLUE_VIOLET     = RGB565(138,43,226);
constexpr uint16_t DARK_VIOLET     = RGB565(148,0,211);
constexpr uint16_t MEDIUM_PURPLE   = RGB565(147,112,219);
constexpr uint16_t THISTLE         = RGB565(216,191,216);
constexpr uint16_t PLUM            = RGB565(221,160,221);
constexpr uint16_t VIOLET          = RGB565(238,130,238);
constexpr uint16_t ORCHID          = RGB565(218,112,214);
constexpr uint16_t MEDIUM_ORCHID   = RGB565(186,85,211);
constexpr uint16_t HOT_PINK        = RGB565(255,105,180);
constexpr uint16_t DEEP_PINK       = RGB565(255,20,147);

// ======================================================
// BROWNS / EARTH
// ======================================================
constexpr uint16_t MAROON          = RGB565(128,0,0);
constexpr uint16_t BROWN           = RGB565(165,42,42);
constexpr uint16_t SADDLE_BROWN    = RGB565(139,69,19);
constexpr uint16_t SIENNA          = RGB565(160,82,45);
constexpr uint16_t CHOCOLATE       = RGB565(210,105,30);
constexpr uint16_t PERU            = RGB565(205,133,63);
constexpr uint16_t TAN             = RGB565(210,180,140);
constexpr uint16_t BURLYWOOD       = RGB565(222,184,135);
constexpr uint16_t WHEAT           = RGB565(245,222,179);

// ======================================================
// LIGHT / NEUTRAL
// ======================================================
constexpr uint16_t BEIGE           = RGB565(245,245,220);
constexpr uint16_t LINEN           = RGB565(250,240,230);
constexpr uint16_t ANTIQUE_WHITE   = RGB565(250,235,215);
constexpr uint16_t SEASHELL        = RGB565(255,245,238);
constexpr uint16_t SNOW            = RGB565(255,250,250);
constexpr uint16_t MINT_CREAM      = RGB565(245,255,250);
constexpr uint16_t HONEYDEW        = RGB565(240,255,240);
constexpr uint16_t AZURE           = RGB565(240,255,255);
constexpr uint16_t ALICE_BLUE      = RGB565(240,248,255);
constexpr uint16_t GHOST_WHITE     = RGB565(248,248,255);
constexpr uint16_t LAVENDER        = RGB565(230,230,250);

// ======================================================
// GRAYS (unique steps)
// ======================================================
constexpr uint16_t GRAY_10 = RGB565(26,26,26);
constexpr uint16_t GRAY_20 = RGB565(51,51,51);
constexpr uint16_t GRAY_30 = RGB565(77,77,77);
constexpr uint16_t GRAY_40 = RGB565(102,102,102);
constexpr uint16_t GRAY_50 = RGB565(128,128,128);
constexpr uint16_t GRAY_60 = RGB565(153,153,153);
constexpr uint16_t GRAY_70 = RGB565(179,179,179);
constexpr uint16_t GRAY_80 = RGB565(204,204,204);
constexpr uint16_t GRAY_90 = RGB565(230,230,230);