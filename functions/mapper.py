import sys

if __name__ == "__main__":
    for line in sys.stdin:
        for word in line.split():
            word.strip('\n ')
            print(word, 1)