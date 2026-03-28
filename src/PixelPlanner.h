#pragma once
#include <Arduino.h>
#include "Colors.h"

enum class BlitMode { TOP_LEFT,
                      TOP_RIGHT,
                      BOTTOM_LEFT,
                      BOTTOM_RIGHT,
                      CENTER,
                      STRETCH,
                      FIT,
                      FILL };
enum class ScaleMode { NEAREST,
                       BILINEAR,
                       AREA };

// --- Alpha blending for 16-bit RGB565 ---
uint16_t blend565(uint16_t dst,
                  uint16_t src,
                  float    alpha);

// --- Rotate a point around a pivot ---
void rotate_point(float  px,
                  float  py,
                  float  cx,
                  float  cy,
                  float  angle_deg,
                  float &out_x,
                  float &out_y);

template <size_t WIDTH, size_t HEIGHT>
class Pixmap
{
private:
    uint16_t pixels[WIDTH*HEIGHT];
    bool     mask[WIDTH*HEIGHT] = { true };

    constexpr size_t index(size_t x, size_t y) const { return y*WIDTH + x; }

public:
    constexpr size_t width() const { return WIDTH; }
    constexpr size_t height() const { return HEIGHT; }

    void clear()
    {
        memset(pixels, 0,     sizeof(pixels));
        memset(mask,   false, sizeof(mask));
    }

    bool inBounds(size_t x, size_t y) const { return x < WIDTH && y < HEIGHT; }

    void setPixelValue(size_t x, size_t y, uint16_t value, float alpha=1.0f)
    {
        if(inBounds(x,y) && alpha > 0)
        {
            pixels[index(x,y)] = blend565(pixels[index(x,y)], value, alpha);
            mask[index(x,y)]   = true;
        }
    }

    uint16_t getPixelValue(size_t x, size_t y) const { return inBounds(x,y)? pixels[index(x,y)] : 0; }
    uint16_t getMaskValue(size_t x, size_t y) const { return inBounds(x,y)? mask[index(x,y)] : 0; }
    const uint16_t* getPixels() const { return pixels; }
    const bool* getMask() const { return mask; }

    // --- Lines ---
    void draw_line_from_points(float x1,float y1,float x2,float y2,float thickness,bool border_to_border,uint16_t color,float alpha=1.0f)
    {
        float dx_f = x2 - x1;
        float dy_f = y2 - y1;
        if(dx_f == 0 && dy_f == 0) return;

        if(border_to_border)
        {
            float t_min=-1e9,t_max=1e9;
            if(dx_f!=0){float tx1=(0-x1)/dx_f;float tx2=(width()-1-x1)/dx_f;t_min=max(t_min,min(tx1,tx2));t_max=min(t_max,max(tx1,tx2));}
            if(dy_f!=0){float ty1=(0-y1)/dy_f;float ty2=(height()-1-y1)/dy_f;t_min=max(t_min,min(ty1,ty2));t_max=min(t_max,max(ty1,ty2));}
            x1+=t_min*dx_f; y1+=t_min*dy_f; x2=x1+(t_max-t_min)*dx_f; y2=y1+(t_max-t_min)*dy_f;
        }

        int x0=(int)x1, y0=(int)y1, x_end=(int)x2, y_end=(int)y2;
        int dx=abs(x_end-x0), dy=-abs(y_end-y0), sx=(x0<x_end)?1:-1, sy=(y0<y_end)?1:-1, err=dx+dy;
        float len = sqrtf(dx_f*dx_f+dy_f*dy_f); 
        float px=-dy_f/len; 
        float py=dx_f/len;
        int half_thick=(int)(thickness/2.0f);

        while(true)
        {
            for(int t=-half_thick;t<=half_thick;t++)
            {
                int tx=(int)(x0+px*t); int ty=(int)(y0+py*t);
                setPixelValue(tx,ty,color,alpha);
            }
            if(x0==x_end && y0==y_end) break;
            int e2=2*err; if(e2>=dy){err+=dy;x0+=sx;} if(e2<=dx){err+=dx;y0+=sy;}
        }
    }

