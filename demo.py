import numpy as np
import matplotlib.pyplot as plt
from dataclasses import dataclass
from typing import List, Tuple

@dataclass
class SampledPoint:
    position: np.ndarray  # [x, y]
    gradient: np.ndarray  # [dx, dy]

    @property
    def x(self) -> float:
        return self.position[0]

    @property
    def y(self) -> float:
        return self.position[1]

    @property
    def dx(self) -> float:
        return self.gradient[0]

    @property
    def dy(self) -> float:
        return self.gradient[1]
    
    def __eq__(self, other: object) -> bool:
        return np.array_equal(self.position, other.position) and np.array_equal(self.gradient, other.gradient)

    
def quad_f(x: float, y: float) -> float:
    return x**4 + y**4 + 2*x**2 + 2*y**2

def quad_grad_f(position: np.ndarray) -> np.ndarray:
    x, y = position
    grad_x = 4 * x**3 + 4 * x
    grad_y = 4 * y**3 + 4 * y
    return np.array([grad_x, grad_y])

def exp_par_f(x: float, y: float) -> float:
    return x**2 + y**2 + np.exp(x + y)

def exp_par_grad_f(position: np.ndarray) -> np.ndarray:
    x, y = position
    common_term = np.exp(x + y)
    return np.array([2 * x + common_term, 2 * y + common_term])

def nc_f(x: float, y: float) -> float:
    return np.sin(x**2 + y**2) + (x**2 - y**2)**2

def nc_grad_f(position: np.ndarray) -> np.ndarray:
    x, y = position
    grad_x = 2 * x * np.cos(x**2 + y**2) + 4 * x * (x**2 - y**2)
    grad_y = 2 * y * np.cos(x**2 + y**2) - 4 * y * (x**2 - y**2)
    return np.array([grad_x, grad_y])

choice = 1;

def f(x: float, y: float) -> float:
    return [quad_f, exp_par_f, nc_f][choice](x, y)

def grad_f(p: np.ndarray) -> np.ndarray:
    return [quad_grad_f, exp_par_grad_f, nc_grad_f][choice](p)

def inverse_distance_weighted_interpolated_vector_field_sample(points: List[SampledPoint], x: float, y: float) -> np.ndarray:
    epsilon: float = 1e-5
    target: np.ndarray = np.array([x, y])
    weights: List[float] = []
    interpolated_vector: np.ndarray = np.zeros(2)
    
    for point in points:
        distance: float = np.linalg.norm(target - point.position) + epsilon
        weight: float = 1 / distance
        weights.append(weight)
        interpolated_vector += weight * point.gradient
    
    total_weight: float = sum(weights)
    if total_weight > 0:
        interpolated_vector /= total_weight
    
    return interpolated_vector

def gradient_descent(
    points: List[SampledPoint], 
    start: Tuple[float, float], 
    learning_rate: float = 0.1, 
    tolerance: float = 1e-5, 
    max_iter: int = 1000
) -> np.ndarray:
    current_position: np.ndarray = np.array(start)
    for _ in range(max_iter):
        gradient: np.ndarray = inverse_distance_weighted_interpolated_vector_field_sample(points, *current_position)
        step_size: float = np.linalg.norm(gradient)
        if step_size < tolerance:
            break
        current_position -= learning_rate * gradient
    return current_position

def iterative_minimization(
    initial_points: List[SampledPoint], 
    start: Tuple[float, float], 
    iterations: int = 10, 
    learning_rate: float = 0.1
) -> Tuple[List[SampledPoint], List[SampledPoint]]:
    sampled_points: List[SampledPoint] = initial_points.copy()
    current_position: np.ndarray = np.array(start)
    added_samples: List[SampledPoint] = []

    for _ in range(iterations):
        minimum_position: np.ndarray = gradient_descent(sampled_points, current_position, learning_rate)
        new_sample: SampledPoint = SampledPoint(minimum_position, grad_f(minimum_position))
        added_samples.append(new_sample)
        
        sampled_points.append(new_sample)
        current_position = minimum_position 
    
    return added_samples, sampled_points

x: np.ndarray = np.linspace(-1.5, 1.5, 20)
y: np.ndarray = np.linspace(-1.5, 1.5, 20)
X, Y = np.meshgrid(x, y)
Z: np.ndarray = f(X, Y)

initial_points: List[SampledPoint] = []
for i in range(1, X.shape[0], 12):
    for j in range(1, X.shape[1], 12):
        xi, yi = X[i, j], Y[i, j]
        initial_points.append(SampledPoint(np.array([xi, yi]), grad_f(np.array([xi, yi]))))

start_point: Tuple[float, float] = (-1.0, -1.0) 
added_samples, final_sampled_points = iterative_minimization(initial_points, start_point, iterations=10)

U: np.ndarray = np.zeros_like(X)
V: np.ndarray = np.zeros_like(Y)
for i in range(X.shape[0]):
    for j in range(X.shape[1]):
        interpolated_gradient: np.ndarray = inverse_distance_weighted_interpolated_vector_field_sample(final_sampled_points, X[i, j], Y[i, j])
        U[i, j], V[i, j] = interpolated_gradient

fig = plt.figure(figsize=(12, 8))
ax = fig.add_subplot(111, projection='3d')

ax.plot_surface(X, Y, Z, cmap='viridis', alpha=0.6)

for point in initial_points:
    zi: float = f(point.x, point.y)
    ax.quiver(point.x, point.y, zi, point.dx, point.dy, 0, color='orange', length=0.3, linewidth=5, normalize=True)

ax.quiver(X, Y, Z, U, V, 0, color='purple', length=0.2, normalize=True)

i: float = 0.0
for point in added_samples:
    zi: float = f(point.x, point.y)
    ax.scatter(point.x, point.y, zi + i, color='red', s=50, label="Added Sample" if point == added_samples[0] else "")
    i += 0.2

ax.set_xlim([-1.5, 1.5])
ax.set_ylim([-1.5, 1.5])
ax.set_zlim([0.75, 5])
ax.legend()

plt.show()

