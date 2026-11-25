#pragma once
#include <Arduino.h>
#include <limits.h>
#include <stdlib.h>
#include "Colors.h"

#ifndef CUSTOM_BITMAP_FONT_H
#define CUSTOM_BITMAP_FONT_H

struct Glyph {
  const bool* data;
  int width;
};

class BitmapFont {
public:
  typedef Glyph (*LookupFunc)(char);
private:
  LookupFunc _lookup;
  int _height;
public:
  BitmapFont(LookupFunc lookup, int height) : _lookup(lookup), _height(height) {}
  inline Glyph lookupChar(char c) const { return _lookup(c); }
  inline int getHeight() const { return _height; }
};

#endif // CUSTOM_BITMAP_FONT_H

enum class BitmapFilter { PRESERVE_STROKES, NEAREST };

// Partition the source raster into output cells when shrinking. Every ink pixel
// belongs to a cell, so thin strokes cannot disappear between sample points.
inline bool sampleBitmapGlyph(Glyph glyph, int nativeHeight, int width, int height, int x, int y,
                              BitmapFilter filter = BitmapFilter::PRESERVE_STROKES)
{
    if (!glyph.data || glyph.width <= 0 || nativeHeight <= 0 || x < 0 || y < 0 ||
        x >= width || y >= height || size_t(glyph.width) > SIZE_MAX / size_t(nativeHeight))
        return false;
    const int left = width == glyph.width ? x : int(int64_t(x) * glyph.width / width);
    const int top = height == nativeHeight ? y : int(int64_t(y) * nativeHeight / height);
    const int right = filter == BitmapFilter::PRESERVE_STROKES && width < glyph.width
        ? int((int64_t(x) + 1) * glyph.width / width) : left + 1;
    const int bottom = filter == BitmapFilter::PRESERVE_STROKES && height < nativeHeight
        ? int((int64_t(y) + 1) * nativeHeight / height) : top + 1;
    for (int row = top; row < bottom; ++row)
    {
        const bool* source = glyph.data + size_t(row) * glyph.width;
        for (int col = left; col < right; ++col)
            if (pgm_read_byte(source + col)) return true;
    }
    return false;
}

/**
 * @brief Defines how a source image is positioned within the destination Pixmap.
 */
enum class BlitMode { TOP_LEFT,
                      TOP_RIGHT,
                      BOTTOM_LEFT,
                      BOTTOM_RIGHT,
                      CENTER,
                      STRETCH,
                      FIT,
                      FILL };

/**
 * @brief Defines how pixel values are interpolated during scaling.
 */
enum class ScaleMode { NEAREST,
                       BILINEAR,
                       AREA };

struct PixmapRect
{
    int x, y, width, height;
};

enum class StrokeJoin { MITER, BEVEL, ROUND };
enum class StrokeCap { BUTT, SQUARE, ROUND };
enum class Antialiasing { OFF, COVERAGE };

struct StrokeStyle
{
    float width = 1.0f;
    StrokeJoin join = StrokeJoin::MITER;
    StrokeCap cap = StrokeCap::BUTT;
    float miterLimit = 4.0f;
    Antialiasing antialias = Antialiasing::OFF;
};

namespace pixelplanner_detail
{
    using PixelSink = void (*)(void*, int, int, uint8_t);
    void strokePath(const float* points, size_t count, bool closed, const StrokeStyle& style,
                    PixmapRect clip, void* target, PixelSink put);
    void fillPath(const float* points, size_t count, PixmapRect clip, void* target, PixelSink put);
    void radialPath(int cx, int cy, int radius, int innerRadius, float angle, float span,
                    PixmapRect clip, void* target, PixelSink put);
}

struct BlitTransform
{
    // (x, y) is the destination position of the pivot within the source region.
    // Coordinates are measured from pixel edges. Positive angles turn clockwise.
    float x, y;
    float angle = 0.0f;
    float scaleX = 1.0f, scaleY = 1.0f;
    float pivotX = 0.0f, pivotY = 0.0f;

    BlitTransform(float x, float y) : x(x), y(y) {}
};

/**
 * @brief Alpha blend two RGB565 colors.
 *
 * @param dst   Destination pixel (background).
 * @param src   Source pixel (foreground).
 * @param alpha Blend factor in range [0.0, 1.0].
 * @return Resulting blended RGB565 color.
 */
uint16_t blend565(uint16_t dst,
                  uint16_t src,
                  float    alpha);

/**
 * @brief Rotate a point around a pivot.
 *
 * @param px        Input point X.
 * @param py        Input point Y.
 * @param cx        Pivot center X.
 * @param cy        Pivot center Y.
 * @param angle_deg Rotation angle in degrees.
 * @param out_x     Output rotated X.
 * @param out_y     Output rotated Y.
 */
void rotate_point(float  px,
                  float  py,
                  float  cx,
                  float  cy,
                  float  angle_deg,
                  float &out_x,
                  float &out_y);

/**
 * @brief Fixed-size 2D pixel buffer with RGB565 color and mask support.
 *
 * @tparam WIDTH  Width of the pixmap in pixels.
 * @tparam HEIGHT Height of the pixmap in pixels.
 */
template <size_t WIDTH, size_t HEIGHT, bool COVERAGE = false>
class Pixmap
{
private:
    static_assert(WIDTH > 0 && HEIGHT > 0, "Pixmap dimensions must be positive");
    static_assert(WIDTH <= INT_MAX && HEIGHT <= INT_MAX && HEIGHT <= SIZE_MAX / WIDTH,
                  "Pixmap dimensions exceed the index range");

    static int lowerBound(int64_t position, size_t extent)
    {
        return position <= 0 ? 0 : position >= int64_t(extent) ? int(extent) : int(position);
    }

    uint16_t pixels[WIDTH*HEIGHT] = {};     ///< Pixel buffer (RGB565)
    bool     mask[WIDTH*HEIGHT]   = {};     ///< Mask indicating written pixels (false = not yet written)
    uint8_t coverage[COVERAGE ? WIDTH * HEIGHT : 1] = {};
    // Zero insets mean a full clip and keep zero-initialized pixmaps in BSS.
    struct { int left, top, right, bottom; } clipInsets{};

    /**
     * @brief Convert 2D coordinates to linear index (caller must ensure in-bounds).
     */
    constexpr size_t index(int x, int y) const { return size_t(y) * WIDTH + size_t(x); }

    uint8_t coverageAt(size_t i) const { return COVERAGE ? coverage[i] : mask[i] ? 255 : 0; }

    void composite(size_t i, uint16_t color, float alpha)
    {
        if (alpha >= 1.0f)
        {
            pixels[i] = color;
            mask[i] = true;
            if (COVERAGE)
                coverage[i] = 255;
            return;
        }
        if (COVERAGE)
        {
            const unsigned incoming = unsigned(alpha * 255.0f + 0.5f);
            if (!incoming)
                return;
            const unsigned retained = (unsigned(coverage[i]) * (255 - incoming) + 127) / 255;
            const unsigned total = incoming + retained;
            pixels[i] = retained ? blend565(pixels[i], color, float(incoming) / total) : color;
            coverage[i] = uint8_t(total);
        }
        else
            pixels[i] = blend565(pixels[i], color, alpha);
        mask[i] = true;
    }

    struct Paint
    {
        Pixmap& target;
        uint16_t color;
        float alpha;