    void draw_line_from_pt_slope(float x,float y,float slope,float thickness,bool border_to_border,float line_distance,uint16_t color,float alpha=1.0f)
    {
        float x1,y1,x2,y2;
        if(isinf(slope))
        {
            if(border_to_border){x1=x;y1=0;x2=x;y2=height()-1;}
            else{float half=line_distance/2.0f;x1=x;y1=y-half;x2=x;y2=y+half;}
        }
        else
        {
            float dx=1.0f,dy=slope,len=sqrtf(dx*dx+dy*dy);dx/=len; dy/=len;
            if(border_to_border){float t_min=-1e9,t_max=1e9; if(dx!=0){float tx1=(0-x)/dx; float tx2=(width()-1-x)/dx; t_min=max(t_min,min(tx1,tx2)); t_max=min(t_max,max(tx1,tx2));} 
            if(dy!=0){float ty1=(0-y)/dy; float ty2=(height()-1-y)/dy; t_min=max(t_min,min(ty1,ty2)); t_max=min(t_max,max(ty1,ty2));} x1=x+t_min*dx;y1=y+t_min*dy;x2=x+t_max*dx;y2=y+t_max*dy;}
            else{float half=line_distance/2.0f;x1=x-dx*half;y1=y-dy*half;x2=x+dx*half;y2=y+dy*half;}
        }
        draw_line_from_points(x1,y1,x2,y2,thickness,false,color,alpha);
    }

    // --- Circle ---
    void draw_circle(int cx,int cy,int radius,int thickness,bool solid,uint16_t color,float alpha=1.0f)
    {
        if(radius<=0) return;
        int r_outer=radius,r_inner=solid?0:max(0,radius-thickness);
        for(int y=-r_outer;y<=r_outer;y++)
        {
            int outer_x=(int)sqrt(r_outer*r_outer-y*y);
            int inner_x=(r_inner>0)?(int)sqrt(r_inner*r_inner-y*y):0;
            for(int x=-outer_x;x<=-inner_x;x++) setPixelValue(cx+x,cy+y,color,alpha);
            for(int x=inner_x;x<=outer_x;x++) setPixelValue(cx+x,cy+y,color,alpha);
        }
    }

    // --- Rectangle ---
    void draw_rect(int x,int y,int w,int h,int thickness,bool solid,uint16_t color,float alpha=1.0f)
    {
        if(w<=0||h<=0) return; thickness=max(1,thickness);
        if(solid){for(int j=0;j<h;j++) for(int i=0;i<w;i++) setPixelValue(x+i,y+j,color,alpha); return;}
        int max_thick=min(w/2,h/2); thickness=min(thickness,max_thick);
        for(int t=0;t<thickness;t++){for(int i=0;i<w;i++){setPixelValue(x+i,y+t,color,alpha); setPixelValue(x+i,y+h-1-t,color,alpha);}}
        for(int t=0;t<thickness;t++){for(int j=0;j<h;j++){setPixelValue(x+t,y+j,color,alpha); setPixelValue(x+w-1-t,y+j,color,alpha);}}
    }

    // --- Filled triangle ---
    void fill_triangle(int x0,int y0,int x1,int y1,int x2,int y2,uint16_t color,float alpha=1.0f)
    {
        int minX=min(x0,min(x1,x2)), maxX=max(x0,max(x1,x2)), minY=min(y0,min(y1,y2)), maxY=max(y0,max(y1,y2));
        for(int y=minY;y<=maxY;y++)
            for(int x=minX;x<=maxX;x++)
            {
                int w0=(x1-x0)*(y-y0)-(y1-y0)*(x-x0);
                int w1=(x2-x1)*(y-y1)-(y2-y1)*(x-x1);
                int w2=(x0-x2)*(y-y2)-(y0-y2)*(x-x2);
                if((w0>=0 && w1>=0 && w2>=0)||(w0<=0 && w1<=0 && w2<=0)) setPixelValue(x,y,color,alpha);
            }
    }

