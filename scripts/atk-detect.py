import numpy as np
import matplotlib.pyplot as plt
import matplotlib.widgets as widgets

# Define function execution pattern
functions = ["Copy", "Scale", "Add", "Triad"]
total_ticks = 128  # Each tick represents one function execution

# Generate ground truth attack data (1 = attack happened, 0 = normal)
ground_truth = np.zeros(total_ticks)
ground_truth[::4] = 1  # Every 4th tick (Copy) is attacked

# Simulate verifier's detection (allowing manual specification of FP and FN)
predicted_attacks = ground_truth.copy()

# Manually specify False Negatives (FN) - missed attack detections
false_negative_indices = []

# 4th cycle: Add and Triad are false negatives
#Scale
false_negative_indices.extend([0])
false_negative_indices.extend([15, 14])
false_negative_indices.extend([19, 18, 16])
false_negative_indices.extend([20, 22])

# 5th cycle: Scale, Add, and Triad are false negatives
# false_negative_indices.extend([19, 18, 16])

# 6th cycle: Scale and Add are false negatives

for idx in false_negative_indices:
    predicted_attacks[idx] = 1  # Missing actual attacks

# Compute categories
true_positives = (ground_truth == 1) & (predicted_attacks == 1)
true_negatives = (ground_truth == 0) & (predicted_attacks == 0)
false_positives = (ground_truth == 0) & (predicted_attacks == 1)
false_negatives = (ground_truth == 1) & (predicted_attacks == 0)

# Generate function labels in the order (Copy, Scale, Add, Triad)
function_labels = functions * (total_ticks // 4)

# Create figure and axis with wider size for better readability
fig, ax = plt.subplots(figsize=(20, 6))

# Plot data points with updated colors and transparency
tp_plot = ax.scatter(np.where(true_positives)[0], np.ones(sum(true_positives)), color='black', label="True Positive (Correct Attack Detection)", marker='s', s=80)
tn_plot = ax.scatter(np.where(true_negatives)[0], np.zeros(sum(true_negatives)), color='black', alpha=0.4, label="True Negative (Correct Normal Detection)", marker='s', s=80)
fp_plot = ax.scatter(np.where(false_positives)[0], np.ones(sum(false_positives)), color='black', label="False Positive (Incorrect Attack Detection)", marker='x', s=100)
fn_plot = ax.scatter(np.where(false_negatives)[0], np.zeros(sum(false_negatives)), color='black', alpha=0.4, label="False Negative (Missed Attack)", marker='x', s=100)

# Vertical warmup line
warmup_boundary = 25
warmup_line = ax.axvline(warmup_boundary, color='black', linestyle='--', linewidth=2, label="Warmup Phase Boundary")

# Show all function names in x-axis and remove unnecessary lines
ax.set_xticks(range(total_ticks))
ax.set_xticklabels(function_labels, rotation=90, fontsize=6)  # Reduce font size slightly for readability
ax.grid(False)

ax.set_xlabel("Function Calls (Copy, Scale, Add, Triad repeating)")
ax.set_ylabel("Detection Status (1 = Attack, 0 = Normal)")
ax.set_title("Detection Performance: TP, TN, FP, FN in Attestation Stream Prime and Probe, Scale Attacked")
ax.legend()

# Create a slider to adjust warmup boundary position
# ax_slider = plt.axes([0.2, 0.01, 0.65, 0.03])  # Position slider below the graph
# slider = widgets.Slider(ax_slider, "Warmup Phase", 0, total_ticks - 1, valinit=warmup_boundary, valstep=1)

# Function to update the warmup line position
def update(val):
    warmup_line.set_xdata(val)
    fig.canvas.draw_idle()

# slider.on_changed(update)

plt.savefig('plot.svg')
# plt.show()
