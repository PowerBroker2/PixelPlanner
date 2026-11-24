#include "PixelPlanner.h"

uint16_t blend565(uint16_t dst,
                  uint16_t src,
                  float    alpha)
{
    if (!(alpha > 0.0f))
        return dst;
    
    if(alpha >= 1.0f)
        return src;

    int dr = (dst >> 11) & 0x1F;
    int dg = (dst >> 5)  & 0x3F;
    int db =  dst        & 0x1F;

    int sr = (src >> 11) & 0x1F;
    int sg = (src >> 5)  & 0x3F;
    int sb =  src        & 0x1F;

    int r = dr + (int)((sr - dr) * alpha);
    int g = dg + (int)((sg - dg) * alpha);
    int b = db + (int)((sb - db) * alpha);

    return (uint16_t(r) << 11) | (uint16_t(g) << 5) | uint16_t(b);
}

void rotate_point(float  px,
                  float  py,
                  float  cx,
                  float  cy,
                  float  angle_deg,
                  float &out_x,
                  float &out_y)
{
    float s = sinf(angle_deg * DEG_TO_RAD);
    float c = cosf(angle_deg * DEG_TO_RAD);

    px -= cx;
    py -= cy;

    float xnew = px * c - py * s;
    float ynew = px * s + py * c;

    out_x = xnew + cx;
    out_y = ynew + cy;
}

namespace
{
constexpr int TileSize = 16;
constexpr int SamplesPerAxis = 4;

struct Point
{
    float x, y;

    Point operator+(Point p) const { return {x + p.x, y + p.y}; }
    Point operator-(Point p) const { return {x - p.x, y - p.y}; }
    Point operator*(float scale) const { return {x * scale, y * scale}; }
    bool operator==(Point p) const { return x == p.x && y == p.y; }
};

Point vertex(const float* points, size_t i) { return {points[2 * i], points[2 * i + 1]}; }
Point normal(Point p) { return {-p.y, p.x}; }
float cross(Point a, Point b) { return a.x * b.y - a.y * b.x; }
float dot(Point a, Point b) { return a.x * b.x + a.y * b.y; }

struct Bounds
{
    float left, top, right, bottom;

    void include(Point p)
    {
        left = min(left, p.x);
        top = min(top, p.y);
        right = max(right, p.x);
        bottom = max(bottom, p.y);
    }

    void expand(float amount)
    {
        left -= amount;
        top -= amount;
        right += amount;
        bottom += amount;
    }
};

Bounds boundsOf(Point p) { return {p.x, p.y, p.x, p.y}; }

int bounded(float edge, int low, int high)
{
    return edge <= low ? low : edge >= high ? high : int(edge);
}

PixmapRect clipBounds(Bounds bounds, PixmapRect clip)
{
    const int x = bounded(floorf(bounds.left), clip.x, clip.x + clip.width);
    const int y = bounded(floorf(bounds.top), clip.y, clip.y + clip.height);
    const int right = bounded(ceilf(bounds.right), x, clip.x + clip.width);
    const int bottom = bounded(ceilf(bounds.bottom), y, clip.y + clip.height);
    return {x, y, right - x, bottom - y};
}

bool pathBounds(const float* points, size_t count, Bounds& bounds)
{
    if (!points || !count || count > SIZE_MAX / 2)
        return false;
    bounds = boundsOf(vertex(points, 0));
    for (size_t i = 0; i < count; ++i)
    {
        const auto point = vertex(points, i);
        if (!isfinite(point.x) || !isfinite(point.y))
            return false;
        bounds.include(point);
    }
    return true;
}

struct CoverageTile
{
    PixmapRect area;
    bool antialias;
    uint16_t samples[TileSize * TileSize]{};

    CoverageTile(PixmapRect area, bool antialias) : area(area), antialias(antialias) {}

