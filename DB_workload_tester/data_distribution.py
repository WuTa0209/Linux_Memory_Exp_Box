import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from collections import Counter

# read file
counts = Counter()
for chunk in pd.read_csv("db_data.txt", comment="#", header=None,
                         names=["time","object","size","next"], chunksize=10**6):
    counts.update(chunk["object"])

# heat distribution
freqs = np.array(list(counts.values()))
freqs_sorted = np.sort(freqs)[::-1]

# Draw rank-frequency plot (Zipf plot)
plt.loglog(range(1, len(freqs_sorted)+1), freqs_sorted, marker=".")
plt.xlabel("Rank")
plt.ylabel("Frequency")
plt.title("Object popularity (Zipf plot)")
plt.show()

# Estimate Zipf alpha (linear regression log-log)
ranks = np.arange(1, len(freqs_sorted)+1)
slope, _ = np.polyfit(np.log(ranks[:1000]), np.log(freqs_sorted[:1000]), 1)
print("Estimated Zipf alpha ~", -slope)
