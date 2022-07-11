import sys

if __name__ == "__main__":
    result = ""
    with open(sys.argv[1] + "_out", "w") as out:
        with open(sys.argv[1], "r") as f:
                for line in f:
                    for word in line.split():
                        word.strip('\n ')
                        out.write(word + " 1\n")
    print("mapper " + sys.argv[1] + " finished")
