#include "PixelPlanner.h"

uint16_t blend565(uint16_t dst,
                  uint16_t src,
                  float    alpha)
{
    if(alpha <= 0.0f)
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

    return (r << 11) | (g << 5) | b;
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