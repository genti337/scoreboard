from PIL import Image
import sys

def resize_bmp(input_path, output_path, width, height):
    # Open BMP image
    img = Image.open(input_path)

    # Resize with high-quality resampling
    resized = img.resize((width, height), Image.LANCZOS)

    # Save as BMP
    resized.save(output_path, format="BMP")
    print(f"Saved resized image to {output_path}")

# Example usage
if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: python resize_bmp.py input.bmp output.bmp width height")
    else:
        in_path = sys.argv[1]
        out_path = sys.argv[2]
        w = int(sys.argv[3])
        h = int(sys.argv[4])
        resize_bmp(in_path, out_path, w, h)
