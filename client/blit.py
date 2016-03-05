from render import drawtext
import cairo
import bitarray
import os, sys, struct
from time import sleep

surf = cairo.ImageSurface(cairo.FORMAT_RGB24, 120, 7)
ctx = cairo.Context(surf)


data = surf.get_data()

stride = surf.get_stride()
row = [None] * 7

nsf = 3
nrows = 7 * nsf
time = 100


#    print bitarray.bitarray(row[i])

while True:
    os.write(sys.stdout.fileno(), struct.pack("<BBH",0xa9,nrows,time<<nsf)) # start frame
    for i in range(0,7):
        row[i] = data[i*stride:(i+1)*stride][2::4] # green channel
        #print "%r" % len(row[i])
    for sf in range(0,nsf):
        for i in range(0,7):
            ba = bitarray.bitarray()
            ba.frombytes(row[i])
            ba2 = ba[sf::8] # MSB
            #print time<<sf
            os.write(sys.stdout.fileno(), struct.pack("<B15sH",1<<i, ba2.tobytes(), time<<sf))

    drawtext(ctx)
    sleep(0.005)
