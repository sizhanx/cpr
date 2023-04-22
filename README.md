# Optimized `cp -r`
## Sizhan Xu, Kristina Zhou
## Overview
We are utilizing `io_uring` and `fallocate` make `cp -r` faster. We designed a scheduling mechanism so that we can batch hundreds of read requests at a time. `fallocate` can make the enlarging the file size faster. 
## Design

### Creations of folders and files
In order to copy the entire directory tree, we first have to create the new folders and the new files in the new directory. We immplemented a recrusive function that would make the folders with `mkdir` and create files with `creat` & `fallocate`. 

### Scheduling reads into buffer
Once the directory tree is copied, read requests for source files are submitted to `io_uring`. Upon completion of those reads, corresponding write requests take their place in the submission queue. 
