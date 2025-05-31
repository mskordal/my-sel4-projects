import numpy as np
import matplotlib.pyplot as plt

x_coords = ['APF', 'APF-Soft']
y_vals = [8034, 14187]
x = np.arange(len(x_coords))
# Create thx = [0, 0.7]e bar chart
plt.figure(figsize=(11, 8))
# plt.bar(x, width=0.4, height=y_vals, color='black')
plt.bar(x[0], height=y_vals[0], width=0.4, color='black', label="APF")  # Solid black
plt.bar(x[1], height=y_vals[1], width=0.4, color=(0, 0, 0, 0.4), label="APF-Soft")
plt.ylabel('Level 1 Data Cache Refills', fontsize=28)
plt.title('STREAM 1K array - 500 runs', fontsize=32)
plt.xticks(ticks=x, labels=x_coords, fontsize=31)
plt.yticks(fontsize=27)
plt.tight_layout()
plt.grid(axis='y', linestyle='--', alpha=1)

figname = __file__[:-2]
plt.savefig(figname + 'png')