    template <typename Contains>
    void stamp(Bounds bounds, Contains contains)
    {
        const auto box = clipBounds(bounds, area);
        const int steps = antialias ? SamplesPerAxis : 1;
        const float step = 1.0f / steps;
        for (int y = box.y; y < box.y + box.height; ++y)
            for (int x = box.x; x < box.x + box.width; ++x)
            {
                auto& bits = samples[(y - area.y) * TileSize + x - area.x];
                for (int sy = 0; sy < steps; ++sy)
                    for (int sx = 0; sx < steps; ++sx)
                    {
                        const uint16_t bit = uint16_t(1u) << (sy * steps + sx);
                        // Bias center samples consistently on shared pixel boundaries.
                        const float bias = antialias ? 0.0f : -0.0001f;
                        if (!(bits & bit) && contains(Point{x + (sx + 0.5f) * step + bias,
                                                           y + (sy + 0.5f) * step + bias}))
                            bits |= bit;
                    }
            }
    }

    void circle(Point center, float radius)
    {
        auto bounds = boundsOf(center);
        bounds.expand(radius);
        const float squared = radius * radius;
        stamp(bounds, [&](Point p) { const auto d = p - center; return dot(d, d) <= squared; });
    }

    void polygon(const Point* shape, int count)
    {
        auto bounds = boundsOf(shape[0]);
        float signedArea = 0.0f;
        for (int i = 1; i < count; ++i)
        {
            bounds.include(shape[i]);
            if (i + 1 < count)
                signedArea += cross(shape[i] - shape[0], shape[i + 1] - shape[0]);
        }
        if (signedArea == 0.0f || !isfinite(signedArea))
            return;

        stamp(bounds, [&](Point p) {
            for (int i = 0; i < count; ++i)
            {
                const auto next = shape[(i + 1) % count];
                const float edge = cross(next - shape[i], p - shape[i]);
                if (signedArea > 0.0f ? edge < 0.0f : edge > 0.0f)
                    return false;
            }
            return true;
        });
    }

    void output(void* target, pixelplanner_detail::PixelSink put) const
    {
        for (int row = 0; row < area.height; ++row)
            for (int col = 0; col < area.width; ++col)
            {
                uint16_t bits = samples[row * TileSize + col];
                if (!bits)
                    continue;
                unsigned opacity = 255;
                if (antialias)
                {
                    unsigned hits = 0;
                    while (bits)
                    {
                        bits &= bits - 1;
                        ++hits;
                    }
                    opacity = (hits * 255 + 8) / 16;
                }
                put(target, area.x + col, area.y + row, uint8_t(opacity));
            }
    }
};

void join(CoverageTile& tile, Point point, Point before, Point after, const StrokeStyle& style)
{
    const float radius = style.width * 0.5f;
    const float turn = cross(before, after);
    if (style.join == StrokeJoin::ROUND)
    {
        if (turn != 0.0f || dot(before, after) < 0.0f)
            tile.circle(point, radius);
        return;
    }
    if (fabsf(turn) < 1e-6f)
        return;
    const float side = turn > 0 ? -radius : radius;
    const auto outsideBefore = normal(before) * side;
    const auto outsideAfter = normal(after) * side;
    if (style.join == StrokeJoin::MITER)
    {
        const float denominator = 1 + dot(before, after);
        const float limit = max(1.0f, style.miterLimit);
        if (denominator > 0.0f && 2.0f / denominator <= limit * limit)
        {
            const auto tip = (outsideBefore + outsideAfter) * (1.0f / denominator);
            const Point shape[] = {point, point + outsideBefore, point + tip, point + outsideAfter};
            tile.polygon(shape, 4);
            return;
        }
    }
    const Point shape[] = {point, point + outsideBefore, point + outsideAfter};
    tile.polygon(shape, 3);
}

void strokeTile(CoverageTile& tile, const float* points, size_t count, bool closed, const StrokeStyle& style)
{
    const float radius = style.width * 0.5f;
    const auto first = vertex(points, 0);
    Point from = first;
    Point firstDirection{}, previousDirection{};
    bool drewSegment = false;
    const size_t segments = closed ? count : count - 1;
    for (size_t i = 0; i < segments; ++i)
    {
        const auto to = vertex(points, (i + 1) % count);
        const auto delta = to - from;
        const float length = hypotf(delta.x, delta.y);
        if (!(length > 0.0f) || !isfinite(length))
            continue;

        const Point direction{delta.x / length, delta.y / length};
        const auto offset = normal(direction) * radius;
        auto start = from, end = to;
        if (!closed && style.cap == StrokeCap::SQUARE)
        {
            if (!drewSegment)
                start = start - direction * radius;
            if (i + 1 == segments)
                end = end + direction * radius;
        }
        const Point shape[] = {start + offset, start - offset, end - offset, end + offset};
        tile.polygon(shape, 4);
        if (drewSegment)
            join(tile, from, previousDirection, direction, style);
        else
            firstDirection = direction;
        previousDirection = direction;
        drewSegment = true;
        from = to;
    }

    if (closed && drewSegment)
        join(tile, first, previousDirection, firstDirection, style);
    else if (!closed && style.cap == StrokeCap::ROUND)
    {
        tile.circle(first, radius);
        if (!(from == first))
            tile.circle(from, radius);
    }
    else if (!closed && !drewSegment && style.cap == StrokeCap::SQUARE)
    {
        const Point square[] = {{first.x - radius, first.y - radius}, {first.x + radius, first.y - radius},
                                {first.x + radius, first.y + radius}, {first.x - radius, first.y + radius}};
        tile.polygon(square, 4);
    }
}

template <typename Paint>
void rasterTiles(PixmapRect bounds, bool antialias, void* target, pixelplanner_detail::PixelSink put,
                 Paint paint)
{
    const int endY = bounds.y + bounds.height, endX = bounds.x + bounds.width;
    for (int y = bounds.y; y < endY;)
    {
        const int rows = min(TileSize, endY - y);
        for (int x = bounds.x; x < endX;)
        {
            const int columns = min(TileSize, endX - x);
            CoverageTile tile({x, y, columns, rows}, antialias);
            paint(tile);
            tile.output(target, put);
            x += columns;
        }
        y += rows;
    }
}
}