        static void put(void* context, int x, int y, uint8_t opacity)
        {
            auto& paint = *static_cast<Paint*>(context);
            paint.target.setPixelValue(x, y, paint.color, paint.alpha * (opacity / 255.0f));
        }
    };

public:
    /**
     * @brief Get pixmap width.
     */
    constexpr size_t width()  const { return WIDTH;  }

    /**
     * @brief Get pixmap height.
     */
    constexpr size_t height() const { return HEIGHT; }

    /**
     * @brief Clear all pixels and mask.
     */
    void clear()
    {
        memset(pixels, 0, sizeof(pixels));
        memset(mask,   0, sizeof(mask));
        if (COVERAGE)
            memset(coverage, 0, sizeof(coverage));
    }

    // Drawing respects the clip. clear() resets the entire pixmap.
    PixmapRect getClipRect() const
    {
        return {clipInsets.left, clipInsets.top,
                int(WIDTH) - clipInsets.left - clipInsets.right,
                int(HEIGHT) - clipInsets.top - clipInsets.bottom};
    }
    void resetClipRect() { clipInsets = {}; }
    void setClipRect(PixmapRect rect)
    {
        resetClipRect();
        intersectClipRect(rect);
    }
    void intersectClipRect(PixmapRect rect)
    {
        const auto clip = getClipRect();
        const int64_t left = max(int64_t(clip.x), int64_t(rect.x));
        const int64_t top = max(int64_t(clip.y), int64_t(rect.y));
        const int64_t right = min(int64_t(clip.x) + clip.width,
                                  int64_t(rect.x) + max(0, rect.width));
        const int64_t bottom = min(int64_t(clip.y) + clip.height,
                                   int64_t(rect.y) + max(0, rect.height));
        if (left >= right || top >= bottom)
            clipInsets = {0, 0, int(WIDTH), int(HEIGHT)};
        else
            clipInsets = {int(left), int(top), int(WIDTH) - int(right), int(HEIGHT) - int(bottom)};
    }

    /**
     * @brief Check if coordinates are inside bounds.
     */
    bool inBounds(int x, int y) const { return ((x >= 0) && (y >= 0) && (x < (int)WIDTH) && (y < (int)HEIGHT)); }

    bool inClip(int x, int y) const
    {
        return x >= clipInsets.left && y >= clipInsets.top &&
               x < int(WIDTH) - clipInsets.right && y < int(HEIGHT) - clipInsets.bottom;
    }

    /**
     * @brief Set a pixel with optional alpha blending.
     *
     * @param x     X coordinate.
     * @param y     Y coordinate.
     * @param value RGB565 color.
     * @param alpha Blend factor [0.0–1.0].
     */
    void setPixelValue(int      x,
                       int      y,
                       uint16_t value,
                       float    alpha = 1.0f)
    {
        if (inClip(x, y) && alpha > 0)
            composite(index(x, y), value, alpha);
    }

    /**
     * @brief Get pixel color.
     */
    uint16_t getPixelValue(int x, int y) const { return inBounds(x, y) ? pixels[index(x, y)] : 0; }

    /**
     * @brief Get mask value.
     */
    bool getMaskValue(int x, int y) const { return inBounds(x, y) ? mask[index(x, y)] : false; }

    uint8_t getCoverage(int x, int y) const { return inBounds(x, y) ? coverageAt(index(x, y)) : 0; }
    const uint8_t* getCoverage() const { return COVERAGE ? coverage : nullptr; }

    /**
     * @brief Get raw pixel buffer pointer.
     */
    const uint16_t* getPixels() const { return pixels; }

    /**
     * @brief Get raw mask buffer pointer.
     */
    const bool*     getMask()   const { return mask;   }

    /**
     * @brief Draw a line between two points.
     *
     * Supports thickness and optional extension to pixmap borders.
     *
     * @param x1               Start X.
     * @param y1               Start Y.
     * @param x2               End X.
     * @param y2               End Y.
     * @param thickness        Line thickness in pixels.
     * @param border_to_border If true, extends the line to the pixmap edges.
     * @param color            RGB565 color.
     * @param alpha            Blend factor [0.0–1.0].
     */
    void draw_line_from_points(float    x1,
                               float    y1,
                               float    x2,
                               float    y2,
                               float    thickness,
                               bool     border_to_border,
                               uint16_t color,
                               float    alpha = 1.0f)
    {
        if (!isfinite(x1) || !isfinite(y1) || !isfinite(x2) || !isfinite(y2) ||
            !isfinite(thickness))
            return;

        const float dx = x2 - x1, dy = y2 - y1;
        const float length = hypotf(dx, dy);
        if (!isfinite(length))
            return;
        if (length == 0.0f)
        {
            if (x1 >= -0.5f && x1 < WIDTH - 0.5f && y1 >= -0.5f && y1 < HEIGHT - 0.5f)
                setPixelValue(int(roundf(x1)), int(roundf(y1)), color, alpha);
            return;
        }

        if (border_to_border)
        {
            float start = -INFINITY, end = INFINITY;
            const auto clip = [&](float origin, float step, float limit) {
                if (step == 0.0f)
                    return origin >= 0.0f && origin <= limit;
                const float a = -origin / step, b = (limit - origin) / step;
                start = max(start, min(a, b));
                end = min(end, max(a, b));
                return start <= end;
            };
            if (!clip(x1, dx, WIDTH - 1) || !clip(y1, dy, HEIGHT - 1))
                return;
            x2 = x1 + end * dx;
            y2 = y1 + end * dy;
            x1 += start * dx;
            y1 += start * dy;
        }

        // Center the stroke on the endpoints and include the end pixels.
        const float half = max(1.0f, thickness) * 0.5f;
        const float u = dx / length, v = dy / length;
        const float px = -v * half, py = u * half;
        x1 -= u * 0.5f; y1 -= v * 0.5f;
        x2 += u * 0.5f; y2 += v * 0.5f;
        const float vertices[] = {
            x1 + px + 0.5f, y1 + py + 0.5f, x2 + px + 0.5f, y2 + py + 0.5f,
            x2 - px + 0.5f, y2 - py + 0.5f, x1 - px + 0.5f, y1 - py + 0.5f
        };
        draw_polygon(vertices, 4, 0.0f, true, color, alpha);
    }

    /**
     * @brief Draw a line from a point and slope.
     *
     * @param x                Anchor X coordinate.
     * @param y                Anchor Y coordinate.
     * @param slope            Rise-over-run slope; use INFINITY for vertical.
     * @param thickness        Line thickness in pixels.
     * @param border_to_border If true, extends the line to the pixmap edges.
     * @param line_distance    Length of the line when border_to_border is false.
     * @param color            RGB565 color.
     * @param alpha            Blend factor [0.0–1.0].
     */
    void draw_line_from_pt_slope(float    x,
                                 float    y,
                                 float    slope,
                                 float    thickness,
                                 bool     border_to_border,
                                 float    line_distance,
                                 uint16_t color,
                                 float    alpha = 1.0f)
    {
        if (!isfinite(x) || !isfinite(y) || isnan(slope) ||
            (!border_to_border && !isfinite(line_distance)))
            return;
        const float length = hypotf(1.0f, slope);
        const float dx = isinf(slope) ? 0.0f : 1.0f / length;
        const float dy = isinf(slope) ? 1.0f : slope / length;
        const float half = border_to_border ? 0.5f : line_distance * 0.5f;
        draw_line_from_points(x - dx * half, y - dy * half, x + dx * half, y + dy * half,
                              thickness, border_to_border, color, alpha);
    }

