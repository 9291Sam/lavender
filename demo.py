import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

def lerpolated_vector_field(vectors, x, y):
    epsilon = 1e-5 
    weights = []
    interpolated_vector = np.array([0.0, 0.0])
    
    for px, py, dx, dy in vectors:
        distance = np.sqrt((x - px) ** 2 + (y - py) ** 2) + epsilon
        weight = 1 / distance 
        weights.append(weight)
        
        interpolated_vector += weight * np.array([dx, dy])
    
    total_weight = sum(weights)
    if total_weight > 0:
        interpolated_vector /= total_weight
    
    return tuple(interpolated_vector)

def real_vector_field(x, y):
    dx = 2 * x
    dy = 2 * y
    return dx, dy

# Define the gradient descent function
def gradient_descent(vectors, start, learning_rate=0.1, tolerance=1e-5, max_iter=1000):
    x, y = start
    for _ in range(max_iter):
        dx, dy = lerpolated_vector_field(vectors, x, y)
        step_size = np.sqrt(dx**2 + dy**2)
        if step_size < tolerance:
            break
        x -= learning_rate * dx
        y -= learning_rate * dy
    return x, y

# Set up the grid and vector field
x = np.linspace(-1.5, 1.5, 20)
y = np.linspace(-1.5, 1.5, 20)
X, Y = np.meshgrid(x, y)
Z = X**2 + Y**2 

sampled_vectors = []
for i in range(1, X.shape[0], 12):
    for j in range(1, X.shape[1], 12):
        xi, yi = X[i, j], Y[i, j]
        dx, dy = real_vector_field(xi, yi)
        sampled_vectors.append((xi, yi, dx, dy))

U, V = np.zeros_like(X), np.zeros_like(Y)
for i in range(X.shape[0]):
    for j in range(X.shape[1]):
        U[i, j], V[i, j] = lerpolated_vector_field(
            sampled_vectors, X[i, j], Y[i, j]
        )

# Perform gradient descent to find the minimum
start_point = (-1, -1)  # Starting point for gradient descent
min_x, min_y = gradient_descent(sampled_vectors, start_point)
min_z = min_x**2 + min_y**2

# Add the minimum point's vector to the samples
min_dx, min_dy = real_vector_field(min_x, min_y)
sampled_vectors.append((min_x, min_y, min_dx, min_dy))

# Recompute the interpolated vector field
U_new, V_new = np.zeros_like(X), np.zeros_like(Y)
for i in range(X.shape[0]):
    for j in range(X.shape[1]):
        U_new[i, j], V_new[i, j] = lerpolated_vector_field(
            sampled_vectors, X[i, j], Y[i, j]
        )

# Plot the updated 3D vector field with the new sample
fig = plt.figure(figsize=(12, 8))
ax = fig.add_subplot(111, projection='3d')

ax.plot_surface(X, Y, Z, cmap='viridis', alpha=0.6)

for xi, yi, dx, dy in sampled_vectors:
    zi = xi**2 + yi**2 
    ax.quiver(xi, yi, zi, dx, dy, 0, color='orange', length=0.3, linewidth=5, normalize=True)

ax.quiver(X, Y, Z, U_new, V_new, 0, color='purple', length=0.2, normalize=True)

# Mark the minimum point
ax.scatter(min_x, min_y, min_z, color='red', s=100, label="Minimum")

ax.set_xlim([-1.5, 1.5])
ax.set_ylim([-1.5, 1.5])
ax.set_zlim([0, 3])
ax.legend()

plt.show()