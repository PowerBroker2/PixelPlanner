# PixelPlanner

Compose RGB565 pixmaps with shapes, images, and bitmap text. A `Pixmap<W, H>` stores its pixels and a transparency mask. It can be used on its own or handed to TeensyTFT for display.

## Draw a label with a custom font

Generate a header with [glyph-bridge](https://github.com/PowerBroker2/glyph-bridge) and place it next to your sketch.

```sh
glyph-bridge --family Inter --size 24 --export --output ./MySketch
```

```cpp
#include <PixelPlanner.h>
#include <TeensyTFT.h>
#include "font_inter_regular_24.h"

TeensyTFT display(10, 9, 255, 255, 255);
Pixmap<160, 40> label;

void setup()
{
    if (!display.begin())
        return;

    label.draw_rect(0, 0, 160, 40, 1, false, CYAN);
    label.print(8, 6, "Ready", font_inter_regular_24::Font, WHITE);
    display.fillRegion(20, 20, label.width(), label.height(),
                       label.getPixels(), label.getMask());
    display.swap();
}

void loop() {}
```

The generated header's base name is its C++ namespace. Older glyph-bridge headers still use the same `BitmapFont` interface. To shrink a new font, export only the printable ASCII characters your sketch needs with `--charset`.

## Images and transparency

- `clear()` resets pixels to black and clears the mask. Drawing operations set the mask where they write. Pass the mask to TeensyTFT to preserve the background where the pixmap is transparent.
- `blit_from_pixmap()` composites another pixmap using its mask. `blitFromArray()` accepts packed RGB565 rows or a Boolean stencil. Use `NEAREST` for pixel art, `BILINEAR` for a smoother upscale, or `AREA` when downsampling.

### Convert images to RGB565

Install Pillow and Typer with `python -m pip install Pillow "typer>=0.16"`, then convert an image or a directory of images.

```sh
python extras/imageConverter.py photo.jpg
python extras/imageConverter.py ./assets -o ./MySketch
python extras/imageConverter.py photo.jpg --resize 160x120 -o ./MySketch
python extras/imageConverter.py --help
```

The converter reads PNG, JPEG, BMP, GIF, WebP, TIFF, and other formats supported by your Pillow installation. It applies EXIF orientation, uses the first frame of animations and multipage images, and gives colliding names distinct suffixes in folder mode. [Pillow image format reference](https://pillow.readthedocs.io/en/stable/handbook/image-file-formats.html)

Headers go to the current directory by default. Use `-o`, `--out`, or `--output` to choose a different folder. The default header contains a packed RGB565 array such as `photo_pixels`, with `PHOTO_WIDTH` and `PHOTO_HEIGHT` constants. Pass these to `blitFromArray()`. Use `--tile 16x16` for sprite sheets, `--rle` for run-length encoded output, or `--alpha-zero` to turn transparent pixels black.

### Resize while drawing

`destination.blitResized(source, x, y, width, height)` draws a scaled copy into an existing pixmap. The source stays at its original size, and resizing needs no heap allocation. The default nearest-neighbor sampler has a fast path for native-size and 2× drawing. Pass `ScaleMode::BILINEAR` for smooth scaling or `ScaleMode::AREA` when reducing an image.

```cpp
Pixmap<32, 24> icon;
Pixmap<160, 120> canvas;
// Draw into icon first, then reuse it at any size.
canvas.blitResized(icon, 12, 8, 64, 48);
```

### Redraw geometry at the new resolution

Use `redrawResized(sourceWidth, sourceHeight, draw)` when you have the original drawing commands. PixelPlanner clears the destination, fits the original coordinate space into it while preserving aspect ratio, and draws the geometry at the destination resolution. The callback receives a scaled drawing surface with rectangle, line, circle, and arc methods. The same callback can draw to several pixmap sizes.

```cpp
Pixmap<120, 120> enlarged;
enlarged.redrawResized(60, 60, [](auto& drawing) {
    drawing.draw_circle(30, 30, 20, 2, false, CYAN);
});
```

Native redraw produces fresh edges for shapes. Photos and other raster assets still use `blitResized()` because their source pixels are the available detail.

In the graphics demo, press a tile to enlarge it to 2×. The tile stays enlarged until release, even if you drag away. Serial Monitor prints the active method and accepts `c` to calibrate touch. [Graphics demo](examples/PixelPlanner_Test/PixelPlanner_Test.ino)

| Method | Demo tiles |
| --- | --- |
| Native redraw | Arcs, alpha blending |
| Nearest neighbor | Polylines, flood fill |
| Bilinear | Polygon, mixed shapes, PNG |
| Area | Pixmap compositing |

### Draw from a sprite sheet

Keep several sprites in one atlas and draw a source rectangle from it. `blitRegion()` accepts a pixmap, a packed RGB565 array, or a Boolean stencil. It uses the same placement and scaling modes as the other blits, with stretch and nearest neighbor as its defaults.

```cpp
// Select the second 40 × 40 sprite from an 80 × 40 atlas.
const PixmapRect sprite{40, 0, 40, 40};
canvas.blitRegion(atlas, sprite, {12, 8, 80, 80});

// Rotate that sprite clockwise around its center and draw it at 2×.
BlitTransform pose(52, 48);
pose.pivotX = pose.pivotY = 20;
pose.angle = 35;
pose.scaleX = pose.scaleY = 2;
canvas.blitTransformed(atlas, sprite, pose, ScaleMode::BILINEAR);
```

The pivot is measured from the selected sprite's top-left edge. The position passed to `BlitTransform` puts that pivot on the destination. Both operations clip to the destination and respect pixmap masks. Use separate source and destination buffers, and keep the source rectangle within the atlas. Neither operation allocates an intermediate pixmap. For rotated blits, `AREA` estimates coverage with a fixed 4 × 4 sample grid.

Press the compass tile in the graphics demo to see both operations together. The dial and needle share an atlas. While you hold, the enlarged needle spins around its pivot and only the compass tile is redrawn.

### Stroke paths and clip drawing

`strokePath()` draws an open or closed path in one pass. Segment overlaps, joins, and self-crossings are composited once, so a translucent stroke stays even. Set `StrokeStyle::join` to `MITER`, `BEVEL`, or `ROUND`, and choose `BUTT`, `SQUARE`, or `ROUND` caps for open paths. Miters that exceed `miterLimit` fall back to bevels.

`setClipRect()` limits writes to a rectangle. Use `intersectClipRect()` to narrow the current clip, and save `getClipRect()` if you need to restore it after nested drawing. `resetClipRect()` restores the full pixmap. `clear()` always clears the whole pixmap and keeps the current clip.

```cpp
Pixmap<80, 60, true> layer;
Pixmap<80, 60> canvas;
const float path[] = {0, 32, 24, 10, 52, 44, 80, 24};

StrokeStyle stroke;
stroke.width = 5;
stroke.join = StrokeJoin::ROUND;
stroke.cap = StrokeCap::ROUND;
stroke.antialias = Antialiasing::COVERAGE;

layer.setClipRect({5, 5, 70, 50});
layer.strokePath(path, 4, false, stroke, CYAN, 0.8f);

canvas.draw_rect(0, 0, 80, 60, 0, true, RGB565(22, 30, 43));
canvas.blit_from_pixmap(layer);
```

### Keep antialiasing through compositing

`Antialiasing::COVERAGE` uses a 4 × 4 sample grid for path strokes, polygons, circles, and arcs. `Pixmap<W, H, true>` stores coverage in one extra byte per pixel. Use it for transparent layers that will be drawn onto another pixmap. Regular `Pixmap<W, H>` instances keep their Boolean mask and can still draw antialiased edges directly onto an existing background.

PixelPlanner blits read partial coverage from the source. For APIs that accept only RGB565 pixels and a Boolean mask, composite the layer onto its final background before passing it out. The demo's polyline tile uses that approach to show rounded and mitered joins clipped to a panel.
