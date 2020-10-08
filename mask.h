#ifndef MASK_H
#define MASK_H

#define N (2 * 1024)

#define WEIGHT_CENTRE 4
#define WEIGHT_SIDE   2
#define WEIGHT_CORNER 1

long mask(long oldImage[N][N], long newImage[N][N], int rows, int cols);
long baseMask(long oldImage[N][N], long newImage[N][N], int rows, int cols);

#endif