    /**
     * @brief Draw a circle or ring.
     *
     * @param cx        Center X.
     * @param cy        Center Y.
     * @param radius    Outer radius in pixels.
     * @param thickness Ring thickness in pixels (ignored when solid is true).
     * @param solid     If true, fills the circle.
     * @param color     RGB565 color.
     * @param alpha     Blend factor [0.0–1.0].
     */
    void draw_circle(int      cx,
                     int      cy,
                     int      radius,
                     int      thickness,
                     bool     solid,
                     uint16_t color,
                     float    alpha = 1.0f,
                     Antialiasing antialias = Antialiasing::OFF)
    {
        const auto clip = getClipRect();
        if (radius <= 0 || !(alpha > 0))
            return;
        const int64_t outer = radius;
        const int64_t inner = solid ? 0 : max(0, radius - max(1, thickness));
        if (antialias == Antialiasing::COVERAGE && alpha > 0)
        {
            Paint paint{*this, color, min(1.0f, alpha)};
            pixelplanner_detail::radialPath(cx, cy, radius, int(inner), 0, 360,
                                           clip, &paint, Paint::put);
            return;
        }
        const int firstX = max(clip.x, lowerBound(int64_t(cx) - outer, WIDTH));
        const int lastX = min(clip.x + clip.width, lowerBound(int64_t(cx) + outer + 1, WIDTH));
        const int firstY = max(clip.y, lowerBound(int64_t(cy) - outer, HEIGHT));
        const int lastY = min(clip.y + clip.height, lowerBound(int64_t(cy) + outer + 1, HEIGHT));
        for (int y = firstY; y < lastY; ++y)
        {
            const int64_t dy = int64_t(y) - cy;
            for (int x = firstX; x < lastX; ++x)
            {
                const int64_t dx = int64_t(x) - cx;
                const int64_t squared = dx * dx + dy * dy;
                if (squared <= outer * outer && squared >= inner * inner)
                    setPixelValue(x, y, color, alpha);
            }
        }
    }

    /**
     * @brief Draw a rectangle (filled or outlined).
     *
     * @param x         Left edge X.
     * @param y         Top edge Y.
     * @param w         Width in pixels.
     * @param h         Height in pixels.
     * @param thickness Border thickness in pixels (ignored when solid is true).
     * @param solid     If true, fills the rectangle.
     * @param color     RGB565 color.
     * @param alpha     Blend factor [0.0–1.0].
     */
    void draw_rect(int      x,
                   int      y,
                   int      w,
                   int      h,
                   int      thickness,
                   bool     solid,
                   uint16_t color,
                   float    alpha = 1.0f)
    {
        const auto clip = getClipRect();
        if (w <= 0 || h <= 0 || !(alpha > 0))
            return;
        const int64_t right = int64_t(x) + w, bottom = int64_t(y) + h;
        const int border = max(1, min(thickness, min(w / 2, h / 2)));
        const int firstX = max(clip.x, lowerBound(x, WIDTH));
        const int lastX = min(clip.x + clip.width, lowerBound(right, WIDTH));
        const int firstY = max(clip.y, lowerBound(y, HEIGHT));
        const int lastY = min(clip.y + clip.height, lowerBound(bottom, HEIGHT));
        for (int row = firstY; row < lastY; ++row)
            for (int col = firstX; col < lastX; ++col)
                if (solid || int64_t(col) - x < border || right - col <= border ||
                    int64_t(row) - y < border || bottom - row <= border)
                    setPixelValue(col, row, color, alpha);
    }

