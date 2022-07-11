import sys

if __name__ == "__main__":
    key = sys.stdin.readline().strip()
    val = map(lambda x: int(x), sys.stdin.readline().split())
    print(key, sum(val))
