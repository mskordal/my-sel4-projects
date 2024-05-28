import matplotlib.pyplot as plt

# fig, ax = plt.subplots()
# Adjust figure size
fig, ax = plt.subplots(figsize=(6, 4), dpi=300)  # Adjust the width (8 inches) as needed

configs = ['Hardware', 'Software']
counts = [1366, 1971]
bar_colors = ['#ffd400', '#b7b7b7']

# Adjust the width of the bars
bar_width = 0.7  # You can adjust this value as needed
ax.bar(configs, counts, color=bar_colors, width=bar_width)


ax.set_ylabel('Level 1 Instruction Cache Refills', color='white')
ax.set_title('STREAM modified 100K array - 285 runs', color='white')

ax.spines['left'].set_color('white')
ax.spines['bottom'].set_color('white')
ax.spines['right'].set_visible(False)
ax.spines['top'].set_visible(False)

# Loop through x tick labels and set color
for label in ax.get_xticklabels():
    label.set_color('white')

# Set y-axis tick parameters with white label color
ax.tick_params(axis='y', which='both', labelcolor='white')

# Adjust the x-axis limits to make it shorter
# ax.set_xlim(-0.25, len(configs) - 0.75)

# Save the png with same name as script with different extension
figname = __file__[:-2]
plt.savefig(figname + 'png', transparent=True)