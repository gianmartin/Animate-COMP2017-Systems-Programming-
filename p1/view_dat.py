import numpy as np
from PIL import Image

# Config
WIDTH, HEIGHT = 10, 10

try:
    # 1. Read the raw bytes as a 1D array of unsigned 8-bit integers
    raw_data = np.fromfile("simple.dat", dtype=np.uint8)
    
    # 2. Reshape into (Number of Pixels, 4 Channels)
    # On your Little-Endian PC, ARGB in memory becomes [B, G, R, A] in the file
    pixels = raw_data.reshape(-1, 4)

    # 3. MANUALLY SWAP THE CHANNELS
    # We want to go from [B, G, R, A] (indices 0,1,2,3) 
    # to [R, G, B, A] (indices 2,1,0,3)
    blue = pixels[:, 0].copy()
    red = pixels[:, 2].copy()
    
    pixels[:, 0] = red   # Put Red in index 0
    pixels[:, 2] = blue  # Put Blue in index 2

    # 4. Handle your "0x01 is Opaque" spec
    # If alpha is > 0, set it to 255 so we can see it
    pixels[pixels[:, 3] > 0, 3] = 255

    # 5. Now we can use the universal 'RGBA' mode
    img = Image.fromarray(pixels.reshape((HEIGHT, WIDTH, 4)), 'RGBA')

    # Scale up for visibility
    img = img.resize((400, 400), resample=Image.NEAREST)
    img.show()
    img.save("result_fixed.png")
    print("Success! Manual swizzle complete.")

except Exception as e:
    print(f"Error: {e}")