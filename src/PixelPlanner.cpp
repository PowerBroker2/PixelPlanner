#include "PixelPlanner.h"

PixelPlanner::PixelPlanner(uint16_t* buffer, int width, int height)
: buf(buffer), bufWidth(width), bufHeight(height)
{}

int PixelPlanner::addArea(int w, int h, int x, int y,
                          AreaAnchor anchor, uint8_t rotation,
                          const uint16_t* pixels,
                          bool visible,
                          int priority)
{
    Area a{w, h, x, y, anchor, static_cast<uint8_t>(rotation % 4), pixels, visible, priority};
    areas.push_back(a);
    return areas.size() - 1;
}

void PixelPlanner::removeArea(int index)
{
    if(index >=0 && index < (int)areas.size())
        areas.erase(areas.begin() + index);
}

void PixelPlanner::clearAreas()
{
    areas.clear();
}

void PixelPlanner::updateAreaLocation(int index, int x, int y, AreaAnchor anchor)
{
    if(index<0 || index>=(int)areas.size()) return;
    areas[index].x = x;
    areas[index].y = y;
    areas[index].anchor = anchor;
}

void PixelPlanner::updateAreaPixels(int index, const uint16_t* pixels)
{
    if(index<0 || index>=(int)areas.size()) return;
    areas[index].pixels = pixels;
}

void PixelPlanner::setAreaVisible(int index, bool visible)
{
    if(index<0 || index>=(int)areas.size()) return;
    areas[index].visible = visible;
}

void PixelPlanner::setAreaPriority(int index, int priority)
{
    if(index<0 || index>=(int)areas.size()) return;
    areas[index].priority = priority;
}

Area& PixelPlanner::getArea(int index)
{
    return areas[index];
}

void PixelPlanner::compose(uint16_t clearColor)
{
    // Clear buffer
    for(int i=0;i<bufWidth*bufHeight;i++) buf[i] = clearColor;

    // Sort by priority (lowest first), stable sort maintains creation order
    std::stable_sort(areas.begin(), areas.end(), [](const Area& a, const Area& b){
        return a.priority < b.priority;
    });

    for(auto& area : areas)
    {
        if(area.visible) {
            resolveAnchor(area);
            blitArea(area);
        }
    }
}

void PixelPlanner::resolveAnchor(Area& area)
{
    switch(area.anchor)
    {
        case AreaAnchor::TopLeft: break;
        case AreaAnchor::TopRight:  area.x -= area.w; break;
        case AreaAnchor::BottomLeft: area.y -= area.h; break;
        case AreaAnchor::BottomRight: area.x -= area.w; area.y -= area.h; break;
        case AreaAnchor::Center: area.x -= area.w/2; area.y -= area.h/2; break;
        case AreaAnchor::TopEdgeCenter: area.x -= area.w/2; break;
        case AreaAnchor::BottomEdgeCenter: area.x -= area.w/2; area.y -= area.h; break;
        case AreaAnchor::LeftEdgeCenter: area.y -= area.h/2; break;
        case AreaAnchor::RightEdgeCenter: area.x -= area.w; area.y -= area.h/2; break;
    }
}

void PixelPlanner::blitArea(const Area& area)
{
    if(!area.pixels) return;

    for(int row=0; row<area.h; row++)
    {
        for(int col=0; col<area.w; col++)
        {
            int srcX = col;
            int srcY = row;
            int destX = area.x;
            int destY = area.y;

            // Apply rotation
            switch(area.rotation)
            {
                case 0: destX += srcX; destY += srcY; break;
                case 1: destX += area.h - 1 - srcY; destY += srcX; break;
                case 2: destX += area.w - 1 - srcX; destY += area.h - 1 - srcY; break;
                case 3: destX += srcY; destY += area.w - 1 - srcX; break;
            }

            // Clip
            if(destX<0 || destX>=bufWidth || destY<0 || destY>=bufHeight) continue;

            uint16_t pix = area.pixels[row*area.w + col];
            if(pix==0x0000) continue; // transparent
            buf[destY*bufWidth + destX] = pix;
        }
    }
}