    // --- Rotated rectangle ---
    void draw_rect_rotated(float x,float y,float w,float h,float angle_deg,float pivot_x,float pivot_y,int thickness,bool solid,uint16_t color,float alpha=1.0f)
    {
        float x0=x,y0=y,x1=x+w,y1=y,x2=x+w,y2=y+h,x3=x,y3=y+h;
        float rx0,ry0,rx1,ry1,rx2,ry2,rx3,ry3;
        rotate_point(x0,y0,pivot_x,pivot_y,angle_deg,rx0,ry0);
        rotate_point(x1,y1,pivot_x,pivot_y,angle_deg,rx1,ry1);
        rotate_point(x2,y2,pivot_x,pivot_y,angle_deg,rx2,ry2);
        rotate_point(x3,y3,pivot_x,pivot_y,angle_deg,rx3,ry3);

        if(solid){fill_triangle((int)(rx0+0.5f),(int)(ry0+0.5f),(int)(rx1+0.5f),(int)(ry1+0.5f),(int)(rx2+0.5f),(int)(ry2+0.5f),color,alpha);
                   fill_triangle((int)(rx0+0.5f),(int)(ry0+0.5f),(int)(rx2+0.5f),(int)(ry2+0.5f),(int)(rx3+0.5f),(int)(ry3+0.5f),color,alpha);}
        else
        {
            auto draw_edge=[&](float xs,float ys,float xe,float ye){
                float dx=xe-xs,dy=ye-ys,len=sqrtf(dx*dx+dy*dy);if(len==0) return;
                float px=-dy/len*(thickness/2.0f),py=dx/len*(thickness/2.0f);
                float vx0=xs+px,vy0=ys+py,vx1=xs-px,vy1=ys-py,vx2=xe+px,vy2=ye+py,vx3=xe-px,vy3=ye-py;
                fill_triangle((int)(vx0+0.5f),(int)(vy0+0.5f),(int)(vx1+0.5f),(int)(vy1+0.5f),(int)(vx2+0.5f),(int)(vy2+0.5f),color,alpha);
                fill_triangle((int)(vx1+0.5f),(int)(vy1+0.5f),(int)(vx3+0.5f),(int)(vy3+0.5f),(int)(vx2+0.5f),(int)(vy2+0.5f),color,alpha);
            };
            draw_edge(rx0,ry0,rx1,ry1); draw_edge(rx1,ry1,rx2,ry2);
            draw_edge(rx2,ry2,rx3,ry3); draw_edge(rx3,ry3,rx0,ry0);
        }
    }

    template <typename T>
    void blitFromArray(const T*  src,
                    size_t    srcW,
                    size_t    srcH,
                    BlitMode  mode,
                    ScaleMode scaleMode = ScaleMode::BILINEAR,
                    uint16_t  (*convert)(T) = nullptr,
                    float     alpha = 1.0f)  // <-- new alpha parameter
    {
        if (!src || srcW == 0 || srcH == 0)
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
        else if (mode == BlitMode::FIT || mode == BlitMode::FILL)
        {
            float sx = (float)width() / srcW;
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

                uint16_t c = 0;

                if (scaleMode == ScaleMode::NEAREST)
                {
                    int xi = roundf(srcXf);
                    int yi = roundf(srcYf);
                    if (xi >= 0 && xi < (int)srcW && yi >= 0 && yi < (int)srcH)
                        c = toColor(src[yi * srcW + xi]);
                }
                else if (scaleMode == ScaleMode::BILINEAR)
                {
                    int x0 = floorf(srcXf);
                    int y0 = floorf(srcYf);
                    int x1 = min(x0 + 1, (int)srcW - 1);
                    int y1 = min(y0 + 1, (int)srcH - 1);

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
                        int ab = a & 0x1F;

                        int br = (b >> 11) & 0x1F;
                        int bg = (b >> 5)  & 0x3F;
                        int bb = b & 0x1F;

                        int rr = ar + (int)((br - ar) * t);
                        int rg = ag + (int)((bg - ag) * t);
                        int rb = ab + (int)((bb - ab) * t);

                        return (rr << 11) | (rg << 5) | rb;
                    };

                    uint16_t cx0 = lerp565(c00, c10, fx);
                    uint16_t cx1 = lerp565(c01, c11, fx);
                    c = lerp565(cx0, cx1, fy);
                }
                else if (scaleMode == ScaleMode::AREA)
                {
                    float x0 = srcXf;
                    float y0 = srcYf;
                    float x1 = srcXf + scaleX;
                    float y1 = srcYf + scaleY;

                    float r = 0, g = 0, b = 0, total = 0;

                    int ix0 = floorf(x0), iy0 = floorf(y0);
                    int ix1 = ceilf(x1),  iy1 = ceilf(y1);

                    for (int sy = iy0; sy < iy1; sy++)
                    {
                        if (sy < 0 || sy >= (int)srcH) continue;
                        float wy = min(y1, (float)(sy+1)) - max(y0, (float)sy);
                        if (wy <= 0) continue;

                        for (int sx = ix0; sx < ix1; sx++)
                        {
                            if (sx < 0 || sx >= (int)srcW) continue;
                            float wx = min(x1, (float)(sx+1)) - max(x0, (float)sx);
                            if (wx <= 0) continue;

                            float w = wx * wy;
                            uint16_t pix = toColor(src[sy * srcW + sx]);
                            r += ((pix >> 11) & 0x1F) * w;
                            g += ((pix >> 5) & 0x3F) * w;
                            b += (pix & 0x1F) * w;
                            total += w;
                        }
                    }

                    if (total > 0) { r /= total; g /= total; b /= total; }
                    c = ((int)r << 11) | ((int)g << 5) | (int)b;
                }

                // --- ALPHA BLENDING ---
                setPixelValue(x, y, c, alpha);
            }
        }
    }
};