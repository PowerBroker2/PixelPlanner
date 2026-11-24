import re
from pathlib import Path
from typing import Optional

from PIL import Image, ImageOps
import typer


app = typer.Typer(no_args_is_help=True, context_settings={"help_option_names": ["-h", "--help"]})


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

            f.write(f"static const uint16_t {varname}_pixels[{len(data)}] PROGMEM = {{\n")

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


def convert_file(path, output, *, resize=None, tile=None, alpha_zero=False,
                 rle=False, sprite=False, used_names=None):

    with Image.open(path) as source:
        img = ImageOps.exif_transpose(source).convert("RGBA")

    if resize:
        img = img.resize(resize, Image.Resampling.LANCZOS)

    basename = Path(path).stem
    basename = re.sub(r"[^a-zA-Z0-9_]+", "_", basename).strip("_") or "image"
    if basename[0].isdigit():
        basename = "image_" + basename

    def export(image, name):
        name = unique_name(name, used_names)
        outpath = output / f"{name}.h"
        write_header(outpath, name, *image.size, process_image(image, alpha_zero), rle, sprite)
        typer.echo(f"Generated: {outpath}")

    if tile:
        for i, piece in enumerate(slice_tiles(img, *tile)):
            export(piece, f"{basename}_{i}")
    else:
        export(img, basename)


def unique_name(name, used_names):
    if used_names is None:
        return name
    base = name
    number = 2
    while name.casefold() in used_names:
        name = f"{base}_{number}"
        number += 1
    used_names.add(name.casefold())
    return name


def dimensions(value):
    match = re.fullmatch(r"([1-9][0-9]*)[xX]([1-9][0-9]*)", value)
    if not match:
        raise ValueError("Use positive dimensions in WIDTHxHEIGHT format, like 160x120.")
    return tuple(map(int, match.groups()))


def parse_dimensions(value, option):
    if value is None:
        return None
    try:
        return dimensions(value)
    except ValueError as exc:
        raise typer.BadParameter(str(exc), param_hint=option) from exc


@app.command()
def convert(
    source: Path = typer.Argument(..., exists=True, readable=True,
                                  help="Image file or folder of images."),
    output: Path = typer.Option(Path("."), "--out", "--output", "-o", file_okay=False,
                               help="Folder for the generated headers."),
    resize: Optional[str] = typer.Option(None, metavar="WIDTHxHEIGHT",
                                        help="Resize before export, for example 160x120."),
    tile: Optional[str] = typer.Option(None, metavar="WIDTHxHEIGHT",
                                      help="Split into tiles, for example 16x16."),
    alpha_zero: bool = typer.Option(False, "--alpha-zero",
                                    help="Turn pixels with alpha below 128 into black."),
    rle: bool = typer.Option(False, "--rle", help="Output run-length encoded pixels."),
    sprite: bool = typer.Option(False, "--sprite", help="Add a sprite marker to the header."),
):
    """Convert a PNG, JPEG, or other image into a PixelPlanner RGB565 header."""
    resize = parse_dimensions(resize, "--resize")
    tile = parse_dimensions(tile, "--tile")
    path = source
    try:
        if source.is_dir():
            extensions = {suffix for suffix, format_name in Image.registered_extensions().items()
                          if format_name in Image.OPEN}
            images = sorted(entry for entry in source.iterdir()
                            if entry.is_file() and entry.suffix.lower() in extensions)
            if not images:
                typer.echo(f"No supported image files found in {source}.", err=True)
                raise typer.Exit(1)
        else:
            images = [source]
        output.mkdir(parents=True, exist_ok=True)
        used_names = set()
        for path in images:
            convert_file(path, output, resize=resize, tile=tile, alpha_zero=alpha_zero,
                         rle=rle, sprite=sprite, used_names=used_names)
    except (OSError, ValueError, Image.DecompressionBombError) as exc:
        typer.echo(f"Error processing {path}: {exc}", err=True)
        raise typer.Exit(1) from exc


def main():
    app()


if __name__ == "__main__":
    main()