void pixelplanner_detail::strokePath(const float* points, size_t count, bool closed, const StrokeStyle& style,
                                     PixmapRect clip, void* target, PixelSink put)
{
    Bounds bounds;
    if (!pathBounds(points, count, bounds) || !(style.width > 0.0f) ||
        !isfinite(style.width) || !isfinite(style.miterLimit))
        return;

    // Trim redundant closing vertices and repeated endpoints for cap selection.
    while (count > 1 && vertex(points, count - 1) == vertex(points, closed ? 0 : count - 2))
        --count;
    const float extension = style.join == StrokeJoin::MITER ? max(1.5f, style.miterLimit) : 1.5f;
    bounds.expand(style.width * 0.5f * extension);
    rasterTiles(clipBounds(bounds, clip), style.antialias == Antialiasing::COVERAGE, target, put,
                [&](CoverageTile& tile) { strokeTile(tile, points, count, closed, style); });
}

void pixelplanner_detail::fillPath(const float* points, size_t count, PixmapRect clip,
                                   void* target, PixelSink put)
{
    Bounds bounds;
    if (count < 3 || !pathBounds(points, count, bounds))
        return;

    rasterTiles(clipBounds(bounds, clip), true, target, put, [&](CoverageTile& tile) {
        tile.stamp(bounds, [&](Point p) {
            bool inside = false;
            Point previous = vertex(points, count - 1);
            for (size_t i = 0; i < count; ++i)
            {
                const auto current = vertex(points, i);
                if ((current.y > p.y) != (previous.y > p.y))
                {
                    const double t = (double(p.y) - current.y) / (double(previous.y) - current.y);
                    if (p.x < (1 - t) * current.x + t * previous.x)
                        inside = !inside;
                }
                previous = current;
            }
            return inside;
        });
    });
}

void pixelplanner_detail::radialPath(int cx, int cy, int radius, int innerRadius, float angle, float span,
                                     PixmapRect clip, void* target, PixelSink put)
{
    const Point center{cx + 0.5f, cy + 0.5f};
    auto bounds = boundsOf(center);
    bounds.expand(radius);
    const float outerSquared = float(radius) * radius;
    const float innerSquared = float(innerRadius) * innerRadius;
    rasterTiles(clipBounds(bounds, clip), true, target, put, [&](CoverageTile& tile) {
        tile.stamp(bounds, [&](Point point) {
            const auto offset = point - center;
            const float distance = dot(offset, offset);
            if (distance < innerSquared || distance > outerSquared)
                return false;
            if (span >= 360.0f)
                return true;
            const float direction = atan2f(offset.y, offset.x) / DEG_TO_RAD;
            return fmodf(direction - angle + 720.0f, 360.0f) <= span;
        });
    });
}
