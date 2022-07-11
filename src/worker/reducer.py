import sys

if __name__ == "__main__":
    # result = ""
    with open(sys.argv[1] + "_out", "w") as out:
        with open(sys.argv[1], "r") as f:
                for line in f:
                    vals = line.split()
                    key = vals[0]
                    val = map(lambda x: int(x), vals[1:])
                    # result = result + key + " " + str(sum(val)) + "\n"
                    out.write(key + " " + str(sum(val)) + "\n")
    print("reducer " + sys.argv[1] + " finished")
