# Mean Blur Optimization

This is a project to optimize an image blurring algorithm. Each subsequent mask version in mask.c attempts to improve on the previous version. 
---
## Successful strategies pursued are as follows:

* reduction of cache misses by changing the pattern of access to the images
* eliminating unnecessary work and writing to output image the minimum necessary amount of times
* reduction of branch prediction miss through eliminating most if statements, as well as less instructions in the pipeline overall. Instead, explicitly calculating the top and bottom rows, as well as the first and last pixel of each row, in order to avoid boundary checks at each iteration which is unnecessary for the majority of the image.
* multithreading the workload

mask14(), mask15() in mask.c are optimized single threaded implementations

mask16() is the optimized multithreaded implemention

---
## Strategies that did not work include:
* reduction of branch prediction miss through manual gcc compiler hints __builtin_expect(), probably because the CPU hardware branch predictor is much better at this than I am
* reduction of cache misses with manual prefetch through gcc __builtin_prefetch(), again, probably because the CPU hardware prefetch already does this

---
## TODO:
* another image access pattern: working down the image going through cache line size number of bytes worth of pixels at a time. This should reduce cache misses by 3 times for the entire image, I have some ideas why my implemention doesn't work even though I think it should (mask7, mask8).

---
## To run this program:

1. run **make** on a pthread and gcc supported system
2. ./timemask <src_image.pgm> <number_of_trials> <output_image.pgm>
