import os
import math
import requests
from PIL import Image, ImageOps, ImageEnhance, ImageFilter, ImageStat
from io import BytesIO

# Constants
MATRIX_SIZE = (32, 32)
GAMMA = 2.6
PASSTHROUGH = (
    (0, 0, 0),
    (255, 0, 0),
    (255, 255, 0),
    (0, 255, 0),
    (0, 255, 255),
    (0, 0, 255),
    (255, 0, 255),
    (255, 255, 255),
)

# Dither and process to 565-compatible .bmp
def process(filename, output_8_bit=True, passthrough=PASSTHROUGH):
    img = Image.open(filename).convert('RGB')
    img_resized = img.resize((32, 32), Image.LANCZOS)
    err_next_pixel = (0, 0, 0)
    err_next_row = [(0, 0, 0) for _ in range(img_resized.size[0])]
    for row in range(img_resized.size[1]):
        for column in range(img_resized.size[0]):
            pixel = img_resized.getpixel((column, row))
            want = (
                math.pow(pixel[0] / 255.0, GAMMA) * 31.0,
                math.pow(pixel[1] / 255.0, GAMMA) * 63.0,
                math.pow(pixel[2] / 255.0, GAMMA) * 31.0,
            )
            if pixel in passthrough:
                got = (pixel[0] >> 3, pixel[1] >> 2, pixel[2] >> 3)
            else:
                got = (
                    min(max(int(err_next_pixel[0] * 0.5 + err_next_row[column][0] * 0.25 + want[0] + 0.5), 0), 31),
                    min(max(int(err_next_pixel[1] * 0.5 + err_next_row[column][1] * 0.25 + want[1] + 0.5), 0), 63),
                    min(max(int(err_next_pixel[2] * 0.5 + err_next_row[column][2] * 0.25 + want[2] + 0.5), 0), 31),
                )
            err_next_pixel = (want[0] - got[0], want[1] - got[1], want[2] - got[2])
            err_next_row[column] = err_next_pixel
            rgb565 = (
                (got[0] << 3) | (got[0] >> 2),
                (got[1] << 2) | (got[1] >> 4),
                (got[2] << 3) | (got[2] >> 2),
            )
            img_resized.putpixel((column, row), rgb565)

    if output_8_bit:
        img_resized = img_resized.convert('P', palette=Image.ADAPTIVE)

    img_resized.save(filename.split('.png')[0] + '.bmp')

# Example usage
if __name__ == "__main__":

    process("/Users/jamesmgentile/Desktop/college-baseball.png")