    /**
     * @brief Fill a triangle using barycentric edge tests.
     *
     * @param x0    First vertex X.
     * @param y0    First vertex Y.
     * @param x1    Second vertex X.
     * @param y1    Second vertex Y.
     * @param x2    Third vertex X.
     * @param y2    Third vertex Y.
     * @param color RGB565 color.
     * @param alpha Blend factor [0.0–1.0].
     */
    void fill_triangle(int      x0,
                       int      y0,
                       int      x1,
                       int      y1,
                       int      x2,
                       int      y2,
                       uint16_t color,
                       float    alpha = 1.0f)
    {
        const auto clip = getClipRect();
        const int minX = max(clip.x, min(x0, min(x1, x2)));
        const int maxX = min(clip.x + clip.width - 1, max(x0, max(x1, x2)));
        const int minY = max(clip.y, min(y0, min(y1, y2)));
        const int maxY = min(clip.y + clip.height - 1, max(y0, max(y1, y2)));
        if (minX > maxX || minY > maxY || !(alpha > 0))
            return;
        const auto edge = [](int ax, int ay, int bx, int by, int px, int py) {
            return (double(bx) - ax) * (double(py) - ay) - (double(by) - ay) * (double(px) - ax);
        };
        if (edge(x0, y0, x1, y1, x2, y2) == 0)
        {
            draw_line_from_points(min(x0, min(x1, x2)), x0 == x1 && x1 == x2 ? min(y0, min(y1, y2)) :
                                  (x0 <= x1 && x0 <= x2 ? y0 : (x1 <= x2 ? y1 : y2)),
                                  max(x0, max(x1, x2)), x0 == x1 && x1 == x2 ? max(y0, max(y1, y2)) :
                                  (x0 >= x1 && x0 >= x2 ? y0 : (x1 >= x2 ? y1 : y2)),
                                  1, false, color, alpha);
            return;
        }
        for (int y = minY; y <= maxY; ++y)
            for (int x = minX; x <= maxX; ++x)
            {
                const double a = edge(x0, y0, x1, y1, x, y);
                const double b = edge(x1, y1, x2, y2, x, y);
                const double c = edge(x2, y2, x0, y0, x, y);
                if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0))
                    setPixelValue(x, y, color, alpha);
            }
    }

    /**
     * @brief Draw a rotated rectangle (filled or outlined).
     *
     * @param x         Top-left X before rotation.
     * @param y         Top-left Y before rotation.
     * @param w         Width in pixels.
     * @param h         Height in pixels.
     * @param angle_deg Rotation angle in degrees (clockwise).
     * @param pivot_x   Pivot X for rotation.
     * @param pivot_y   Pivot Y for rotation.
     * @param thickness Border thickness in pixels (ignored when solid is true).
     * @param solid     If true, fills the rectangle.
     * @param color     RGB565 color.
     * @param alpha     Blend factor [0.0–1.0].
     */
    void draw_rect_rotated(float    x,
                           float    y,
                           float    w,
                           float    h,
                           float    angle_deg,
                           float    pivot_x,
                           float    pivot_y,
                           int      thickness,
                           bool     solid,
                           uint16_t color,
                           float    alpha = 1.0f)
    {
        if (!(w > 0.0f) || !(h > 0.0f))
            return;
        float vertices[] = {x, y, x + w, y, x + w, y + h, x, y + h};
        const float sine = sinf(angle_deg * DEG_TO_RAD), cosine = cosf(angle_deg * DEG_TO_RAD);
        for (int i = 0; i < 8; i += 2)
        {
            const float dx = vertices[i] - pivot_x, dy = vertices[i + 1] - pivot_y;
            vertices[i] = pivot_x + dx * cosine - dy * sine;
            vertices[i + 1] = pivot_y + dx * sine + dy * cosine;
        }
        draw_polygon(vertices, 4, thickness, solid, color, alpha);
    }

    /**
     * @brief Draw an arc or filled pie slice.
     *
     * Angles follow the screen convention: 0° points right, increasing clockwise
     * (because Y increases downward). Both solid (pie slice) and ring-segment
     * modes are supported.
     *
     * @param cx        Center X.
     * @param cy        Center Y.
     * @param radius    Outer radius in pixels.
     * @param thickness Ring thickness in pixels (ignored when solid is true).
     * @param solid     If true, draws a filled pie slice; otherwise a ring segment.
     * @param start_deg Start angle in degrees.
     * @param end_deg   End angle in degrees (may exceed 360 to wrap around).
     * @param color     RGB565 color.
     * @param alpha     Blend factor [0.0–1.0].
     */
    void draw_arc(int      cx,
                  int      cy,
                  int      radius,
                  int      thickness,
                  bool     solid,
                  float    start_deg,
                  float    end_deg,
                  uint16_t color,
                  float    alpha = 1.0f,
                  Antialiasing antialias = Antialiasing::OFF)
    {
        const auto clip = getClipRect();
        if (radius <= 0 || !isfinite(start_deg) || !isfinite(end_deg) || !(alpha > 0))
            return;
        const double difference = double(end_deg) - start_deg;
        if (difference >= 360.0 || difference <= -360.0)
        {
            draw_circle(cx, cy, radius, thickness, solid, color, alpha, antialias);
            return;
        }
        const float span = difference < 0 ? difference + 360.0 : difference;
        const float start = fmodf(start_deg, 360.0f);
        const int64_t outer = radius;
        const int64_t inner = solid ? 0 : max(0, radius - max(1, thickness));
        if (antialias == Antialiasing::COVERAGE && alpha > 0)
        {
            Paint paint{*this, color, min(1.0f, alpha)};
            pixelplanner_detail::radialPath(cx, cy, radius, int(inner), start, span,
                                           clip, &paint, Paint::put);
            return;
        }
        const int firstX = max(clip.x, lowerBound(int64_t(cx) - outer, WIDTH));
        const int lastX = min(clip.x + clip.width, lowerBound(int64_t(cx) + outer + 1, WIDTH));
        const int firstY = max(clip.y, lowerBound(int64_t(cy) - outer, HEIGHT));
        const int lastY = min(clip.y + clip.height, lowerBound(int64_t(cy) + outer + 1, HEIGHT));
        for (int y = firstY; y < lastY; ++y)
        {
            const int64_t dy = int64_t(y) - cy;
            for (int x = firstX; x < lastX; ++x)
            {
                const int64_t dx = int64_t(x) - cx;
                const int64_t squared = dx * dx + dy * dy;
                if (squared > outer * outer || squared < inner * inner)
                    continue;
                bool inside = squared == 0;
                for (int sy = 0; sy < 2 && !inside; ++sy)
                    for (int sx = 0; sx < 2 && !inside; ++sx)
                    {
                        const float angle = atan2f(dy + (sy ? 0.5f : -0.5f),
                                                   dx + (sx ? 0.5f : -0.5f)) / DEG_TO_RAD;
                        const float relative = fmodf(angle - start + 720.0f, 360.0f);
                        inside = relative <= span;
                    }
                if (inside)
                    setPixelValue(x, y, color, alpha);
            }
        }
    }

    /**
     * @brief Stroke an open or closed path, blending each covered pixel once.
     *
     * points contains x/y pairs in pixmap coordinates. count is the number of
     * vertices. Caps apply only to open paths.
     */
    void strokePath(const float* points, size_t count, bool closed, const StrokeStyle& style,
                    uint16_t color, float alpha = 1.0f)
    {
        const auto clip = getClipRect();
        if (!(alpha > 0) || !clip.width || !clip.height)
            return;
        Paint paint{*this, color, min(1.0f, alpha)};
        pixelplanner_detail::strokePath(points, count, closed, style, clip, &paint, Paint::put);
    }

    // Convenience for an open path with square caps and miter joins.
    void draw_polyline(const float* points,
                       size_t       count,
                       float        thickness,
                       uint16_t     color,
                       float        alpha = 1.0f)
    {
        if (count < 2 || !isfinite(thickness))
            return;
        StrokeStyle style;
        style.width = max(1.0f, thickness);
        style.cap = StrokeCap::SQUARE;
        strokePath(points, count, false, style, color, alpha);
    }

    /**
     * @brief Draw a polygon (filled or outlined) through an ordered list of vertices.
     *
     * Filled mode uses a scanline even-odd rasterizer and correctly handles both
     * convex and concave polygons. Outlined mode strokes a closed path.
     *
     * Polygons with more than 64 vertices use heap scratch space.
     *
     * @param points    Flat array of vertex coordinates [x0,y0, x1,y1, ...].
     * @param count     Number of vertices (not the number of floats).
     * @param thickness Edge thickness in pixels (ignored when solid is true).
     * @param solid     If true, fills the polygon interior.
     * @param color     RGB565 color.
     * @param alpha     Blend factor [0.0–1.0].
     */
    void draw_polygon(const float* points,
                      size_t       count,
                      float        thickness,
                      bool         solid,
                      uint16_t     color,
                      float        alpha = 1.0f,
                      Antialiasing antialias = Antialiasing::OFF)
    {
        const auto clip = getClipRect();
        if (!points || count < 3 || count > SIZE_MAX / 2 || !(alpha > 0))
            return;
        if (!solid)
        {
            if (!isfinite(thickness))
                return;
            StrokeStyle style;
            style.width = max(1.0f, thickness);
            style.antialias = antialias;
            strokePath(points, count, true, style, color, alpha);
            return;
        }
        if (antialias == Antialiasing::COVERAGE)
        {
            Paint paint{*this, color, min(1.0f, alpha)};
            pixelplanner_detail::fillPath(points, count, clip, &paint, Paint::put);
            return;
        }
        for (size_t i = 0; i < count * 2; ++i)
            if (!isfinite(points[i]))
                return;

        float minY = points[1], maxY = points[1];
        for (size_t i = 1; i < count; ++i)
        {
            minY = min(minY, points[2 * i + 1]);
            maxY = max(maxY, points[2 * i + 1]);
        }
        const auto pixelBound = [](float edge, size_t extent) {
            const float pixel = ceilf(edge - 0.5f);
            return pixel <= 0.0f ? 0 : double(pixel) >= double(extent) ? int(extent) : int(pixel);
        };
        const int firstRow = max(clip.y, pixelBound(minY, HEIGHT));
        const int lastRow = min(clip.y + clip.height, pixelBound(maxY, HEIGHT));
        if (firstRow >= lastRow || count > SIZE_MAX / sizeof(float))
            return;

        constexpr size_t localCapacity = 64;
        float local[localCapacity];
        float* intersections = count <= localCapacity ? local : static_cast<float*>(malloc(count * sizeof(float)));
        if (!intersections)
            return;
        for (int row = firstRow; row < lastRow; ++row)
        {
            size_t hits = 0;
            const double y = row + 0.5;
            for (size_t i = 0; i < count; ++i)
            {
                const size_t j = (i + 1) % count;
                const float ay = points[2 * i + 1], by = points[2 * j + 1];
                if ((ay <= y && by > y) || (by <= y && ay > y))
                {
                    const double fraction = (y - ay) / (double(by) - ay);
                    intersections[hits++] = (1 - fraction) * points[2 * i] + fraction * points[2 * j];
                }
            }

            for (size_t i = 1; i < hits; ++i)
            {
                const float value = intersections[i];
                size_t j = i;
                while (j > 0 && intersections[j - 1] > value)
                {
                    intersections[j] = intersections[j - 1];
                    --j;
                }
                intersections[j] = value;
            }
            for (size_t i = 0; i + 1 < hits; i += 2)
            {
                const int end = min(clip.x + clip.width, pixelBound(intersections[i + 1], WIDTH));
                for (int x = max(clip.x, pixelBound(intersections[i], WIDTH)); x < end; ++x)
                    setPixelValue(x, row, color, alpha);
            }
        }
        if (intersections != local)
            free(intersections);
    }

    /**
     * @brief Flood fill a contiguous region starting from a seed pixel.
     *
     * Replaces all pixels connected to the seed that share its color with
     * @p fill_color. Uses a bounded heap queue to avoid recursion.
     *
     * @param x          Seed X coordinate.
     * @param y          Seed Y coordinate.
     * @param fill_color Replacement RGB565 color.
     * @param alpha      Blend factor [0.0–1.0].
     */
    void flood_fill(int      x,
                    int      y,
                    uint16_t fill_color,
                    float    alpha = 1.0f)
    {
        const auto clip = getClipRect();
        if (!inClip(x, y) || !(alpha > 0))
            return;
        const size_t seed = index(x, y);
        const uint16_t target = pixels[seed];
        const uint16_t replacement = blend565(target, fill_color, alpha);
        constexpr size_t count = WIDTH * HEIGHT;
        using QueueIndex = size_t;
        const size_t capacity = size_t(clip.width) * clip.height;
        if (capacity > SIZE_MAX / sizeof(QueueIndex))
            return;
        auto* queue = static_cast<QueueIndex*>(malloc(capacity * sizeof(QueueIndex)));
        const bool needsVisited = COVERAGE || replacement == target;
        auto* visited = needsVisited ? static_cast<uint8_t*>(calloc(count / 8 + 1, 1)) : nullptr;
        if (!queue || (needsVisited && !visited))
        {
            free(queue);
            free(visited);
            return;
        }

        size_t head = 0, tail = 0;
        const auto visit = [&](size_t offset) {
            if (pixels[offset] != target)
                return;
            if (visited)
            {
                const uint8_t bit = uint8_t(1u << (offset % 8));
                if (visited[offset / 8] & bit)
                    return;
                visited[offset / 8] |= bit;
            }
            composite(offset, fill_color, alpha);
            queue[tail++] = offset;
        };
        visit(seed);
        while (head < tail)
        {
            const size_t offset = queue[head++];
            const size_t column = offset % WIDTH;
            const size_t row = offset / WIDTH;
            if (column > size_t(clip.x)) visit(offset - 1);
            if (column + 1 < size_t(clip.x + clip.width)) visit(offset + 1);
            if (row > size_t(clip.y)) visit(offset - WIDTH);
            if (row + 1 < size_t(clip.y + clip.height)) visit(offset + WIDTH);
        }
        free(visited);
        free(queue);
    }

    /**
     * @brief Blit another Pixmap onto this one, respecting the source mask.
     *
     * Only pixels where the source mask is set are written, allowing shaped or
     * partially-drawn source pixmaps to composite correctly over the destination.
     * All BlitMode and ScaleMode options from blitFromArray apply.
     *
     * @tparam W2       Source Pixmap width.
     * @tparam H2       Source Pixmap height.
     * @param src       Source Pixmap.
     * @param mode      Placement / scaling mode.
     * @param scaleMode Interpolation algorithm.
     * @param alpha     Blend factor [0.0–1.0].
     */
    template <size_t W2, size_t H2, bool SOURCE_COVERAGE>
    void blit_from_pixmap(const Pixmap<W2, H2, SOURCE_COVERAGE>& src,
                          BlitMode mode = BlitMode::TOP_LEFT,
                          ScaleMode scaleMode = ScaleMode::NEAREST,
                          float alpha = 1.0f,
                          int destX = 0, int destY = 0, int destW = -1, int destH = -1)
    {
        if (static_cast<const void*>(this) == static_cast<const void*>(&src))
            return;
        blit(W2, H2, mode, scaleMode, alpha, destX, destY, destW, destH,
             [&](size_t i) { return samplePixmap(src, i); });
    }

    // Scale the original pixmap directly into a destination rectangle.
    template <size_t W2, size_t H2, bool SOURCE_COVERAGE>
    void blitResized(const Pixmap<W2, H2, SOURCE_COVERAGE>& src, int x, int y, int w, int h,
                     ScaleMode scaleMode = ScaleMode::NEAREST, float alpha = 1.0f)
    {
        if (w > 0 && h > 0)
            blit_from_pixmap(src, BlitMode::STRETCH, scaleMode, alpha, x, y, w, h);
    }

    // Drawing coordinates are in the original layout. Geometry is rasterized
    // at the destination size, preserving aspect ratio.
    class ScaledDrawing
    {
        friend class Pixmap;

        ScaledDrawing(Pixmap& target, int sourceWidth, int sourceHeight)
            : _target(target),
              _scale(min(float(WIDTH) / sourceWidth, float(HEIGHT) / sourceHeight)),
              _left((WIDTH - sourceWidth * _scale) * 0.5f),
              _top((HEIGHT - sourceHeight * _scale) * 0.5f)
        {
        }

    public:
        // Map pixel centers for use with other Pixmap drawing operations.
        float mapX(float x) const { return _left + (x + 0.5f) * _scale - 0.5f; }
        float mapY(float y) const { return _top + (y + 0.5f) * _scale - 0.5f; }
        float mapLength(float length) const { return length * _scale; }
        Pixmap& target() const { return _target; }

        void draw_rect(int x, int y, int w, int h, int thickness, bool solid,
                       uint16_t color, float alpha = 1.0f)
        {
            if (w <= 0 || h <= 0)
                return;
            const float left = _left + x * _scale;
            const float top = _top + y * _scale;
            const int x0 = rounded(left), y0 = rounded(top);
            const int x1 = rounded(left + w * _scale), y1 = rounded(top + h * _scale);
            _target.draw_rect(x0, y0, int(min(int64_t(INT_MAX), int64_t(x1) - x0)),
                              int(min(int64_t(INT_MAX), int64_t(y1) - y0)),
                              rounded(mapLength(thickness)), solid, color, alpha);
        }

        void draw_circle(int x, int y, int radius, int thickness, bool solid,
                         uint16_t color, float alpha = 1.0f,
                         Antialiasing antialias = Antialiasing::OFF)
        {
            _target.draw_circle(rounded(mapX(x)), rounded(mapY(y)), rounded(mapLength(radius)),
                                rounded(mapLength(thickness)), solid, color, alpha, antialias);
        }

        void draw_arc(int x, int y, int radius, int thickness, bool solid, float start, float end,
                      uint16_t color, float alpha = 1.0f,
                      Antialiasing antialias = Antialiasing::OFF)
        {
            _target.draw_arc(rounded(mapX(x)), rounded(mapY(y)), rounded(mapLength(radius)),
                             rounded(mapLength(thickness)), solid, start, end, color, alpha, antialias);
        }

        void draw_line_from_points(float x1, float y1, float x2, float y2, float thickness,
                                   bool border_to_border, uint16_t color, float alpha = 1.0f)
        {
            _target.draw_line_from_points(mapX(x1), mapY(y1), mapX(x2), mapY(y2),
                                          mapLength(thickness), border_to_border, color, alpha);
        }

    private:
        static int rounded(float value)
        {
            const double integer = roundf(value);
            return integer >= INT_MAX ? INT_MAX : integer <= INT_MIN ? INT_MIN : int(integer);
        }

        Pixmap& _target;
        float _scale, _left, _top;
    };

    // Clear this pixmap and replay the original drawing at the new resolution.
    template <typename Draw>
    void redrawResized(int sourceWidth, int sourceHeight, Draw&& draw)
    {
        if (sourceWidth <= 0 || sourceHeight <= 0)
            return;
        clear();
        ScaledDrawing canvas(*this, sourceWidth, sourceHeight);
        draw(canvas);
    }

    void blitFromArray(const uint16_t* src, size_t srcW, size_t srcH,
                       BlitMode mode, ScaleMode scaleMode = ScaleMode::BILINEAR,
                       float alpha = 1.0f, int destX = 0, int destY = 0,
                       int destW = -1, int destH = -1, const bool* mask = nullptr)
    {
        if (src)
            blit(srcW, srcH, mode, scaleMode, alpha, destX, destY, destW, destH,
                 [&](size_t i) {
                     const uint8_t coverage = !mask || pgm_read_byte(mask + i) ? 255 : 0;
                     return Sample{pgm_read_word(src + i), coverage};
                 });
    }

    void blitFromArray(const bool* src, size_t srcW, size_t srcH, uint16_t color,
                       BlitMode mode, ScaleMode scaleMode = ScaleMode::BILINEAR,
                       float alpha = 1.0f, int destX = 0, int destY = 0,
                       int destW = -1, int destH = -1)
    {
        if (src)
            blit(srcW, srcH, mode, scaleMode, alpha, destX, destY, destW, destH,
                 [&](size_t i) { return Sample{color, uint8_t(pgm_read_byte(src + i) ? 255 : 0)}; });
    }

    // Draw a rectangular sprite from a larger asset. The source must lie within the image.
    template <size_t W2, size_t H2, bool SOURCE_COVERAGE>
    void blitRegion(const Pixmap<W2, H2, SOURCE_COVERAGE>& src, PixmapRect source, PixmapRect destination,
                    BlitMode mode = BlitMode::STRETCH, ScaleMode filter = ScaleMode::NEAREST,
                    float alpha = 1.0f)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&src))
            drawRegion(W2, H2, source, destination, mode, filter, alpha,
                       [&](size_t i) { return samplePixmap(src, i); });
    }

    void blitRegion(const uint16_t* src, size_t srcW, size_t srcH, PixmapRect source,
                    PixmapRect destination, BlitMode mode = BlitMode::STRETCH,
                    ScaleMode filter = ScaleMode::NEAREST, float alpha = 1.0f)
    {
        if (src && src != pixels)
            drawRegion(srcW, srcH, source, destination, mode, filter, alpha,
                       [&](size_t i) { return Sample{pgm_read_word(src + i), 255}; });
    }

    void blitRegion(const bool* src, size_t srcW, size_t srcH, uint16_t color, PixmapRect source,
                    PixmapRect destination, BlitMode mode = BlitMode::STRETCH,
                    ScaleMode filter = ScaleMode::NEAREST, float alpha = 1.0f)
    {
        if (src && src != mask)
            drawRegion(srcW, srcH, source, destination, mode, filter, alpha,
                       [&](size_t i) { return Sample{color, uint8_t(pgm_read_byte(src + i) ? 255 : 0)}; });
    }

    // Rotate and scale in a single draw using a pivot relative to the source region.
    template <size_t W2, size_t H2, bool SOURCE_COVERAGE>
    void blitTransformed(const Pixmap<W2, H2, SOURCE_COVERAGE>& src, PixmapRect source, const BlitTransform& transform,
                         ScaleMode filter = ScaleMode::NEAREST, float alpha = 1.0f)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&src))
            drawTransformed(W2, H2, source, transform, filter, alpha,
                            [&](size_t i) { return samplePixmap(src, i); });
    }

    void blitTransformed(const uint16_t* src, size_t srcW, size_t srcH, PixmapRect source,
                         const BlitTransform& transform, ScaleMode filter = ScaleMode::NEAREST,
                         float alpha = 1.0f)
    {
        if (src && src != pixels)
            drawTransformed(srcW, srcH, source, transform, filter, alpha,
                            [&](size_t i) { return Sample{pgm_read_word(src + i), 255}; });
    }

    void blitTransformed(const bool* src, size_t srcW, size_t srcH, uint16_t color, PixmapRect source,
                         const BlitTransform& transform, ScaleMode filter = ScaleMode::NEAREST,
                         float alpha = 1.0f)
    {
        if (src && src != mask)
            drawTransformed(srcW, srcH, source, transform, filter, alpha,
                            [&](size_t i) { return Sample{color, uint8_t(pgm_read_byte(src + i) ? 255 : 0)}; });
    }

