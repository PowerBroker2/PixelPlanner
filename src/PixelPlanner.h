#pragma once
#include <Arduino.h>
#include <vector>
#include <algorithm>

/**
 * @brief Anchors for positioning areas on the screen
 */
enum class AreaAnchor {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center,
    TopEdgeCenter,
    BottomEdgeCenter,
    LeftEdgeCenter,
    RightEdgeCenter
};

/**
 * @brief Represents a rectangular area/sprite to be drawn
 */
struct Area {
    int w, h;                  ///< Width and height in pixels
    int x, y;                   ///< Position on screen (depends on anchor)
    AreaAnchor anchor;          ///< How x,y is interpreted
    uint8_t rotation;           ///< Rotation in 90-degree steps (0-3)
    const uint16_t* pixels;     ///< Pixel buffer (w*h RGB565)
    bool visible;               ///< Whether to draw the area
    int priority;               ///< Draw order priority (higher = drawn on top)
};

/**
 * @brief Compositing scene for a user-provided buffer
 *
 * Users can define multiple areas, update positions, visibility, priority, and compose
 * them into a frame buffer for display.
 */
class PixelPlanner {
public:
    /**
     * @brief Construct a scene tied to a user buffer
     * @param buffer Pointer to a preallocated RGB565 buffer
     * @param width Buffer width in pixels
     * @param height Buffer height in pixels
     */
    PixelPlanner(uint16_t* buffer, int width, int height);

    /**
     * @brief Add a new area to the scene
     * @return index of the created area
     */
    int addArea(int w, int h, int x, int y,
                AreaAnchor anchor = AreaAnchor::TopLeft,
                uint8_t rotation = 0,
                const uint16_t* pixels = nullptr,
                bool visible = true,
                int priority = 0);

    /**
     * @brief Remove an area by index
     */
    void removeArea(int index);

    /**
     * @brief Clear all areas
     */
    void clearAreas();

    /**
     * @brief Update location of an area
     */
    void updateAreaLocation(int index, int x, int y, AreaAnchor anchor);

    /**
     * @brief Update the pixel data for an area
     */
    void updateAreaPixels(int index, const uint16_t* pixels);

    /**
     * @brief Set the visibility of an area
     */
    void setAreaVisible(int index, bool visible);

    /**
     * @brief Set the priority of an area (higher = drawn on top)
     */
    void setAreaPriority(int index, int priority);

    /**
     * @brief Compose all areas into the buffer
     * @param clearColor Optional background color
     */
    void compose(uint16_t clearColor = 0x0000);

    /**
     * @brief Get a reference to an area by index
     */
    Area& getArea(int index);

private:
    uint16_t* buf;
    int bufWidth, bufHeight;
    std::vector<Area> areas;

    void resolveAnchor(Area& area);
    void blitArea(const Area& area);

    static bool areaCompare(const Area& a, const Area& b) {
        return a.priority < b.priority;
    }
};