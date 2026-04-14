import sys
import pandas as pd
data = pd.read_csv(sys.argv[1],sep=',',header=None)
data = pd.DataFrame(data)


import matplotlib.pyplot as plt
import numpy as np

I = data[0]
#Q = data[1]


plt.plot(I, 'r')
#plt.plot(Q, 'b')
plt.show()


