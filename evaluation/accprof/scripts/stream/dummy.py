import matplotlib.pyplot as plt

# Sample data (replace with your actual data)
x_data = [1, 2, 3, 4, 5]
y_data = [4, 6, 5, 8, 2]

# Create the plot
plt.plot(x_data, y_data)

# Set labels and title
plt.xlabel('X-axis Label')
plt.ylabel('Y-axis Label')
plt.title('Sample Plot')

# Set narrower x-axis limits (adjust as needed)
plt.xlim(2, 4)  # Restricts visible range from x=2 to x=4

# Display the plot
plt.show()