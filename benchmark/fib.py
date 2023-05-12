import sys

def fib(n):
  return n if n < 2 else fib(n - 1) + fib(n - 2)

n = int(sys.argv[1])
m = int(sys.argv[2])
for _ in range(0, n):
  print(fib(m))
