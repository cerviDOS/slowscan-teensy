import sys
import pandas as pd
from PIL import Image
import subprocess
import select
import cv2
import numpy as np

image = Image.new(mode='RGB', size=(320, 256), color=(0,0,0))
pixels: Image.PixelAccess = image.load()

teensy_output = subprocess.Popen(['tail', '-F', sys.argv[1]], \
    stdout=subprocess.PIPE)
io = select.poll()
io.register(teensy_output.stdout)

width = 320
height = 256

x = 0
y = 0

img = np.zeros([height, width, 3], dtype=np.uint8)
while True:
    while io.poll(1):
        split = teensy_output.stdout.readline() \
            .decode() \
            .replace('\r\n', '') \
            .split(',', maxsplit=3)

        print(split)
        
        ignore_line = False

        for val in split:
            if not val.isdigit():
                ignore_line = True
                break

        if ignore_line or len(split) != 3:
            continue

        red, green, blue = tuple(map(int, split))

        img[y, x, 0] = red
        img[y, x, 1] = green
        img[y, x, 2] = blue

        x += 1
        if x == 320:
            x = 0
            if y == 255:
                y = 0
            else:
                y += 1
        cv2.imshow('SSTV', img)

    cv2.waitKey(1)





'''
data = pd.read_csv(sys.argv[1],sep=',',header=None)
data = pd.DataFrame(data)

red_channel = data[0]
green_channel = data[1]
blue_channel = data[2]

image = Image.new(mode='RGB', size=(320, 256), color=(0,0,0))
pixels: Image.PixelAccess = image.load()

x = 0
y = 0

for pixel in zip(red_channel, green_channel, blue_channel):
    pixels[x,y] = pixel

    x += 1
    if x == 320:
        x = 0
        if y == 255:
            break
        else:
            y += 1
'''