private:
    struct Sample { uint16_t color; uint8_t coverage; };

    template <typename Source>
    static Sample samplePixmap(const Source& src, size_t i)
    {
        const auto* alpha = src.getCoverage();
        return {src.getPixels()[i], alpha ? alpha[i] : uint8_t(src.getMask()[i] ? 255 : 0)};
    }

    struct WeightedPixel
    {
        float red = 0, green = 0, blue = 0, coverage = 0;

        void add(Sample sample, float weight)
        {
            if (!sample.coverage || weight <= 0.0f)
                return;
            if (sample.coverage != 255)
                weight *= sample.coverage / 255.0f;
            red += ((sample.color >> 11) & 31) * weight;
            green += ((sample.color >> 5) & 63) * weight;
            blue += (sample.color & 31) * weight;
            coverage += weight;
        }

        uint16_t color() const
        {
            return (uint16_t(red / coverage + 0.5f) << 11) |
                   (uint16_t(green / coverage + 0.5f) << 5) | uint16_t(blue / coverage + 0.5f);
        }
    };

    static bool validRegion(size_t srcW, size_t srcH, const PixmapRect& region)
    {
        return srcW && srcH && srcW <= INT_MAX && srcH <= INT_MAX &&
               srcW <= SIZE_MAX / srcH && region.x >= 0 && region.y >= 0 &&
               region.width > 0 && region.height > 0 &&
               int64_t(region.x) + region.width <= int64_t(srcW) &&
               int64_t(region.y) + region.height <= int64_t(srcH);
    }

    template <typename Fetch>
    void drawRegion(size_t srcW, size_t srcH, PixmapRect source, PixmapRect destination,
                    BlitMode mode, ScaleMode filter, float alpha, Fetch fetch)
    {
        if (!validRegion(srcW, srcH, source) || destination.width <= 0 || destination.height <= 0)
            return;
        const size_t start = size_t(source.y) * srcW + source.x;
        blit(source.width, source.height, mode, filter, alpha, destination.x, destination.y,
             destination.width, destination.height, fetch, srcW, start);
    }

    template <typename At>
    static WeightedPixel sampleBilinear(float u, float v, int width, int height, At at)
    {
        // Source coordinates are measured from the image edge, not the first center.
        const float sx = max(0.0f, min(float(width - 1), u - 0.5f));
        const float sy = max(0.0f, min(float(height - 1), v - 0.5f));
        const int lx = int(sx), ly = int(sy);
        const int rx = min(lx + 1, width - 1), ry = min(ly + 1, height - 1);
        const float fx = sx - lx, fy = sy - ly;
        WeightedPixel pixel;
        pixel.add(at(lx, ly), (1 - fx) * (1 - fy));
        pixel.add(at(rx, ly), fx * (1 - fy));
        pixel.add(at(lx, ry), (1 - fx) * fy);
        pixel.add(at(rx, ry), fx * fy);
        return pixel;
    }

    static int clampEdge(float position, size_t extent)
    {
        return position <= 0.0f ? 0 : double(position) >= double(extent)
            ? int(extent) : int(position);
    }

    template <typename Fetch>
    void drawTransformed(size_t srcW, size_t srcH, PixmapRect source,
                         const BlitTransform& transform, ScaleMode filter, float alpha, Fetch fetch)
    {
        const auto clip = getClipRect();
        if (!validRegion(srcW, srcH, source) || !(alpha > 0) ||
            !isfinite(transform.x) || !isfinite(transform.y) || !isfinite(transform.angle) ||
            !isfinite(transform.pivotX) || !isfinite(transform.pivotY) ||
            !isfinite(transform.scaleX) || !isfinite(transform.scaleY) ||
            transform.scaleX == 0.0f || transform.scaleY == 0.0f)
            return;

        const float radians = fmodf(transform.angle, 360.0f) * DEG_TO_RAD;
        float sine = sinf(radians), cosine = cosf(radians);
        // Keep exact quarter-turns aligned with the pixel grid.
        if (fmodf(transform.angle, 90.0f) == 0.0f)
        {
            sine = roundf(sine);
            cosine = roundf(cosine);
        }
        const float dux = cosine / transform.scaleX, duy = sine / transform.scaleX;
        const float dvx = -sine / transform.scaleY, dvy = cosine / transform.scaleY;
        if (!isfinite(dux) || !isfinite(duy) || !isfinite(dvx) || !isfinite(dvy))
            return;

        float left = INFINITY, top = INFINITY, right = -INFINITY, bottom = -INFINITY;
        const int edgesX[] = {0, source.width};
        const int edgesY[] = {0, source.height};
        for (int y : edgesY)
        {
            const float localY = (y - transform.pivotY) * transform.scaleY;
            for (int x : edgesX)
            {
                const float localX = (x - transform.pivotX) * transform.scaleX;
                const float tx = transform.x + localX * cosine - localY * sine;
                const float ty = transform.y + localX * sine + localY * cosine;
                if (!isfinite(tx) || !isfinite(ty))
                    return;
                left = min(left, tx);
                right = max(right, tx);
                top = min(top, ty);
                bottom = max(bottom, ty);
            }
        }

        // Integer-aligned, unrotated transforms can use the faster rectangle blitter.
        const float drawW = right - left, drawH = bottom - top;
        if (sine == 0.0f && cosine == 1.0f && transform.scaleX > 0 && transform.scaleY > 0 &&
            double(left) >= INT_MIN && double(left) <= INT_MAX &&
            double(top) >= INT_MIN && double(top) <= INT_MAX &&
            drawW > 0 && double(drawW) <= INT_MAX && drawH > 0 && double(drawH) <= INT_MAX &&
            left == floorf(left) && top == floorf(top) &&
            drawW == floorf(drawW) && drawH == floorf(drawH))
        {
            drawRegion(srcW, srcH, source, {int(left), int(top), int(drawW), int(drawH)},
                       BlitMode::STRETCH, filter, alpha, fetch);
            return;
        }

        alpha = min(1.0f, alpha);
        const int firstX = max(clip.x, clampEdge(floorf(left), WIDTH));
        const int lastX = min(clip.x + clip.width, clampEdge(ceilf(right), WIDTH));
        const int firstY = max(clip.y, clampEdge(floorf(top), HEIGHT));
        const int lastY = min(clip.y + clip.height, clampEdge(ceilf(bottom), HEIGHT));
        const size_t start = size_t(source.y) * srcW + source.x;
        const auto at = [&](int x, int y) { return fetch(start + size_t(y) * srcW + x); };
        const auto inside = [&](float u, float v) {
            return u >= 0.0f && v >= 0.0f && double(u) < source.width && double(v) < source.height;
        };
        const float xOffset = firstX + 0.5f - transform.x;
        for (int y = firstY; y < lastY; ++y)
        {
            const float yOffset = y + 0.5f - transform.y;
            const float u0 = transform.pivotX + xOffset * dux + yOffset * duy;
            const float v0 = transform.pivotY + xOffset * dvx + yOffset * dvy;
            for (int x = firstX; x < lastX; ++x)
            {
                const float u = u0 + (x - firstX) * dux;
                const float v = v0 + (x - firstX) * dvx;
                if (filter == ScaleMode::NEAREST)
                {
                    if (inside(u, v))
                    {
                        const auto sample = at(int(u), int(v));
                        if (sample.coverage)
                            setPixelValue(x, y, sample.color, alpha * (sample.coverage / 255.0f));
                    }
                    continue;
                }

                WeightedPixel pixel;
                if (filter == ScaleMode::BILINEAR && inside(u, v))
                    pixel = sampleBilinear(u, v, source.width, source.height, at);
                else if (filter == ScaleMode::AREA)
                {
                    // Approximate the rotated pixel footprint with evenly spaced samples.
                    for (int sy = 0; sy < 4; ++sy)
                    {
                        const float dy = (sy - 1.5f) * 0.25f;
                        for (int sx = 0; sx < 4; ++sx)
                        {
                            const float dx = (sx - 1.5f) * 0.25f;
                            const float su = u + dx * dux + dy * duy;
                            const float sv = v + dx * dvx + dy * dvy;
                            if (inside(su, sv))
                                pixel.add(at(int(su), int(sv)), 1.0f / 16.0f);
                        }
                    }
                }
                if (pixel.coverage > 0.0f)
                    setPixelValue(x, y, pixel.color(), alpha * min(1.0f, pixel.coverage));
            }
        }
    }

    template <typename Fetch>
    void blit(size_t srcW, size_t srcH, BlitMode mode, ScaleMode scaleMode,
              float alpha, int destX, int destY, int destW, int destH, Fetch fetch,
              size_t stride = 0, size_t sourceOffset = 0)
    {
        const auto clip = getClipRect();
        if (!srcW || !srcH || srcW > INT_MAX || srcH > INT_MAX ||
            srcW > SIZE_MAX / srcH || !(alpha > 0))
            return;

        const int64_t boxW = destW < 0 ? int64_t(WIDTH) - destX : destW;
        const int64_t boxH = destH < 0 ? int64_t(HEIGHT) - destY : destH;
        if (boxW <= 0 || boxH <= 0)
            return;
        alpha = min(1.0f, alpha);
        const int x0 = max(clip.x, destX), y0 = max(clip.y, destY);
        const int x1 = int(min(int64_t(clip.x + clip.width), destX + boxW));
        const int y1 = int(min(int64_t(clip.y + clip.height), destY + boxH));
        if (x0 >= x1 || y0 >= y1)
            return;

        float drawW = srcW, drawH = srcH;
        float left = destX, top = destY;
        if (mode == BlitMode::STRETCH)
        {
            drawW = boxW;
            drawH = boxH;
        }
        else if (mode == BlitMode::FIT || mode == BlitMode::FILL)
        {
            const float sx = float(boxW) / srcW, sy = float(boxH) / srcH;
            const float scale = mode == BlitMode::FIT ? min(sx, sy) : max(sx, sy);
            drawW *= scale;
            drawH *= scale;
        }

        if (mode == BlitMode::CENTER || mode == BlitMode::FIT || mode == BlitMode::FILL)
        {
            left += (boxW - drawW) * 0.5f;
            top += (boxH - drawH) * 0.5f;
        }
        if (mode == BlitMode::TOP_RIGHT || mode == BlitMode::BOTTOM_RIGHT)
            left += boxW - drawW;
        if (mode == BlitMode::BOTTOM_LEFT || mode == BlitMode::BOTTOM_RIGHT)
            top += boxH - drawH;

        const float scaleX = srcW / drawW, scaleY = srcH / drawH;
        if (!stride)
            stride = srcW;
        const auto at = [&](int sx, int sy) {
            return fetch(sourceOffset + size_t(sy) * stride + sx);
        };
        if (scaleMode == ScaleMode::NEAREST)
        {
            // Common native-size and 2x blits can map coordinates with integer shifts.
            const bool integralOrigin = left == floorf(left) && top == floorf(top);
            if (integralOrigin && (scaleX == 1.0f || scaleX == 0.5f) &&
                (scaleY == 1.0f || scaleY == 0.5f))
            {
                const int firstX = max(x0, lowerBound(int64_t(left), WIDTH));
                const int firstY = max(y0, lowerBound(int64_t(top), HEIGHT));
                const int lastX = min(x1, lowerBound(int64_t(left) + int64_t(drawW), WIDTH));
                const int lastY = min(y1, lowerBound(int64_t(top) + int64_t(drawH), HEIGHT));
                const unsigned shiftX = scaleX == 0.5f, shiftY = scaleY == 0.5f;
                for (int y = firstY; y < lastY; ++y)
                {
                    const size_t srcRow = sourceOffset +
                        size_t((int64_t(y) - int64_t(top)) >> shiftY) * stride;
                    size_t dst = size_t(y) * WIDTH + firstX;
                    size_t srcX = size_t(int64_t(firstX) - int64_t(left));
                    for (int x = firstX; x < lastX; ++x, ++dst, ++srcX)
                    {
                        const auto sample = fetch(srcRow + (srcX >> shiftX));
                        if (sample.coverage)
                            composite(dst, sample.color, alpha * (sample.coverage / 255.0f));
                    }
                }
                return;
            }

            for (int y = y0; y < y1; ++y)
            {
                const float v = y + 0.5f - top;
                if (v < 0.0f || v >= drawH)
                    continue;
                const int sy = int(min(float(srcH - 1), v * scaleY));
                for (int x = x0; x < x1; ++x)
                {
                    const float u = x + 0.5f - left;
                    if (u < 0.0f || u >= drawW)
                        continue;
                    const auto sample = at(int(min(float(srcW - 1), u * scaleX)), sy);
                    if (sample.coverage)
                        setPixelValue(x, y, sample.color, alpha * (sample.coverage / 255.0f));
                }
            }
            return;
        }

        for (int y = y0; y < y1; ++y)
        {
            const float v = y + 0.5f - top;
            if (v < 0.0f || v >= drawH)
                continue;
            for (int x = x0; x < x1; ++x)
            {
                const float u = x + 0.5f - left;
                if (u < 0.0f || u >= drawW)
                    continue;

                WeightedPixel pixel;
                if (scaleMode == ScaleMode::BILINEAR)
                {
                    pixel = sampleBilinear(u * scaleX, v * scaleY, int(srcW), int(srcH), at);
                }
                else if (scaleMode == ScaleMode::AREA)
                {
                    const float lx = max(0.0f, (u - 0.5f) * scaleX);
                    const float rx = min(float(srcW), (u + 0.5f) * scaleX);
                    const float ly = max(0.0f, (v - 0.5f) * scaleY);
                    const float ry = min(float(srcH), (v + 0.5f) * scaleY);
                    const float area = (rx - lx) * (ry - ly);
                    if (area <= 0.0f)
                        continue;
                    const int endY = min(int(srcH), int(ceilf(ry)));
                    const int endX = min(int(srcW), int(ceilf(rx)));
                    for (int sy = int(ly); sy < endY; ++sy)
                    {
                        const float dy = min(ry, float(sy + 1)) - max(ly, float(sy));
                        for (int sx = int(lx); sx < endX; ++sx)
                        {
                            const float dx = min(rx, float(sx + 1)) - max(lx, float(sx));
                            pixel.add(at(sx, sy), dx * dy / area);
                        }
                    }
                }
                if (pixel.coverage > 0.0f)
                    setPixelValue(x, y, pixel.color(), alpha * min(1.0f, pixel.coverage));
            }
        }
    }

