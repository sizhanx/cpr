import string
import random

for i in range(15000):
    filename = "test_many_small_files/small_file" + str(i) + ".txt"
    f = open(filename, "w")
    line = ""
    for j in range(10):
        line = line + random.choice(string.ascii_letters)
    f.write(line + "\n")
    f.close()
