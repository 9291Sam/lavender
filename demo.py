import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

def lerpolated_vector_field(vectors, x, y):
    # inverse distance weighting
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

x = np.linspace(-1.5, 1.5, 20)
y = np.linspace(-1.5, 1.5, 20)
X, Y = np.meshgrid(x, y)
Z = X**2 + Y**2 

sampled_vectors = []
for i in range(0, X.shape[0], 5):
    for j in range(0, X.shape[1], 5):
        xi, yi = X[i, j], Y[i, j]
        dx, dy = real_vector_field(xi, yi)
        sampled_vectors.append((xi, yi, dx, dy))

U, V = np.zeros_like(X), np.zeros_like(Y)
for i in range(X.shape[0]):
    for j in range(X.shape[1]):
        U[i, j], V[i, j] = lerpolated_vector_field(
            sampled_vectors, X[i, j], Y[i, j]
        )

fig = plt.figure(figsize=(12, 8))
ax = fig.add_subplot(111, projection='3d')

ax.plot_surface(X, Y, Z, cmap='viridis', alpha=0.6)

for xi, yi, dx, dy in sampled_vectors:
    zi = xi**2 + yi**2 
    ax.quiver(xi, yi, zi, dx, dy, 0, color='orange', length=0.3, linewidth=5, normalize=True)


ax.quiver(X, Y, Z, U, V, 0, color='purple', length=0.2, normalize=True)

ax.set_xlim([-1.5, 1.5])
ax.set_ylim([-1.5, 1.5])
ax.set_zlim([0, 3])

plt.show()
