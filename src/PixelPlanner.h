#pragma once
#include <Arduino.h>
#include "Colors.h"

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
template <size_t WIDTH, size_t HEIGHT>
class Pixmap
{
private:
    uint16_t pixels[WIDTH*HEIGHT] = {};     ///< Pixel buffer (RGB565)
    bool     mask[WIDTH*HEIGHT]   = {};     ///< Mask indicating written pixels (false = not yet written)

    /**
     * @brief Convert 2D coordinates to linear index (caller must ensure in-bounds).
     */
    constexpr size_t index(int x, int y) const { return (size_t)(y * (int)WIDTH + x); }

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
    }

    /**
     * @brief Check if coordinates are inside bounds.
     */
    bool inBounds(int x, int y) const { return ((x >= 0) && (y >= 0) && (x < (int)WIDTH) && (y < (int)HEIGHT)); }

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
        if(inBounds(x, y) && alpha > 0)
        {
            pixels[index(x, y)] = blend565(pixels[index(x, y)],
                                           value,
                                           alpha);
            mask[index(x, y)] = true;
        }
    }

    /**
     * @brief Get pixel color.
     */
    uint16_t getPixelValue(int x, int y) const { return inBounds(x, y) ? pixels[index(x, y)] : 0; }

    /**
     * @brief Get mask value.
     */
    bool getMaskValue(int x, int y) const { return inBounds(x, y) ? mask[index(x, y)] : false; }

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
        float dx_f = x2 - x1;
        float dy_f = y2 - y1;

        if ((dx_f == 0) && (dy_f == 0))
            return;

        if (border_to_border)
        {
            float t_min = -1e9f;
            float t_max =  1e9f;

            if (dx_f != 0)
            {
                float tx1 = (0 - x1)            / dx_f;
                float tx2 = (width() - 1 - x1)  / dx_f;

                t_min = max(t_min, min(tx1, tx2));
                t_max = min(t_max, max(tx1, tx2));
            }

            if (dy_f != 0)
            {
                float ty1 = (0 - y1)            / dy_f;
                float ty2 = (height() - 1 - y1) / dy_f;

                t_min = max(t_min, min(ty1, ty2));
                t_max = min(t_max, max(ty1, ty2));
            }

            x1 += t_min * dx_f;
            y1 += t_min * dy_f;
            x2  = x1 + (t_max - t_min) * dx_f;
            y2  = y1 + (t_max - t_min) * dy_f;

            // Recompute direction after clipping
            dx_f = x2 - x1;
            dy_f = y2 - y1;
        }

        float len = sqrtf((dx_f * dx_f) + (dy_f * dy_f));

        if (len == 0)
            return;

        // Rasterize the thick line as a filled quad (two triangles).
        // The old Bresenham + integer perpendicular-offset approach fails for
        // diagonal lines: the fractional perpendicular vector causes adjacent
        // integer t-steps to map to the same pixel (gaps) or the same pixel
        // twice (multi-line artifact). Filling the exact bounding quad with
        // fill_triangle covers every pixel exactly once at the true thickness.
        float half = max(0.5f, thickness * 0.5f);
        float px   = -(dy_f / len) * half;
        float py   =  (dx_f / len) * half;

        float vx0 = x1 + px;   float vy0 = y1 + py;
        float vx1 = x1 - px;   float vy1 = y1 - py;
        float vx2 = x2 + px;   float vy2 = y2 + py;
        float vx3 = x2 - px;   float vy3 = y2 - py;

        fill_triangle((int)roundf(vx0), (int)roundf(vy0),
                      (int)roundf(vx1), (int)roundf(vy1),
                      (int)roundf(vx2), (int)roundf(vy2),
                      color, alpha);

        fill_triangle((int)roundf(vx1), (int)roundf(vy1),
                      (int)roundf(vx3), (int)roundf(vy3),
                      (int)roundf(vx2), (int)roundf(vy2),
                      color, alpha);
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
        float x1;
        float y1;
        float x2;
        float y2;

        if (isinf(slope))
        {
            if (border_to_border)
            {
                x1 = x;
                y1 = 0;
                x2 = x;
                y2 = height()-1;
            }
            else
            {
                float half = line_distance / 2.0f;

                x1 = x;
                y1 = y - half;
                x2 = x;
                y2 = y + half;
            }
        }
        else
        {
            float dx  = 1.0f;
            float dy  = slope;
            float len = sqrtf(dx * dx + dy * dy);

            dx /= len;
            dy /= len;

            if (border_to_border)
            {
                float t_min = -1e9;
                float t_max =  1e9;

                if (dx != 0)
                {
                    float tx1 = (0 - x)           / dx;
                    float tx2 = (width() - 1 - x) / dx;

                    t_min = max(t_min, min(tx1, tx2));
                    t_max = min(t_max, max(tx1, tx2));
                }

                if (dy != 0)
                {
                    float ty1 = (0 - y)            / dy;
                    float ty2 = (height() - 1 - y) / dy;

                    t_min = max(t_min, min(ty1, ty2));
                    t_max = min(t_max, max(ty1, ty2));
                }

                x1 = x + t_min * dx;
                y1 = y + t_min * dy;
                x2 = x + t_max * dx;
                y2 = y + t_max * dy;
            }
            else
            {
                float half = line_distance / 2.0f;

                x1 = x - dx * half;
                y1 = y - dy * half;
                x2 = x + dx * half;
                y2 = y + dy * half;
            }
        }

        draw_line_from_points(x1,
                              y1,
                              x2,
                              y2,
                              thickness,
                              false,
                              color,
                              alpha);
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
                     float    alpha = 1.0f)
    {
        if (radius <= 0)
            return;

        int r_outer = radius;
        int r_inner = solid ? 0 : max(0, radius - max(1, thickness));

        for (int y = -r_outer; y <= r_outer; y++)
        {
            // Use sqrtf + roundf to avoid integer truncation leaving gaps near
            // the top and bottom of the circle where the span changes rapidly.
            int outer_x = (int)roundf(sqrtf((float)((r_outer * r_outer) - (y * y))));
            int inner_x = (r_inner > 0) ? (int)roundf(sqrtf((float)((r_inner * r_inner) - (y * y)))) : 0;

            for (int x = -outer_x; x <= -inner_x; x++)
                setPixelValue(cx + x,
                              cy + y,
                              color,
                              alpha);

            for (int x = inner_x; x <= outer_x; x++)
                setPixelValue(cx + x,
                              cy + y,
                              color,
                              alpha);
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
        if (w <= 0 || h <= 0)
            return;

        if (solid)
        {
            for (int j = 0; j < h; j++)
                for (int i = 0; i < w; i++)
                    setPixelValue(x + i,
                                  y + j,
                                  color,
                                  alpha);
            return;
        }

        // Outline path: clamp thickness to [1, half the smaller dimension]
        int max_thick = min(w / 2, h / 2);
        thickness     = max(1, min(thickness, max_thick));

        for(int t = 0; t < thickness; t++)
        {
            for(int i = 0; i < w; i++)
            {
                setPixelValue(x + i,
                              y + t,
                              color,
                              alpha);
                setPixelValue(x + i,
                              y + h - 1 - t,
                              color,
                              alpha);
            }
        }
        for(int t = 0; t < thickness; t++)
        {
            for(int j = 0; j < h; j++)
            {
                setPixelValue(x + t,
                              y + j,
                              color,
                              alpha);
                setPixelValue(x + w - 1 - t,
                              y + j,
                              color,
                              alpha);
            }
        }
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
        int minX = min(x0, min(x1, x2));
        int maxX = max(x0, max(x1, x2));
        int minY = min(y0, min(y1, y2));
        int maxY = max(y0, max(y1, y2));

        for (int y = minY; y <= maxY; y++)
            for (int x = minX; x <= maxX; x++)
            {
                int w0 = ((x1 - x0) * (y - y0)) - ((y1 - y0) * (x - x0));
                int w1 = ((x2 - x1) * (y - y1)) - ((y2 - y1) * (x - x1));
                int w2 = ((x0 - x2) * (y - y2)) - ((y0 - y2) * (x - x2));

                if (((w0 >= 0) && (w1 >= 0) && (w2 >= 0)) || ((w0 <= 0) && (w1 <= 0) && (w2 <= 0)))
                    setPixelValue(x,
                                  y,
                                  color,
                                  alpha);
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
        float x0 = x;
        float y0 = y;
        float x1 = x + w;
        float y1 = y;
        float x2 = x + w;
        float y2 = y + h;
        float x3 = x;
        float y3 = y + h;

        float rx0;
        float ry0;
        float rx1;
        float ry1;
        float rx2;
        float ry2;
        float rx3;
        float ry3;

        rotate_point(x0, y0, pivot_x, pivot_y, angle_deg, rx0, ry0);
        rotate_point(x1, y1, pivot_x, pivot_y, angle_deg, rx1, ry1);
        rotate_point(x2, y2, pivot_x, pivot_y, angle_deg, rx2, ry2);
        rotate_point(x3, y3, pivot_x, pivot_y, angle_deg, rx3, ry3);

        if (solid)
        {
            fill_triangle((int)(rx0 + 0.5f),
                          (int)(ry0 + 0.5f),
                          (int)(rx1 + 0.5f),
                          (int)(ry1 + 0.5f),
                          (int)(rx2 + 0.5f),
                          (int)(ry2 + 0.5f),
                          color,
                          alpha);

            fill_triangle((int)(rx0 + 0.5f),
                          (int)(ry0 + 0.5f),
                          (int)(rx2 + 0.5f),
                          (int)(ry2 + 0.5f),
                          (int)(rx3 + 0.5f),
                          (int)(ry3 + 0.5f),
                          color,
                          alpha);
        }
        else
        {
            auto draw_edge = [&](float xs,
                                 float ys,
                                 float xe,
                                 float ye)
            {
                float dx  = xe - xs;
                float dy  = ye - ys;
                float len = sqrtf((dx * dx) + (dy * dy));

                if (len == 0)
                    return;

                float px = -(dy / len) * (thickness / 2.0f);
                float py =  (dx / len) * (thickness / 2.0f);

                float vx0 = xs + px;
                float vy0 = ys + py;
                float vx1 = xs - px;
                float vy1 = ys - py;
                float vx2 = xe + px;
                float vy2 = ye + py;
                float vx3 = xe - px;
                float vy3 = ye - py;

                fill_triangle((int)roundf(vx0),
                              (int)roundf(vy0),
                              (int)roundf(vx1),
                              (int)roundf(vy1),
                              (int)roundf(vx2),
                              (int)roundf(vy2),
                              color,
                              alpha);

                fill_triangle((int)roundf(vx1),
                              (int)roundf(vy1),
                              (int)roundf(vx3),
                              (int)roundf(vy3),
                              (int)roundf(vx2),
                              (int)roundf(vy2),
                              color,
                              alpha);
            };

            draw_edge(rx0, ry0, rx1, ry1);
            draw_edge(rx1, ry1, rx2, ry2);
            draw_edge(rx2, ry2, rx3, ry3);
            draw_edge(rx3, ry3, rx0, ry0);
        }
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
                  float    alpha = 1.0f)
    {
        if (radius <= 0)
            return;

        // Allow end_deg < start_deg by wrapping forward
        while (end_deg < start_deg)
            end_deg += 360.0f;

        float span    = end_deg - start_deg;
        int   r_outer = radius;
        int   r_inner = solid ? 0 : max(0, radius - max(1, thickness));

        for (int y = -r_outer; y <= r_outer; y++)
        {
            for (int x = -r_outer; x <= r_outer; x++)
            {
                int r2 = (x * x) + (y * y);

                if (r2 > (r_outer * r_outer))
                    continue;

                if (!solid && (r2 < (r_inner * r_inner)))
                    continue;

                // Sample four sub-pixel corners and accept the pixel if any
                // corner falls within the arc. This prevents gaps on thin arcs
                // where a single centre-point atan2 test misses boundary pixels.
                bool inside = false;

                for (int sy = 0; (sy < 2) && !inside; sy++)
                {
                    for (int sx = 0; (sx < 2) && !inside; sx++)
                    {
                        float fx = (float)x + (sx ? 0.5f : -0.5f);
                        float fy = (float)y + (sy ? 0.5f : -0.5f);

                        float angle = atan2f(fy, fx) * (180.0f / (float)M_PI);

                        while (angle <  start_deg)          angle += 360.0f;
                        while (angle >= start_deg + 360.0f) angle -= 360.0f;

                        if (angle <= start_deg + span)
                            inside = true;
                    }
                }

                if (inside)
                    setPixelValue(cx + x,
                                  cy + y,
                                  color,
                                  alpha);
            }
        }
    }

    /**
     * @brief Draw an open polyline through an ordered list of points.
     *
     * Draws connected line segments between consecutive vertices. The path
     * is not closed; use draw_polygon with solid=false to close the shape.
     *
     * @param points    Flat array of vertex coordinates [x0,y0, x1,y1, ...].
     * @param count     Number of vertices (not the number of floats).
     * @param thickness Line thickness in pixels.
     * @param color     RGB565 color.
     * @param alpha     Blend factor [0.0–1.0].
     */
    void draw_polyline(const float* points,
                       size_t       count,
                       float        thickness,
                       uint16_t     color,
                       float        alpha = 1.0f)
    {
        if (!points || (count < 2))
            return;

        for (size_t i = 0; i < count - 1; i++)
            draw_line_from_points(points[(i * 2) + 0],
                                  points[(i * 2) + 1],
                                  points[(i * 2) + 2],
                                  points[(i * 2) + 3],
                                  thickness,
                                  false,
                                  color,
                                  alpha);
    }

    /**
     * @brief Draw a polygon (filled or outlined) through an ordered list of vertices.
     *
     * Filled mode uses a scanline even-odd rasterizer and correctly handles both
     * convex and simple concave polygons. Outlined mode draws each edge as a
     * thick line and closes the last vertex back to the first.
     *
     * The intersection scratch buffer is capped at 64 entries per scanline, which
     * is sufficient for typical use. Increase @c MAX_INTERSECTIONS for highly
     * complex shapes.
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
                      float        alpha = 1.0f)
    {
        if (!points || (count < 3))
            return;

        if (!solid)
        {
            for (size_t i = 0; i < count; i++)
            {
                size_t j = (i + 1) % count;

                draw_line_from_points(points[(i * 2) + 0],
                                      points[(i * 2) + 1],
                                      points[(j * 2) + 0],
                                      points[(j * 2) + 1],
                                      thickness,
                                      false,
                                      color,
                                      alpha);
            }
            return;
        }

        // Compute vertical extents
        float minY =  1e9f;
        float maxY = -1e9f;

        for (size_t i = 0; i < count; i++)
        {
            float fy = points[(i * 2) + 1];

            if (fy < minY) minY = fy;
            if (fy > maxY) maxY = fy;
        }

        int scanY0 = max(0,                (int)floorf(minY));
        int scanY1 = min((int)height() - 1, (int)ceilf(maxY));

        static const size_t MAX_INTERSECTIONS = 64;

        float xs[MAX_INTERSECTIONS];

        for (int scanY = scanY0; scanY <= scanY1; scanY++)
        {
            size_t hits = 0;
            float  fy   = (float)scanY + 0.5f; // sample at pixel centre

            for (size_t i = 0; (i < count) && (hits < MAX_INTERSECTIONS); i++)
            {
                size_t j  = (i + 1) % count;
                float  ay = points[(i * 2) + 1];
                float  by = points[(j * 2) + 1];

                if (((ay <= fy) && (by > fy)) || ((by <= fy) && (ay > fy)))
                {
                    float ax = points[(i * 2) + 0];
                    float bx = points[(j * 2) + 0];
                    float t  = (fy - ay) / (by - ay);

                    xs[hits++] = ax + (t * (bx - ax));
                }
            }

            // Insertion sort — N is small
            for (size_t a = 1; a < hits; a++)
            {
                float key = xs[a];
                int   b   = (int)a - 1;

                while ((b >= 0) && (xs[b] > key))
                {
                    xs[b + 1] = xs[b];
                    b--;
                }

                xs[b + 1] = key;
            }

            // Fill between each pair of intersections
            for (size_t a = 0; (a + 1) < hits; a += 2)
            {
                int xLeft  = max(0,                (int)ceilf (xs[a]));
                int xRight = min((int)width() - 1, (int)floorf(xs[a + 1]));

                for (int px = xLeft; px <= xRight; px++)
                    setPixelValue(px,
                                  scanY,
                                  color,
                                  alpha);
            }
        }
    }

    /**
     * @brief Flood fill a contiguous region starting from a seed pixel.
     *
     * Replaces all pixels connected to the seed that share its color with
     * @p fill_color. Uses an iterative scanline algorithm to avoid deep
     * recursion on embedded targets.
     *
     * Stack memory is allocated on the heap (@c new / @c delete). If your
     * environment uses a custom allocator or has no heap, replace the
     * allocation with a statically sized array sized to WIDTH * HEIGHT.
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
        if (!inBounds(x, y))
            return;

        uint16_t target = getPixelValue(x, y);

        if (target == fill_color)
            return;

        struct Point { int16_t x; int16_t y; };

        const int max_stack = (int)(WIDTH * HEIGHT);
        Point*    stack     = new Point[max_stack];

        if (!stack)
            return;

        int top      = 0;
        stack[top++] = { (int16_t)x, (int16_t)y };

        while (top > 0)
        {
            Point p  = stack[--top];
            int   cx = p.x;
            int   cy = p.y;

            if (!inBounds(cx, cy))
                continue;

            if (getPixelValue(cx, cy) != target)
                continue;

            // Extend to the leftmost and rightmost matching pixel on this scanline
            int left  = cx;
            int right = cx;

            while ((left  > 0)               && (getPixelValue(left  - 1, cy) == target)) left--;
            while ((right < (int)width() - 1) && (getPixelValue(right + 1, cy) == target)) right++;

            for (int px = left; px <= right; px++)
            {
                setPixelValue(px, cy, fill_color, alpha);

                if ((cy > 0)                && (getPixelValue(px, cy - 1) == target) && (top < max_stack))
                    stack[top++] = { (int16_t)px, (int16_t)(cy - 1) };

                if ((cy < (int)height() - 1) && (getPixelValue(px, cy + 1) == target) && (top < max_stack))
                    stack[top++] = { (int16_t)px, (int16_t)(cy + 1) };
            }
        }

        delete[] stack;
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
    template <size_t W2, size_t H2>
    void blit_from_pixmap(const Pixmap<W2, H2>& src,
                          BlitMode              mode      = BlitMode::TOP_LEFT,
                          ScaleMode             scaleMode = ScaleMode::NEAREST,
                          float                 alpha     = 1.0f)
    {
        const uint16_t* src_pixels = src.getPixels();
        const bool*     src_mask   = src.getMask();

        // Determine destination drawing area and offsets (mirrors blitFromArray logic)
        float drawW   = (float)W2;
        float drawH   = (float)H2;
        float offsetX = 0.0f;
        float offsetY = 0.0f;

        if (mode == BlitMode::STRETCH)
        {
            drawW = (float)width();
            drawH = (float)height();
        }
        else if ((mode == BlitMode::FIT) || (mode == BlitMode::FILL))
        {
            float sx = (float)width()  / W2;
            float sy = (float)height() / H2;
            float s  = (mode == BlitMode::FIT) ? min(sx, sy) : max(sx, sy);

            drawW = W2 * s;
            drawH = H2 * s;

            offsetX = (width()  - drawW) * 0.5f;
            offsetY = (height() - drawH) * 0.5f;
        }
        else if (mode == BlitMode::CENTER)
        {
            offsetX = (width()  - W2) * 0.5f;
            offsetY = (height() - H2) * 0.5f;
        }
        else if (mode == BlitMode::TOP_RIGHT)
        {
            offsetX = width() - W2;
            offsetY = 0;
        }
        else if (mode == BlitMode::BOTTOM_LEFT)
        {
            offsetX = 0;
            offsetY = height() - H2;
        }
        else if (mode == BlitMode::BOTTOM_RIGHT)
        {
            offsetX = width() - W2;
            offsetY = height() - H2;
        }
        // TOP_LEFT: offsetX = offsetY = 0

        float scaleX = (drawW > 1) ? (W2 - 1) / (drawW - 1) : 0.0f;
        float scaleY = (drawH > 1) ? (H2 - 1) / (drawH - 1) : 0.0f;

        for (int y = 0; y < (int)height(); y++)
        {
            for (int x = 0; x < (int)width(); x++)
            {
                float srcXf = (x - offsetX) * scaleX;
                float srcYf = (y - offsetY) * scaleY;

                uint16_t c        = 0;
                bool     src_set  = false;

                if (scaleMode == ScaleMode::NEAREST)
                {
                    int xi = (int)roundf(srcXf);
                    int yi = (int)roundf(srcYf);

                    if ((xi >= 0) && (xi < (int)W2) && (yi >= 0) && (yi < (int)H2))
                    {
                        src_set = src_mask[(yi * W2) + xi];
                        c       = src_pixels[(yi * W2) + xi];
                    }
                }
                else if (scaleMode == ScaleMode::BILINEAR)
                {
                    int x0 = (int)floorf(srcXf);
                    int y0 = (int)floorf(srcYf);
                    int x1 = min(x0 + 1, (int)W2 - 1);
                    int y1 = min(y0 + 1, (int)H2 - 1);

                    if ((x0 >= 0) && (x0 < (int)W2) && (y0 >= 0) && (y0 < (int)H2))
                    {
                        float fx = srcXf - x0;
                        float fy = srcYf - y0;

                        // Treat the pixel as set if any of the four samples are masked
                        src_set = src_mask[(y0 * W2) + x0] ||
                                  src_mask[(y0 * W2) + x1] ||
                                  src_mask[(y1 * W2) + x0] ||
                                  src_mask[(y1 * W2) + x1];

                        uint16_t c00 = src_pixels[(y0 * W2) + x0];
                        uint16_t c10 = src_pixels[(y0 * W2) + x1];
                        uint16_t c01 = src_pixels[(y1 * W2) + x0];
                        uint16_t c11 = src_pixels[(y1 * W2) + x1];

                        auto lerp565 = [](uint16_t a, uint16_t b, float t) -> uint16_t
                        {
                            int ar = (a >> 11) & 0x1F;
                            int ag = (a >> 5)  & 0x3F;
                            int ab =  a        & 0x1F;
                            int br = (b >> 11) & 0x1F;
                            int bg = (b >> 5)  & 0x3F;
                            int bb =  b        & 0x1F;

                            return (uint16_t)(((ar + (int)((br - ar) * t)) << 11) |
                                             ((ag + (int)((bg - ag) * t)) << 5)  |
                                              (ab + (int)((bb - ab) * t)));
                        };

                        c = lerp565(lerp565(c00, c10, fx),
                                    lerp565(c01, c11, fx),
                                    fy);
                    }
                }
                else if (scaleMode == ScaleMode::AREA)
                {
                    float ax0 = srcXf;
                    float ay0 = srcYf;
                    float ax1 = srcXf + scaleX;
                    float ay1 = srcYf + scaleY;

                    float r     = 0.0f;
                    float g     = 0.0f;
                    float b     = 0.0f;
                    float total = 0.0f;

                    int ix0 = (int)floorf(ax0);
                    int iy0 = (int)floorf(ay0);
                    int ix1 = (int)ceilf(ax1);
                    int iy1 = (int)ceilf(ay1);

                    for (int sy = iy0; sy < iy1; sy++)
                    {
                        if ((sy < 0) || (sy >= (int)H2))
                            continue;

                        float wy = min(ay1, (float)(sy + 1)) - max(ay0, (float)sy);

                        if (wy <= 0.0f)
                            continue;

                        for (int sx = ix0; sx < ix1; sx++)
                        {
                            if ((sx < 0) || (sx >= (int)W2))
                                continue;

                            float wx = min(ax1, (float)(sx + 1)) - max(ax0, (float)sx);

                            if (wx <= 0.0f)
                                continue;

                            float    w   = wx * wy;
                            uint16_t pix = src_pixels[(sy * W2) + sx];

                            if (src_mask[(sy * W2) + sx])
                                src_set = true;

                            r     += ((pix >> 11) & 0x1F) * w;
                            g     += ((pix >> 5)  & 0x3F) * w;
                            b     +=  (pix        & 0x1F) * w;
                            total +=   w;
                        }
                    }

                    if (total > 0.0f)
                    {
                        r /= total;
                        g /= total;
                        b /= total;
                    }

                    c = ((int)r << 11) | ((int)g << 5) | (int)b;
                }

                if (src_set)
                    setPixelValue(x,
                                  y,
                                  c,
                                  alpha);
            }
        }
    }

    /**
     * @brief Blit raw pixel data into the Pixmap with scaling and placement modes.
     *
     * @tparam T        Source pixel type.
     * @param src       Source buffer.
     * @param srcW      Source width.
     * @param srcH      Source height.
     * @param mode      Placement mode.
     * @param scaleMode Scaling algorithm.
     * @param convert   Optional conversion function to RGB565.
     * @param alpha     Blend factor.
     */
    template <typename T>
    void blitFromArray(const T*  src,
                       size_t    srcW,
                       size_t    srcH,
                       BlitMode  mode,
                       ScaleMode scaleMode     = ScaleMode::BILINEAR,
                       uint16_t  (*convert)(T) = nullptr,
                       float     alpha         = 1.0f)
    {
        if (!src || (srcW == 0) || (srcH == 0))
            return;

        auto toColor = [&](T v) -> uint16_t
        {
            return convert ? convert(v) : (uint16_t)v;
        };

        // --- Determine drawing area and scaling ---
        float drawW = (float)srcW;
        float drawH = (float)srcH;

        float offsetX = 0.0f;
        float offsetY = 0.0f;

        if (mode == BlitMode::STRETCH)
        {
            drawW = (float)width();
            drawH = (float)height();
        }
        else if ((mode == BlitMode::FIT) || (mode == BlitMode::FILL))
        {
            float sx = (float)width()  / srcW;
            float sy = (float)height() / srcH;
            float s  = (mode == BlitMode::FIT) ? min(sx, sy) : max(sx, sy);

            drawW = srcW * s;
            drawH = srcH * s;

            offsetX = (width()  - drawW) * 0.5f;
            offsetY = (height() - drawH) * 0.5f;
        }
        else if (mode == BlitMode::CENTER)
        {
            offsetX = (width()  - srcW) * 0.5f;
            offsetY = (height() - srcH) * 0.5f;
        }
        else if (mode == BlitMode::TOP_RIGHT)
        {
            offsetX = width() - srcW;
            offsetY = 0;
        }
        else if (mode == BlitMode::BOTTOM_LEFT)
        {
            offsetX = 0;
            offsetY = height() - srcH;
        }
        else if (mode == BlitMode::BOTTOM_RIGHT)
        {
            offsetX = width() - srcW;
            offsetY = height() - srcH;
        }
        // TOP_LEFT: offsetX = offsetY = 0

        // --- Compute scale from dest → src ---
        float scaleX = (drawW > 1) ? (srcW - 1) / (drawW - 1) : 0;
        float scaleY = (drawH > 1) ? (srcH - 1) / (drawH - 1) : 0;

        // --- Main loop ---
        for (int y = 0; y < (int)height(); y++)
        {
            for (int x = 0; x < (int)width(); x++)
            {
                float srcXf = (x - offsetX) * scaleX;
                float srcYf = (y - offsetY) * scaleY;

                uint16_t c       = 0;
                bool     src_set = false;

                if (scaleMode == ScaleMode::NEAREST)
                {
                    int xi = (int)roundf(srcXf);
                    int yi = (int)roundf(srcYf);

                    if ((xi >= 0) && (xi < (int)srcW) && (yi >= 0) && (yi < (int)srcH))
                    {
                        c       = toColor(src[yi * srcW + xi]);
                        src_set = true;
                    }
                }
                else if (scaleMode == ScaleMode::BILINEAR)
                {
                    int x0 = (int)floorf(srcXf);
                    int y0 = (int)floorf(srcYf);
                    int x1 = min(x0 + 1, (int)srcW - 1);
                    int y1 = min(y0 + 1, (int)srcH - 1);

                    if ((x0 >= 0) && (x0 < (int)srcW) && (y0 >= 0) && (y0 < (int)srcH))
                    {
                        float fx = srcXf - x0;
                        float fy = srcYf - y0;

                        uint16_t c00 = toColor(src[y0 * srcW + x0]);
                        uint16_t c10 = toColor(src[y0 * srcW + x1]);
                        uint16_t c01 = toColor(src[y1 * srcW + x0]);
                        uint16_t c11 = toColor(src[y1 * srcW + x1]);

                        auto lerp565 = [](uint16_t a, uint16_t b, float t) -> uint16_t
                        {
                            int ar = (a >> 11) & 0x1F;
                            int ag = (a >> 5)  & 0x3F;
                            int ab =  a & 0x1F;

                            int br = (b >> 11) & 0x1F;
                            int bg = (b >> 5)  & 0x3F;
                            int bb =  b & 0x1F;

                            int rr = ar + (int)((br - ar) * t);
                            int rg = ag + (int)((bg - ag) * t);
                            int rb = ab + (int)((bb - ab) * t);

                            return (uint16_t)((rr << 11) | (rg << 5) | rb);
                        };

                        uint16_t cx0 = lerp565(c00, c10, fx);
                        uint16_t cx1 = lerp565(c01, c11, fx);

                        c       = lerp565(cx0, cx1, fy);
                        src_set = true;
                    }
                }
                else if (scaleMode == ScaleMode::AREA)
                {
                    float x0 = srcXf;
                    float y0 = srcYf;
                    float x1 = srcXf + scaleX;
                    float y1 = srcYf + scaleY;

                    float r     = 0;
                    float g     = 0;
                    float b     = 0;
                    float total = 0;

                    int ix0 = floorf(x0);
                    int iy0 = floorf(y0);
                    int ix1 = ceilf(x1);
                    int iy1 = ceilf(y1);

                    for (int sy = iy0; sy < iy1; sy++)
                    {
                        if ((sy < 0) || (sy >= (int)srcH))
                            continue;

                        float wy = min(y1, (float)(sy + 1)) - max(y0, (float)sy);

                        if (wy <= 0)
                            continue;

                        for (int sx = ix0; sx < ix1; sx++)
                        {
                            if ((sx < 0) || (sx >= (int)srcW))
                                continue;

                            float wx = min(x1, (float)(sx + 1)) - max(x0, (float)sx);

                            if (wx <= 0)
                                continue;

                            float    w   = wx * wy;
                            uint16_t pix = toColor(src[sy * srcW + sx]);

                            r     += ((pix >> 11) & 0x1F) * w;
                            g     += ((pix >> 5)  & 0x3F) * w;
                            b     +=  (pix        & 0x1F) * w;
                            total +=   w;
                        }
                    }

                    if (total > 0)
                    {
                        r /= total;
                        g /= total;
                        b /= total;
                        src_set = true;
                    }

                    c = ((int)r << 11) | ((int)g << 5) | (int)b;
                }

                if (src_set && c >= 0)
                    setPixelValue(x,
                                  y,
                                  c,
                                  alpha);
            }
        }
    }
};