public:
    void print(int x,
               int y,
               const char* str,
               const BitmapFont& font,
               uint32_t color,
               int maxGlyphHeight = -1,
               float spacingPercent = 0.0f,
               BitmapFilter filter = BitmapFilter::PRESERVE_STROKES)
    {
        const auto clip = getClipRect();
        if (!str || font.getHeight() <= 0)
            return;
        const int nativeHeight = font.getHeight();
        const int height = maxGlyphHeight > 0 ? maxGlyphHeight : nativeHeight;
        const int firstRow = lowerBound(int64_t(clip.y) - y, height);
        const int lastRow = lowerBound(int64_t(clip.y) + clip.height - y, height);
        if (firstRow >= lastRow)
            return;

        const double spacing = round(double(height) * spacingPercent);
        if (!(spacing >= INT_MIN && spacing <= INT_MAX))
            return;

        int64_t cursor = x;
        while (*str)
        {
            const char c = *str++;
            const Glyph glyph = font.lookupChar(c);
            if (!glyph.data || glyph.width <= 0 ||
                size_t(glyph.width) > SIZE_MAX / size_t(nativeHeight))
            {
                if (c == ' ')
                    cursor += int64_t(height / 3) + int(spacing);
                continue;
            }

            const int64_t scaledWidth = (int64_t(glyph.width) * height + nativeHeight / 2) / nativeHeight;
            const int width = int(min(int64_t(INT_MAX), max(int64_t(1), scaledWidth)));
            const int firstColumn = lowerBound(int64_t(clip.x) - cursor, width);
            const int lastColumn = lowerBound(int64_t(clip.x) + clip.width - cursor, width);
            for (int row = firstRow; row < lastRow; ++row)
            {
                for (int col = firstColumn; col < lastColumn; ++col)
                {
                    if (sampleBitmapGlyph(glyph, nativeHeight, width, height, col, row, filter))
                        setPixelValue(int(cursor + col), y + row, uint16_t(color));
                }
            }
            cursor += int64_t(width) + int(spacing);
        }
    }
};
