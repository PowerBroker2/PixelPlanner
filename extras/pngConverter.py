import os
import argparse
from PIL import Image


def rgb888_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def rle_encode(data):
    encoded = []
    i = 0
    n = len(data)

    while i < n:
        value = data[i]
        run = 1
        i += 1

        while i < n and data[i] == value and run < 65535:
            run += 1
            i += 1

        encoded.append((run, value))

    return encoded


def process_image(img, alpha_zero):

    img = img.convert("RGBA")
    width, height = img.size
    pixels = img.load()

    data = []

    for y in range(height):
        for x in range(width):

            r, g, b, a = pixels[x, y]

            if alpha_zero and a < 128:
                data.append(0x0000)
            else:
                data.append(rgb888_to_rgb565(r, g, b))

    return data


def write_header(path, varname, width, height, data, rle=False, sprite=False):

    with open(path, "w") as f:

        f.write("#pragma once\n")
        f.write("#include <Arduino.h>\n\n")

        f.write(f"#define {varname.upper()}_WIDTH {width}\n")
        f.write(f"#define {varname.upper()}_HEIGHT {height}\n\n")

        if not rle:

            f.write(f"static const uint16_t {varname}_pixels[{len(data)}] = {{\n")

            for i, pix in enumerate(data):

                f.write(f"0x{pix:04X},")

                if i % 12 == 11:
                    f.write("\n")

            f.write("\n};\n\n")

        else:

            encoded = rle_encode(data)

            f.write(f"#define {varname.upper()}_RLE_COUNT {len(encoded)}\n\n")

            f.write(f"struct {varname}_rle_t {{ uint16_t len; uint16_t color; }};\n\n")

            f.write(f"static const {varname}_rle_t {varname}_rle[] = {{\n")

            for length, color in encoded:
                f.write(f"{{{length},0x{color:04X}}},\n")

            f.write("};\n\n")

        if sprite:
            f.write(f"#define {varname.upper()}_SPRITE 1\n")


def slice_tiles(img, tile_w, tile_h):

    width, height = img.size
    tiles = []

    for ty in range(0, height, tile_h):
        for tx in range(0, width, tile_w):

            box = (tx, ty, tx + tile_w, ty + tile_h)
            tile = img.crop(box)
            tiles.append(tile)

    return tiles


def convert_file(path, args):

    img = Image.open(path)

    if args.resize:
        w, h = map(int, args.resize.lower().split("x"))
        img = img.resize((w, h), Image.LANCZOS)

    basename = os.path.splitext(os.path.basename(path))[0]

    if args.tile:

        tw, th = map(int, args.tile.lower().split("x"))
        tiles = slice_tiles(img, tw, th)

        for i, tile in enumerate(tiles):

            name = f"{basename}_{i}"
            outpath = os.path.join(args.out, name + ".h")

            data = process_image(tile, args.alpha_zero)

            write_header(outpath, name, tw, th, data, args.rle, args.sprite)

            print("Generated:", outpath)

    else:

        data = process_image(img, args.alpha_zero)
        width, height = img.size

        outpath = os.path.join(args.out, basename + ".h")

        write_header(outpath, basename, width, height, data, args.rle, args.sprite)

        print("Generated:", outpath)


def main():

    parser = argparse.ArgumentParser(description="Convert PNG to TFT C++ header")

    parser.add_argument("input", help="PNG file or folder")
    parser.add_argument("-o", "--out", default=".", help="Output directory")

    parser.add_argument("--resize", help="Resize image (WxH)")
    parser.add_argument("--alpha-zero", action="store_true",
                        help="Convert transparent pixels to 0x0000")

    parser.add_argument("--rle", action="store_true",
                        help="Enable RLE compression")

    parser.add_argument("--sprite", action="store_true",
                        help="Mark output as sprite")

    parser.add_argument("--tile",
                        help="Slice sprite sheet into tiles (WxH)")

    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)

    if os.path.isdir(args.input):

        for file in os.listdir(args.input):

            if file.lower().endswith(".png"):
                convert_file(os.path.join(args.input, file), args)

    else:

        convert_file(args.input, args)


if __name__ == "__main__":
    main()