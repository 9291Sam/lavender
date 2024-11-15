numbers = [-0.3, 1.2, -0.5, 0.1, -0.9, 0.8]

var = 0.0666666666666666666;

total = 0.0;

for n in numbers:
    total += (n - var) ** 2;

total /= 6

print(total)
