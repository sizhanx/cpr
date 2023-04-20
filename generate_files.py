import string
import random

for i in range(10):
    filename = "test_many_small_files/med_file" + str(i) + ".txt"
    f = open(filename, "w")
    line = ""
    for j in range(2 ** 19):
        line = line + random.choice(string.ascii_letters)
        if j % 1000 == 0:
            line = line + "\n"
    f.write(line + "\n")
    f.close()
