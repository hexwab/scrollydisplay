import numpy as np
from time import sleep
import struct
from bitarray import bitarray
import os, sys

def life_step_1(X):
    """Game of life step using generator expressions"""
    nbrs_count = sum(np.roll(np.roll(X, i, 0), j, 1)
                     for i in (-1, 0, 1) for j in (-1, 0, 1)
                     if (i != 0 or j != 0))
    return (nbrs_count == 3) | (X & (nbrs_count == 2))

def life_step_2(X):
    """Game of life step using scipy tools"""
    from scipy.signal import convolve2d
    nbrs_count = convolve2d(X, np.ones((3, 3)), mode='same', boundary='wrap') - X
    return (nbrs_count == 3) | (X & (nbrs_count == 2))
    
life_step = life_step_1


#np.random.seed(0)
X = np.zeros((7, 120), dtype=bool)
r = np.random.random((7, 120))
X[:] = (r > 0.75)
#glider = [[True, True, True], [False, False, True], [False, True, False]]
#X[3:6,60:63] = glider
#X[0:3,40:43] = glider

while True:
    os.write(sys.stdout.fileno(), struct.pack("<BBH",0xa9,7,500))
#    r = np.random.random((7, 120))
#    X[:] = (r > 0.75)
    X = life_step(X)
    for i in range(0,7):
        row = X[i].tolist()
        os.write(sys.stdout.fileno(), struct.pack("<B15sH",1<<i, bitarray(row).tobytes(), 500))
    sleep(.05)
