import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
from dataclasses import dataclass

# Define a dataclass for sampled points
@dataclass
class SampledPoint:
    position: np.ndarray  # [x, y]
    gradient: np.ndarray  # [dx, dy]

    @property
    def x(self):
        return self.position[0]

    @property
    def y(self):
        return self.position[1]

    @property
    def dx(self):
        return self.gradient[0]

    @property
    def dy(self):
        return self.gradient[1]
    
    def __eq__(self, other):
        if not isinstance(other, SampledPoint):
            return NotImplemented
        return np.array_equal(self.position, other.position) and np.array_equal(self.gradient, other.gradient)


# Define the scalar field f and its gradient ∇f
def f(x, y):
    return x**2 + y**2 + np.exp(x + y)

def grad_f(position):
    x, y = position
    common_term = np.exp(x + y)
    return np.array([2 * x + common_term, 2 * y + common_term])

# Interpolated vector field based on sampled points
def lerpolated_vector_field(points, x, y):
    epsilon = 1e-5
    target = np.array([x, y])
    weights = []
    interpolated_vector = np.zeros(2)
    
    for point in points:
        distance = np.linalg.norm(target - point.position) + epsilon
        weight = 1 / distance
        weights.append(weight)
        interpolated_vector += weight * point.gradient
    
    total_weight = sum(weights)
    if total_weight > 0:
        interpolated_vector /= total_weight
    
    return interpolated_vector

# Gradient descent function
def gradient_descent(points, start, learning_rate=0.1, tolerance=1e-5, max_iter=1000):
    current_position = np.array(start)
    for _ in range(max_iter):
        gradient = lerpolated_vector_field(points, *current_position)
        step_size = np.linalg.norm(gradient)
        if step_size < tolerance:
            break
        current_position -= learning_rate * gradient
    return current_position

# Iteratively refine the minimum
def iterative_minimization(initial_points, start, iterations=10, learning_rate=0.1):
    sampled_points = initial_points.copy()
    current_position = np.array(start)
    added_samples = []

    for _ in range(iterations):
        # Perform gradient descent
        minimum_position = gradient_descent(sampled_points, current_position, learning_rate)
        added_samples.append(SampledPoint(minimum_position, grad_f(minimum_position)))
        
        # Add the new minimum point to sampled points
        sampled_points.append(SampledPoint(minimum_position, grad_f(minimum_position)))
        current_position = minimum_position  # Update starting point
    
    return added_samples, sampled_points

# Initial samples
x = np.linspace(-1.5, 1.5, 20)
y = np.linspace(-1.5, 1.5, 20)
X, Y = np.meshgrid(x, y)
Z = f(X, Y)

initial_points = []
for i in range(1, X.shape[0], 12):
    for j in range(1, X.shape[1], 12):
        xi, yi = X[i, j], Y[i, j]
        initial_points.append(SampledPoint(np.array([xi, yi]), grad_f(np.array([xi, yi]))))

# Perform iterative minimization
start_point = (-1.0, -1.0)  # Starting point
added_samples, final_sampled_points = iterative_minimization(initial_points, start_point, iterations=10)

# Compute interpolated vector field
U, V = np.zeros_like(X), np.zeros_like(Y)
for i in range(X.shape[0]):
    for j in range(X.shape[1]):
        interpolated_gradient = lerpolated_vector_field(final_sampled_points, X[i, j], Y[i, j])
        U[i, j], V[i, j] = interpolated_gradient

# Plot the vector field and sampled points
fig = plt.figure(figsize=(12, 8))
ax = fig.add_subplot(111, projection='3d')

# Plot the scalar field surface
ax.plot_surface(X, Y, Z, cmap='viridis', alpha=0.6)

# Plot initial sampled vectors
for point in initial_points:
    zi = f(point.x, point.y)
    ax.quiver(point.x, point.y, zi, point.dx, point.dy, 0, color='orange', length=0.3, linewidth=5, normalize=True)

# Plot the interpolated vector field
ax.quiver(X, Y, Z, U, V, 0, color='purple', length=0.2, normalize=True)

# Mark newly added samples
i = 0.0
for point in added_samples:
    zi = f(point.x, point.y)
    ax.scatter(point.x, point.y, zi + i, color='red', s=50, label="Added Sample" if point == added_samples[0] else "")
    i += 0.2

# Add plot labels
ax.set_xlim([-1.5, 1.5])
ax.set_ylim([-1.5, 1.5])
ax.set_zlim([0.75, 3])
ax.set_title("Iterative Gradient Descent with Added Samples")
ax.legend()

plt.show()
