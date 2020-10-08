#include <stdio.h>
#include <stdlib.h>
#include "mask.h"
#include <pthread.h>

#ifndef MASK_VERSION
#define MASK_VERSION mask16
#endif

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define CACHE_LINE_SIZE 64
#define ELEM_PER_CACHE_LINE CACHE_LINE_SIZE / 8
#define PREFETCH_OFFSET 32
#define PREFETCH_LOCALITY 3

#define WEIGHT_CORNER_TOTAL (WEIGHT_CENTRE + 2 * WEIGHT_SIDE + WEIGHT_CORNER)
#define WEIGHT_EDGE_TOTAL (3 * WEIGHT_SIDE + 2 * WEIGHT_CORNER + WEIGHT_CENTRE)
#define WEIGHT_CENTER_TOTAL (WEIGHT_CENTRE + 4 * WEIGHT_SIDE + 4 * WEIGHT_CORNER)

// #pragma GCC push_options
// #pragma GCC optimize ("unroll-loops")
// #pragma GCC optimize ("O3") // turn this on to get to 0.005 of original runtime

static inline long mask0(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    return baseMask(oldImage, newImage, rows, cols);
}

/**Simply row major
The optimized implementation took:
        Best   :       299124 usec (ratio: 0.198059)
        Average:       307414 usec (ratio: 0.199212)
 */
static inline long mask1(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    int i, j;
    int col, row;
    long check = 0;

    long(*weight)[N] = calloc(N * N, sizeof(long));

    //initialize the new image
    for (i = 0; i < cols; i++)
        for (j = 0; j < rows; j++)
        {
            newImage[j][i] = WEIGHT_CENTRE * oldImage[j][i];
            weight[j][i] = WEIGHT_CENTRE;
        }

    // Count the cells to the top left
    for (j = 1; j < rows; j++)
    {
        row = j - 1;
        for (i = 1; i < cols; i++)
        {
            col = i - 1;
            newImage[j][i] += WEIGHT_CORNER * oldImage[row][col];
            weight[j][i] += WEIGHT_CORNER;
        }
    }

    // Count the cells immediately above
    for (j = 1; j < rows; j++)
    {
        row = j - 1;
        for (i = 0; i < cols; i++)
        {
            newImage[j][i] += WEIGHT_SIDE * oldImage[row][i];
            weight[j][i] += WEIGHT_SIDE;
        }
    }

    // Count the cells to the top right
    for (j = 1; j < rows; j++)
    {
        row = j - 1;
        for (i = 0; i < cols - 1; i++)
        {
            col = i + 1;
            newImage[j][i] += WEIGHT_CORNER * oldImage[row][col];
            weight[j][i] += WEIGHT_CORNER;
        }
    }

    // Count the cells to the immediate left
    for (j = 0; j < rows; j++)
    {
        for (i = 1; i < cols; i++)
        {
            col = i - 1;
            newImage[j][i] += WEIGHT_SIDE * oldImage[j][col];
            weight[j][i] += WEIGHT_SIDE;
        }
    }

    // Count the cells to the immediate right
    for (j = 0; j < rows; j++)
    {
        for (i = 0; i < cols - 1; i++)
        {
            col = i + 1;
            newImage[j][i] += WEIGHT_SIDE * oldImage[j][col];
            weight[j][i] += WEIGHT_SIDE;
        }
    }

    // Count the cells to the bottom left
    for (j = 0; j < rows - 1; j++)
    {
        row = j + 1;
        for (i = 1; i < cols; i++)
        {
            col = i - 1;
            newImage[j][i] += WEIGHT_CORNER * oldImage[row][col];
            weight[j][i] += WEIGHT_CORNER;
        }
    }

    // Count the cells immediately below
    for (j = 0; j < rows - 1; j++)
    {
        row = j + 1;
        for (i = 0; i < cols; i++)
        {
            newImage[j][i] += WEIGHT_SIDE * oldImage[row][i];
            weight[j][i] += WEIGHT_SIDE;
        }
    }

    // Count the cells to the bottom right
    for (j = 0; j < rows - 1; j++)
    {
        row = j + 1;
        for (i = 0; i < cols - 1; i++)
        {
            col = i + 1;
            newImage[j][i] += WEIGHT_CORNER * oldImage[row][col];
            weight[j][i] += WEIGHT_CORNER;
        }
    }

    // Produce the final result
    for (j = 0; j < rows; j++)
    {
        for (i = 0; i < cols; i++)
        {
            newImage[j][i] = newImage[j][i] / weight[j][i];
            check += newImage[j][i];
        }
    }

    return check;
}

/**One loop
 * Wait a minute...NO DIFFERENCE!?
The optimized implementation took:
        Best   :       312546 usec (ratio: 0.207447)
        Average:       350922 usec (ratio: 0.225776)
 */
static inline long mask2(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    register int i, j;
    register int top, bot, left, right; // TODO maybe not necessary
    register long check = 0;

    long(*weight)[N] = calloc(N * N, sizeof(long));

    //initialize the new image
    for (j = 0; j < rows; j++)
    {
        top = j - 1; // TODO some ternary to make bound checking easier
        bot = j + 1;
        for (i = 0; i < cols; i++)
        {
            left = i - 1;
            right = i + 1;

            // TODO remove weight
            // TODO temp var for image[j][i]
            //initialize the new image
            newImage[j][i] = WEIGHT_CENTRE * oldImage[j][i];
            weight[j][i] = WEIGHT_CENTRE;

            // TODO __builtin_expect
            // TODO do borders first, then middle? nah bad cache
            // TODO define cols - 1 and rows - 1?
            // Count the cells to the top left
            if (j > 0 && i > 0)
            {
                newImage[j][i] += WEIGHT_CORNER * oldImage[top][left];
                weight[j][i] += WEIGHT_CORNER;
            }

            // Count the cells immediately above
            if (j > 0)
            {
                newImage[j][i] += WEIGHT_SIDE * oldImage[top][i];
                weight[j][i] += WEIGHT_SIDE;
            }

            // Count the cells to the top right
            if (j > 0 && i < cols - 1)
            {
                newImage[j][i] += WEIGHT_CORNER * oldImage[top][right];
                weight[j][i] += WEIGHT_CORNER;
            }

            // Count the cells to the immediate left
            if (i > 0)
            {
                newImage[j][i] += WEIGHT_SIDE * oldImage[j][left];
                weight[j][i] += WEIGHT_SIDE;
            }

            // Count the cells to the immediate right
            if (i < cols - 1)
            {
                newImage[j][i] += WEIGHT_SIDE * oldImage[j][right];
                weight[j][i] += WEIGHT_SIDE;
            }

            // Count the cells to the bottom left
            if (j < rows - 1 && i > 0)
            {
                newImage[j][i] += WEIGHT_CORNER * oldImage[bot][left];
                weight[j][i] += WEIGHT_CORNER;
            }

            // Count the cells immediately below
            if (j < rows - 1)
            {
                newImage[j][i] += WEIGHT_SIDE * oldImage[bot][i];
                weight[j][i] += WEIGHT_SIDE;
            }

            // Count the cells to the bottom right
            if (j < rows - 1 && i < cols - 1)
            {
                newImage[j][i] += WEIGHT_CORNER * oldImage[bot][right];
                weight[j][i] += WEIGHT_CORNER;
            }
        }
    }
    // Produce the final result
    for (i = 0; i < cols; i++)
        for (j = 0; j < rows; j++)
        {
            newImage[j][i] = newImage[j][i] / weight[j][i];
            check += newImage[j][i];
        }

    return check;
}

/**One loop with branch suggestions
 * OKAY NO DIFFERENCE!?
The optimized implementation took:
        Best   :       318740 usec (ratio: 0.212283)
        Average:       321732 usec (ratio: 0.211305)
 */
static inline long mask3(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    register int i, j;
    register int top, bot, left, right; // TODO maybe not necessary
    register long check = 0;

    long(*weight)[N] = calloc(N * N, sizeof(long));

    //initialize the new image
    for (j = 0; j < rows; j++)
    {
        top = j - 1; // TODO some ternary to make bound checking easier
        bot = j + 1;
        for (i = 0; i < cols; i++)
        {
            left = i - 1;
            right = i + 1;

            // TODO remove weight
            // TODO temp var for image[j][i]
            //initialize the new image
            newImage[j][i] = WEIGHT_CENTRE * oldImage[j][i];
            weight[j][i] = WEIGHT_CENTRE;

            // TODO do borders first, then middle? nah bad cache
            // TODO define cols - 1 and rows - 1?
            // Count the cells to the top left
            if (likely(j > 0 && i > 0))
            {
                newImage[j][i] += WEIGHT_CORNER * oldImage[top][left];
                weight[j][i] += WEIGHT_CORNER;
            }

            // Count the cells immediately above
            if (likely(j > 0))
            {
                newImage[j][i] += WEIGHT_SIDE * oldImage[top][i];
                weight[j][i] += WEIGHT_SIDE;
            }

            // Count the cells to the top right
            if (likely(j > 0 && i < cols - 1))
            {
                newImage[j][i] += WEIGHT_CORNER * oldImage[top][right];
                weight[j][i] += WEIGHT_CORNER;
            }

            // Count the cells to the immediate left
            if (likely(i > 0))
            {
                newImage[j][i] += WEIGHT_SIDE * oldImage[j][left];
                weight[j][i] += WEIGHT_SIDE;
            }

            // Count the cells to the immediate right
            if (likely(i < cols - 1))
            {
                newImage[j][i] += WEIGHT_SIDE * oldImage[j][right];
                weight[j][i] += WEIGHT_SIDE;
            }

            // Count the cells to the bottom left
            if (likely(j < rows - 1 && i > 0))
            {
                newImage[j][i] += WEIGHT_CORNER * oldImage[bot][left];
                weight[j][i] += WEIGHT_CORNER;
            }

            // Count the cells immediately below
            if (likely(j < rows - 1))
            {
                newImage[j][i] += WEIGHT_SIDE * oldImage[bot][i];
                weight[j][i] += WEIGHT_SIDE;
            }

            // Count the cells to the bottom right
            if (likely(j < rows - 1 && i < cols - 1))
            {
                newImage[j][i] += WEIGHT_CORNER * oldImage[bot][right];
                weight[j][i] += WEIGHT_CORNER;
            }
        }
    }
    // Produce the final result
    for (i = 0; i < cols; i++)
        for (j = 0; j < rows; j++)
        {
            newImage[j][i] = newImage[j][i] / weight[j][i];
            check += newImage[j][i];
        }

    return check;
}

/**One loop with branch suggestions
 * with temp variable to store each pixel and then write once
 * with no weight array because it's a waste of time
The optimized implementation took:
        Best   :        78392 usec (ratio: 0.052811)
        Average:        79143 usec (ratio: 0.052280)
 */
static inline long mask4(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    // TODO we got too many register variables meow
    register int i, j;
    register int top, bot, left, right; // TODO maybe not necessary
    register long check = 0;

    //initialize the new image
    for (j = 0; j < rows; j++)
    {
        top = j - 1; // TODO some ternary to make bound checking easier
        bot = j + 1;
        for (i = 0; i < cols; i++)
        {
            left = i - 1;
            right = i + 1;

            // TODO remove weight
            // TODO temp var for image[j][i]
            //initialize the new image
            register long pixel = WEIGHT_CENTRE * oldImage[j][i];
            register long weight = WEIGHT_CENTRE;

            // TODO do borders first, then middle? nah bad cache
            // TODO define cols - 1 and rows - 1?
            // Count the cells to the top left
            if (likely(j > 0 && i > 0))
            {
                pixel += WEIGHT_CORNER * oldImage[top][left];
                weight += WEIGHT_CORNER;
            }

            if (likely(j > 0))
            {
                // Count the cells immediately above
                pixel += WEIGHT_SIDE * oldImage[top][i];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the top right
            if (likely(j > 0 && i < cols - 1))
            {
                pixel += WEIGHT_CORNER * oldImage[top][right];
                weight += WEIGHT_CORNER;
            }

            // Count the cells to the immediate left
            if (likely(i > 0))
            {
                pixel += WEIGHT_SIDE * oldImage[j][left];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the immediate right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_SIDE * oldImage[j][right];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the bottom left
            if (likely(j < rows - 1 && i > 0))
            {
                pixel += WEIGHT_CORNER * oldImage[bot][left];
                weight += WEIGHT_CORNER;
            }

            // Count the cells immediately below
            if (likely(j < rows - 1))
            {
                pixel += WEIGHT_SIDE * oldImage[bot][i];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the bottom right
            if (likely(j < rows - 1 && i < cols - 1))
            {
                pixel += WEIGHT_CORNER * oldImage[bot][right];
                weight += WEIGHT_CORNER;
            }

            // Produce the final result
            pixel /= weight;
            newImage[j][i] = pixel;
            check += pixel;
        }
    }

    return check;
}

/**One loop with branch suggestions
 * with temp variable to store each pixel and then write once
 * with no weight array because it's a waste of time
 * with less ifs
 * 
The optimized implementation took:
        Best   :        69592 usec (ratio: 0.046778)
        Average:        70723 usec (ratio: 0.046515)
 */
static inline long mask5(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    // TODO we got too many register variables meow
    register int i, j;
    register int top, bot, left, right; // TODO maybe not necessary
    register long check = 0;

    //initialize the new image
    for (j = 0; j < rows; j++)
    {
        top = j - 1; // TODO some ternary to make bound checking easier
        bot = j + 1;
        for (i = 0; i < cols; i++)
        {
            left = i - 1;
            right = i + 1;

            // TODO remove weight
            // TODO temp var for image[j][i]
            //initialize the new image
            register long pixel = WEIGHT_CENTRE * oldImage[j][i];
            register long weight = WEIGHT_CENTRE;

            // TODO do borders first, then middle? nah bad cache
            // TODO define cols - 1 and rows - 1?

            // top
            if (likely(j > 0))
            {
                // Count the cells to the top left
                if (likely(i > 0))
                {
                    pixel += WEIGHT_CORNER * oldImage[top][left];
                    weight += WEIGHT_CORNER;
                }

                // Count the cells immediately above
                pixel += WEIGHT_SIDE * oldImage[top][i];
                weight += WEIGHT_SIDE;

                // Count the cells to the top right
                if (likely(i < cols - 1))
                {
                    pixel += WEIGHT_CORNER * oldImage[top][right];
                    weight += WEIGHT_CORNER;
                }
            }

            // Count the cells to the immediate left
            if (likely(i > 0))
            {
                pixel += WEIGHT_SIDE * oldImage[j][left];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the immediate right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_SIDE * oldImage[j][right];
                weight += WEIGHT_SIDE;
            }

            if (likely(j < rows - 1))
            {
                // Count the cells to the bottom left
                if (likely(i > 0))
                {
                    pixel += WEIGHT_CORNER * oldImage[bot][left];
                    weight += WEIGHT_CORNER;
                }

                // Count the cells immediately below
                pixel += WEIGHT_SIDE * oldImage[bot][i];
                weight += WEIGHT_SIDE;

                // Count the cells to the bottom right
                if (likely(i < cols - 1))
                {
                    pixel += WEIGHT_CORNER * oldImage[bot][right];
                    weight += WEIGHT_CORNER;
                }
            }

            // Produce the final result
            pixel /= weight;
            newImage[j][i] = pixel;
            check += pixel;
        }
    }

    return check;
}

/**One loop with branch suggestions
 * with temp variable to store each pixel and then write once
 * with no weight array because it's a waste of time
 * with less ifs
 * with prefetch; tested a variety of different PREFETCH_OFFSET values
 * 
 * Okay this don't do nothin
 * 
The optimized implementation took:
        Best   :        74632 usec (ratio: 0.049999)
        Average:        76175 usec (ratio: 0.050180)
 */
static inline long mask6(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    // TODO we got too many register variables meow
    register int i, j;
    register int top, bot, left, right; // TODO maybe not necessary
    register long check = 0;

    //initialize the new image
    for (j = 0; j < rows; j++)
    {
        top = j - 1; // TODO some ternary to make bound checking easier
        bot = j + 1;
        for (i = 0; i < cols; i++)
        {
            if (likely(i < cols - 1))
                __builtin_prefetch(&oldImage[j][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);

            left = i - 1;
            right = i + 1;

            // TODO remove weight
            // TODO temp var for image[j][i]
            //initialize the new image
            register long pixel = WEIGHT_CENTRE * oldImage[j][i];
            register long weight = WEIGHT_CENTRE;

            // TODO do borders first, then middle? nah bad cache
            // TODO define cols - 1 and rows - 1?

            // top
            if (likely(j > 0))
            {
                // Count the cells to the top left
                if (likely(i > 0))
                {
                    pixel += WEIGHT_CORNER * oldImage[top][left];
                    weight += WEIGHT_CORNER;
                }

                // Count the cells immediately above
                pixel += WEIGHT_SIDE * oldImage[top][i];
                weight += WEIGHT_SIDE;

                // Count the cells to the top right
                if (likely(i < cols - 1))
                {
                    pixel += WEIGHT_CORNER * oldImage[top][right];
                    weight += WEIGHT_CORNER;
                }
            }

            // Count the cells to the immediate left
            if (likely(i > 0))
            {
                pixel += WEIGHT_SIDE * oldImage[j][left];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the immediate right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_SIDE * oldImage[j][right];
                weight += WEIGHT_SIDE;
            }

            if (likely(j < rows - 1))
            {
                // Count the cells to the bottom left
                if (likely(i > 0))
                {
                    pixel += WEIGHT_CORNER * oldImage[bot][left];
                    weight += WEIGHT_CORNER;
                }

                // Count the cells immediately below
                pixel += WEIGHT_SIDE * oldImage[bot][i];
                weight += WEIGHT_SIDE;

                // Count the cells to the bottom right
                if (likely(i < cols - 1))
                {
                    pixel += WEIGHT_CORNER * oldImage[bot][right];
                    weight += WEIGHT_CORNER;
                }
            }

            // Produce the final result
            pixel /= weight;
            newImage[j][i] = pixel;
            check += pixel;
        }
    }

    return check;
}

/**3 loops with branch suggestions
 * with temp variable to store each pixel and then write once
 * with no weight array because it's a waste of time
 * with less ifs
 * with 3 big loops to eliminate some ifs
 * 
The optimized implementation took:
        Best   :        67111 usec (ratio: 0.044862)
        Average:        67702 usec (ratio: 0.044516)
 */
static inline long mask7(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    register int i, j;
    register int top, bot, left, right; // TODO maybe not necessary
    register long check = 0;

    // top row
    bot = 1;
    j = 0;
    for (i = 0; i < cols; i++)
    {
        left = i - 1;
        right = i + 1;

        //initialize the new image
        register long pixel = WEIGHT_CENTRE * oldImage[j][i];
        register long weight = WEIGHT_CENTRE;

        // Count the cells to the immediate left
        if (likely(i > 0))
        {
            pixel += WEIGHT_SIDE * oldImage[j][left];
            weight += WEIGHT_SIDE;
        }

        // Count the cells to the immediate right
        if (likely(i < cols - 1))
        {
            pixel += WEIGHT_SIDE * oldImage[j][right];
            weight += WEIGHT_SIDE;
        }

        // Count the cells to the bottom left
        if (likely(i > 0))
        {
            pixel += WEIGHT_CORNER * oldImage[bot][left];
            weight += WEIGHT_CORNER;
        }

        // Count the cells immediately below
        pixel += WEIGHT_SIDE * oldImage[bot][i];
        weight += WEIGHT_SIDE;

        // Count the cells to the bottom right
        if (likely(i < cols - 1))
        {
            pixel += WEIGHT_CORNER * oldImage[bot][right];
            weight += WEIGHT_CORNER;
        }

        // Produce the final result
        pixel /= weight;
        newImage[j][i] = pixel;
        check += pixel;
    }

    // middle
    for (j = 1; j < rows - 1; j++)
    {
        top = j - 1; // TODO some ternary to make bound checking easier
        bot = j + 1;
        for (i = 0; i < cols; i++)
        {
            left = i - 1;
            right = i + 1;

            //initialize the new image
            register long pixel = WEIGHT_CENTRE * oldImage[j][i];
            register long weight = WEIGHT_CENTRE;

            // Count the cells to the top left
            if (likely(i > 0))
            {
                pixel += WEIGHT_CORNER * oldImage[top][left];
                weight += WEIGHT_CORNER;
            }

            // Count the cells immediately above
            pixel += WEIGHT_SIDE * oldImage[top][i];
            weight += WEIGHT_SIDE;

            // Count the cells to the top right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_CORNER * oldImage[top][right];
                weight += WEIGHT_CORNER;
            }

            // Count the cells to the immediate left
            if (likely(i > 0))
            {
                pixel += WEIGHT_SIDE * oldImage[j][left];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the immediate right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_SIDE * oldImage[j][right];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the bottom left
            if (likely(i > 0))
            {
                pixel += WEIGHT_CORNER * oldImage[bot][left];
                weight += WEIGHT_CORNER;
            }

            // Count the cells immediately below
            pixel += WEIGHT_SIDE * oldImage[bot][i];
            weight += WEIGHT_SIDE;

            // Count the cells to the bottom right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_CORNER * oldImage[bot][right];
                weight += WEIGHT_CORNER;
            }

            // Produce the final result
            pixel /= weight;
            newImage[j][i] = pixel;
            check += pixel;
        }
    }

    // bot row
    top = rows - 2;
    for (i = 0; i < cols; i++)
    {
        left = i - 1;
        right = i + 1;

        //initialize the new image
        register long pixel = WEIGHT_CENTRE * oldImage[j][i];
        register long weight = WEIGHT_CENTRE;

        // Count the cells to the top left
        if (likely(i > 0))
        {
            pixel += WEIGHT_CORNER * oldImage[top][left];
            weight += WEIGHT_CORNER;
        }

        // Count the cells immediately above
        pixel += WEIGHT_SIDE * oldImage[top][i];
        weight += WEIGHT_SIDE;

        // Count the cells to the top right
        if (likely(i < cols - 1))
        {
            pixel += WEIGHT_CORNER * oldImage[top][right];
            weight += WEIGHT_CORNER;
        }

        // Count the cells to the immediate left
        if (likely(i > 0))
        {
            pixel += WEIGHT_SIDE * oldImage[j][left];
            weight += WEIGHT_SIDE;
        }

        // Count the cells to the immediate right
        if (likely(i < cols - 1))
        {
            pixel += WEIGHT_SIDE * oldImage[j][right];
            weight += WEIGHT_SIDE;
        }

        // Produce the final result
        pixel /= weight;
        newImage[j][i] = pixel;
        check += pixel;
    }

    return check;
}

/**Triple nested loop going block column by block column
 * with temp variable to store each pixel and then write once
 * with no weight array because it's a waste of time
 * with less ifs
 * with 3 big loops to eliminate some ifs
 *
 * The optimized implementation took:
        Best   :       106061 usec (ratio: 0.070251)
        Average:       108030 usec (ratio: 0.069941)
 */
static inline long mask8(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    register int i, j, k;
    register int top, bot, left, right; // TODO maybe not necessary
    register long check = 0;

    //initialize the new image
    for (k = 0; k < cols; k += ELEM_PER_CACHE_LINE)
    {

        // top row
        j = 0;
        bot = 1;
        for (i = k; i < k + ELEM_PER_CACHE_LINE; i++)
        {
            left = i - 1;
            right = i + 1;

            //initialize the new image
            register long pixel = WEIGHT_CENTRE * oldImage[j][i];
            register long weight = WEIGHT_CENTRE;

            // Count the cells to the immediate left
            if (likely(i > 0))
            {
                pixel += WEIGHT_SIDE * oldImage[j][left];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the immediate right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_SIDE * oldImage[j][right];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the bottom left
            if (likely(i > 0))
            {
                pixel += WEIGHT_CORNER * oldImage[bot][left];
                weight += WEIGHT_CORNER;
            }

            // Count the cells immediately below
            pixel += WEIGHT_SIDE * oldImage[bot][i];
            weight += WEIGHT_SIDE;

            // Count the cells to the bottom right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_CORNER * oldImage[bot][right];
                weight += WEIGHT_CORNER;
            }

            // Produce the final result
            pixel /= weight;
            newImage[j][i] = pixel;
            check += pixel;
        }

        for (j = 1; j < rows - 1; j++)
        {
            top = j - 1; // TODO some ternary to make bound checking easier
            bot = j + 1;
            for (i = k; i < k + ELEM_PER_CACHE_LINE; i++)
            {
                left = i - 1;
                right = i + 1;

                //initialize the new image
                register long pixel = WEIGHT_CENTRE * oldImage[j][i];
                register long weight = WEIGHT_CENTRE;

                // Count the cells to the top left
                if (likely(i > 0))
                {
                    pixel += WEIGHT_CORNER * oldImage[top][left];
                    weight += WEIGHT_CORNER;
                }

                // Count the cells immediately above
                pixel += WEIGHT_SIDE * oldImage[top][i];
                weight += WEIGHT_SIDE;

                // Count the cells to the top right
                if (likely(i < cols - 1))
                {
                    pixel += WEIGHT_CORNER * oldImage[top][right];
                    weight += WEIGHT_CORNER;
                }

                // Count the cells to the immediate left
                if (likely(i > 0))
                {
                    pixel += WEIGHT_SIDE * oldImage[j][left];
                    weight += WEIGHT_SIDE;
                }

                // Count the cells to the immediate right
                if (likely(i < cols - 1))
                {
                    pixel += WEIGHT_SIDE * oldImage[j][right];
                    weight += WEIGHT_SIDE;
                }

                // Count the cells to the bottom left
                if (likely(i > 0))
                {
                    pixel += WEIGHT_CORNER * oldImage[bot][left];
                    weight += WEIGHT_CORNER;
                }

                // Count the cells immediately below
                pixel += WEIGHT_SIDE * oldImage[bot][i];
                weight += WEIGHT_SIDE;

                // Count the cells to the bottom right
                if (likely(i < cols - 1))
                {
                    pixel += WEIGHT_CORNER * oldImage[bot][right];
                    weight += WEIGHT_CORNER;
                }

                // Produce the final result
                pixel /= weight;
                newImage[j][i] = pixel;
                check += pixel;
            }
        }

        // bot row
        top = rows - 2;
        for (i = k; i < k + ELEM_PER_CACHE_LINE; i++)
        {
            left = i - 1;
            right = i + 1;

            //initialize the new image
            register long pixel = WEIGHT_CENTRE * oldImage[j][i];
            register long weight = WEIGHT_CENTRE;

            // Count the cells to the top left
            if (likely(i > 0))
            {
                pixel += WEIGHT_CORNER * oldImage[top][left];
                weight += WEIGHT_CORNER;
            }

            // Count the cells immediately above
            pixel += WEIGHT_SIDE * oldImage[top][i];
            weight += WEIGHT_SIDE;

            // Count the cells to the top right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_CORNER * oldImage[top][right];
                weight += WEIGHT_CORNER;
            }

            // Count the cells to the immediate left
            if (likely(i > 0))
            {
                pixel += WEIGHT_SIDE * oldImage[j][left];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the immediate right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_SIDE * oldImage[j][right];
                weight += WEIGHT_SIDE;
            }

            // Produce the final result
            pixel /= weight;
            newImage[j][i] = pixel;
            check += pixel;
        }
    }

    return check;
}

/**Triple loop going block column by block column (should be 3x less cache miss but probably bad math somewhere)
 * simple one loop to debug mask9
 *
 * The optimized implementation took:
        Best   :       109966 usec (ratio: 0.073469)
        Average:       111019 usec (ratio: 0.073042)
 *
 */
static inline long mask9(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    register int i, j, k;
    register int top, bot, left, right;
    register long check = 0;

    //initialize the new image
    for (k = 0; k < cols; k += ELEM_PER_CACHE_LINE)
    {
        for (j = 0; j < rows; j++)
        {
            top = j - 1;
            bot = j + 1;
            for (i = k; i < k + ELEM_PER_CACHE_LINE; i++)
            {
                left = i - 1;
                right = i + 1;

                //initialize the new image
                register long pixel = WEIGHT_CENTRE * oldImage[j][i];
                register long weight = WEIGHT_CENTRE;

                // top
                if (likely(j > 0))
                {
                    // Count the cells to the top left
                    if (likely(i > 0))
                    {
                        pixel += WEIGHT_CORNER * oldImage[top][left];
                        weight += WEIGHT_CORNER;
                    }

                    // Count the cells immediately above
                    pixel += WEIGHT_SIDE * oldImage[top][i];
                    weight += WEIGHT_SIDE;

                    // Count the cells to the top right
                    if (likely(i < cols - 1))
                    {
                        pixel += WEIGHT_CORNER * oldImage[top][right];
                        weight += WEIGHT_CORNER;
                    }
                }

                // Count the cells to the immediate left
                if (likely(i > 0))
                {
                    pixel += WEIGHT_SIDE * oldImage[j][left];
                    weight += WEIGHT_SIDE;
                }

                // Count the cells to the immediate right
                if (likely(i < cols - 1))
                {
                    pixel += WEIGHT_SIDE * oldImage[j][right];
                    weight += WEIGHT_SIDE;
                }

                if (likely(j < rows - 1))
                {
                    // Count the cells to the bottom left
                    if (likely(i > 0))
                    {
                        pixel += WEIGHT_CORNER * oldImage[bot][left];
                        weight += WEIGHT_CORNER;
                    }

                    // Count the cells immediately below
                    pixel += WEIGHT_SIDE * oldImage[bot][i];
                    weight += WEIGHT_SIDE;

                    // Count the cells to the bottom right
                    if (likely(i < cols - 1))
                    {
                        pixel += WEIGHT_CORNER * oldImage[bot][right];
                        weight += WEIGHT_CORNER;
                    }
                }

                // Produce the final result
                pixel /= weight;
                newImage[j][i] = pixel;
                check += pixel;
            }
        }
    }

    return check;
}

/**
 * with temp variable to store each pixel and then write once
 * with no weight array because it's a waste of time
 * with less ifs
 * with 3 big loops to eliminate some ifs
 *
 * let's try prefetching again
 *
The optimized implementation took:
        Best   :        69287 usec (ratio: 0.046563)
        Average:        69878 usec (ratio: 0.046083)
 */
static inline long mask10(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    register int i, j;
    register int top, bot, left, right; // TODO maybe not necessary
    register long check = 0;

    // top row
    bot = 1;
    j = 0;
    for (i = 0; i < cols; i++)
    {
        if (unlikely(!(i & 7)))
        {
            __builtin_prefetch(&oldImage[j][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
            __builtin_prefetch(&oldImage[j + 1][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
        }
        left = i - 1;
        right = i + 1;

        //initialize the new image
        register long pixel = WEIGHT_CENTRE * oldImage[j][i];
        register long weight = WEIGHT_CENTRE;

        // Count the cells to the immediate left
        if (likely(i > 0))
        {
            pixel += WEIGHT_SIDE * oldImage[j][left];
            weight += WEIGHT_SIDE;
        }

        // Count the cells to the immediate right
        if (likely(i < cols - 1))
        {
            pixel += WEIGHT_SIDE * oldImage[j][right];
            weight += WEIGHT_SIDE;
        }

        // Count the cells to the bottom left
        if (likely(i > 0))
        {
            pixel += WEIGHT_CORNER * oldImage[bot][left];
            weight += WEIGHT_CORNER;
        }

        // Count the cells immediately below
        pixel += WEIGHT_SIDE * oldImage[bot][i];
        weight += WEIGHT_SIDE;

        // Count the cells to the bottom right
        if (likely(i < cols - 1))
        {
            pixel += WEIGHT_CORNER * oldImage[bot][right];
            weight += WEIGHT_CORNER;
        }

        // Produce the final result
        pixel /= weight;
        newImage[j][i] = pixel;
        check += pixel;
    }

    // middle
    for (j = 1; j < rows - 1; j++)
    {
        top = j - 1; // TODO some ternary to make bound checking easier
        bot = j + 1;
        for (i = 0; i < cols; i++)
        {
            if (unlikely(!(i & 7)))
            {
                __builtin_prefetch(&oldImage[j][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
                __builtin_prefetch(&oldImage[j + 1][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
                __builtin_prefetch(&oldImage[j - 1][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
            }
            left = i - 1;
            right = i + 1;

            //initialize the new image
            register long pixel = WEIGHT_CENTRE * oldImage[j][i];
            register long weight = WEIGHT_CENTRE;

            // Count the cells to the top left
            if (likely(i > 0))
            {
                pixel += WEIGHT_CORNER * oldImage[top][left];
                weight += WEIGHT_CORNER;
            }

            // Count the cells immediately above
            pixel += WEIGHT_SIDE * oldImage[top][i];
            weight += WEIGHT_SIDE;

            // Count the cells to the top right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_CORNER * oldImage[top][right];
                weight += WEIGHT_CORNER;
            }

            // Count the cells to the immediate left
            if (likely(i > 0))
            {
                pixel += WEIGHT_SIDE * oldImage[j][left];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the immediate right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_SIDE * oldImage[j][right];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the bottom left
            if (likely(i > 0))
            {
                pixel += WEIGHT_CORNER * oldImage[bot][left];
                weight += WEIGHT_CORNER;
            }

            // Count the cells immediately below
            pixel += WEIGHT_SIDE * oldImage[bot][i];
            weight += WEIGHT_SIDE;

            // Count the cells to the bottom right
            if (likely(i < cols - 1))
            {
                pixel += WEIGHT_CORNER * oldImage[bot][right];
                weight += WEIGHT_CORNER;
            }

            // Produce the final result
            pixel /= weight;
            newImage[j][i] = pixel;
            check += pixel;
        }
    }

    // bot row
    top = rows - 2;
    for (i = 0; i < cols; i++)
    {
        if (unlikely(!(i & 7)))
        {
            __builtin_prefetch(&oldImage[j][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
            __builtin_prefetch(&oldImage[j - 1][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
        }
        left = i - 1;
        right = i + 1;

        //initialize the new image
        register long pixel = WEIGHT_CENTRE * oldImage[j][i];
        register long weight = WEIGHT_CENTRE;

        // Count the cells to the top left
        if (likely(i > 0))
        {
            pixel += WEIGHT_CORNER * oldImage[top][left];
            weight += WEIGHT_CORNER;
        }

        // Count the cells immediately above
        pixel += WEIGHT_SIDE * oldImage[top][i];
        weight += WEIGHT_SIDE;

        // Count the cells to the top right
        if (likely(i < cols - 1))
        {
            pixel += WEIGHT_CORNER * oldImage[top][right];
            weight += WEIGHT_CORNER;
        }

        // Count the cells to the immediate left
        if (likely(i > 0))
        {
            pixel += WEIGHT_SIDE * oldImage[j][left];
            weight += WEIGHT_SIDE;
        }

        // Count the cells to the immediate right
        if (likely(i < cols - 1))
        {
            pixel += WEIGHT_SIDE * oldImage[j][right];
            weight += WEIGHT_SIDE;
        }

        // Produce the final result
        pixel /= weight;
        newImage[j][i] = pixel;
        check += pixel;
    }

    return check;
}

/**withOUT branch suggestions
 * with temp variable to store each pixel and then write once
 * with no weight array because it's a waste of time
 * with less ifs
 * with 3 big loops to eliminate some ifs
 *
 *
The optimized implementation took:
        Best   :        69287 usec (ratio: 0.046563)
        Average:        69878 usec (ratio: 0.046083)
 */
static inline long mask11(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    register int i, j;
    register int top, bot, left, right; // TODO maybe not necessary
    register long check = 0;

    // top row
    bot = 1;
    j = 0;
    for (i = 0; i < cols; i++)
    {
        left = i - 1;
        right = i + 1;

        //initialize the new image
        register long pixel = WEIGHT_CENTRE * oldImage[j][i];
        register long weight = WEIGHT_CENTRE;

        // Count the cells to the immediate left
        if (i > 0)
        {
            pixel += WEIGHT_SIDE * oldImage[j][left];
            weight += WEIGHT_SIDE;
        }

        // Count the cells to the immediate right
        if (i < cols - 1)
        {
            pixel += WEIGHT_SIDE * oldImage[j][right];
            weight += WEIGHT_SIDE;
        }

        // Count the cells to the bottom left
        if (i > 0)
        {
            pixel += WEIGHT_CORNER * oldImage[bot][left];
            weight += WEIGHT_CORNER;
        }

        // Count the cells immediately below
        pixel += WEIGHT_SIDE * oldImage[bot][i];
        weight += WEIGHT_SIDE;

        // Count the cells to the bottom right
        if (i < cols - 1)
        {
            pixel += WEIGHT_CORNER * oldImage[bot][right];
            weight += WEIGHT_CORNER;
        }

        // Produce the final result
        pixel /= weight;
        newImage[j][i] = pixel;
        check += pixel;
    }

    // middle
    for (j = 1; j < rows - 1; j++)
    {
        top = j - 1; // TODO some ternary to make bound checking easier
        bot = j + 1;
        for (i = 0; i < cols; i++)
        {
            left = i - 1;
            right = i + 1;

            //initialize the new image
            register long pixel = WEIGHT_CENTRE * oldImage[j][i];
            register long weight = WEIGHT_CENTRE;

            // Count the cells to the top left
            if (i > 0)
            {
                pixel += WEIGHT_CORNER * oldImage[top][left];
                weight += WEIGHT_CORNER;
            }

            // Count the cells immediately above
            pixel += WEIGHT_SIDE * oldImage[top][i];
            weight += WEIGHT_SIDE;

            // Count the cells to the top right
            if (i < cols - 1)
            {
                pixel += WEIGHT_CORNER * oldImage[top][right];
                weight += WEIGHT_CORNER;
            }

            // Count the cells to the immediate left
            if (i > 0)
            {
                pixel += WEIGHT_SIDE * oldImage[j][left];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the immediate right
            if (i < cols - 1)
            {
                pixel += WEIGHT_SIDE * oldImage[j][right];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the bottom left
            if (i > 0)
            {
                pixel += WEIGHT_CORNER * oldImage[bot][left];
                weight += WEIGHT_CORNER;
            }

            // Count the cells immediately below
            pixel += WEIGHT_SIDE * oldImage[bot][i];
            weight += WEIGHT_SIDE;

            // Count the cells to the bottom right
            if (i < cols - 1)
            {
                pixel += WEIGHT_CORNER * oldImage[bot][right];
                weight += WEIGHT_CORNER;
            }

            // Produce the final result
            pixel /= weight;
            newImage[j][i] = pixel;
            check += pixel;
        }
    }

    // bot row
    top = rows - 2;
    for (i = 0; i < cols; i++)
    {
        left = i - 1;
        right = i + 1;

        //initialize the new image
        register long pixel = WEIGHT_CENTRE * oldImage[j][i];
        register long weight = WEIGHT_CENTRE;

        // Count the cells to the top left
        if (i > 0)
        {
            pixel += WEIGHT_CORNER * oldImage[top][left];
            weight += WEIGHT_CORNER;
        }

        // Count the cells immediately above
        pixel += WEIGHT_SIDE * oldImage[top][i];
        weight += WEIGHT_SIDE;

        // Count the cells to the top right
        if (i < cols - 1)
        {
            pixel += WEIGHT_CORNER * oldImage[top][right];
            weight += WEIGHT_CORNER;
        }

        // Count the cells to the immediate left
        if (i > 0)
        {
            pixel += WEIGHT_SIDE * oldImage[j][left];
            weight += WEIGHT_SIDE;
        }

        // Count the cells to the immediate right
        if (i < cols - 1)
        {
            pixel += WEIGHT_SIDE * oldImage[j][right];
            weight += WEIGHT_SIDE;
        }

        // Produce the final result
        pixel /= weight;
        newImage[j][i] = pixel;
        check += pixel;
    }

    return check;
}

/**
 * with temp variable to store each pixel and then write once
 * with no weight array because it's a waste of time
 * with less ifs
 * with 3 big loops to eliminate some ifs
 * let's try prefetching
 *
The optimized implementation took:
        Best   :        63315 usec (ratio: 0.042622)
        Average:        63718 usec (ratio: 0.041987)
 */
static inline long mask12(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    register int i, j;
    register int top, bot, left, right; // TODO maybe not necessary
    register long check = 0;

    // top row
    bot = 1;
    j = 0;
    for (i = 0; i < cols; i++)
    {
        if (unlikely(!(i & 7)))
        {
            __builtin_prefetch(&oldImage[j][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
            __builtin_prefetch(&oldImage[j + 1][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
        }
        left = i - 1;
        right = i + 1;

        //initialize the new image
        register long pixel = WEIGHT_CENTRE * oldImage[j][i];
        register long weight = WEIGHT_CENTRE;

        // Count the cells to the immediate left
        if (i > 0)
        {
            pixel += WEIGHT_SIDE * oldImage[j][left];
            weight += WEIGHT_SIDE;
        }

        // Count the cells to the immediate right
        if (i < cols - 1)
        {
            pixel += WEIGHT_SIDE * oldImage[j][right];
            weight += WEIGHT_SIDE;
        }

        // Count the cells to the bottom left
        if (i > 0)
        {
            pixel += WEIGHT_CORNER * oldImage[bot][left];
            weight += WEIGHT_CORNER;
        }

        // Count the cells immediately below
        pixel += WEIGHT_SIDE * oldImage[bot][i];
        weight += WEIGHT_SIDE;

        // Count the cells to the bottom right
        if (i < cols - 1)
        {
            pixel += WEIGHT_CORNER * oldImage[bot][right];
            weight += WEIGHT_CORNER;
        }

        // Produce the final result
        pixel /= weight;
        newImage[j][i] = pixel;
        check += pixel;
    }

    // middle
    for (j = 1; j < rows - 1; j++)
    {
        top = j - 1; // TODO some ternary to make bound checking easier
        bot = j + 1;
        for (i = 0; i < cols; i++)
        {
            if (!(i & 7))
            {
                __builtin_prefetch(&oldImage[j][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
                __builtin_prefetch(&oldImage[j + 1][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
                __builtin_prefetch(&oldImage[j - 1][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
            }
            left = i - 1;
            right = i + 1;

            //initialize the new image
            register long pixel = WEIGHT_CENTRE * oldImage[j][i];
            register long weight = WEIGHT_CENTRE;

            // Count the cells to the top left
            if (i > 0)
            {
                pixel += WEIGHT_CORNER * oldImage[top][left];
                weight += WEIGHT_CORNER;
            }

            // Count the cells immediately above
            pixel += WEIGHT_SIDE * oldImage[top][i];
            weight += WEIGHT_SIDE;

            // Count the cells to the top right
            if (i < cols - 1)
            {
                pixel += WEIGHT_CORNER * oldImage[top][right];
                weight += WEIGHT_CORNER;
            }

            // Count the cells to the immediate left
            if (i > 0)
            {
                pixel += WEIGHT_SIDE * oldImage[j][left];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the immediate right
            if (i < cols - 1)
            {
                pixel += WEIGHT_SIDE * oldImage[j][right];
                weight += WEIGHT_SIDE;
            }

            // Count the cells to the bottom left
            if (i > 0)
            {
                pixel += WEIGHT_CORNER * oldImage[bot][left];
                weight += WEIGHT_CORNER;
            }

            // Count the cells immediately below
            pixel += WEIGHT_SIDE * oldImage[bot][i];
            weight += WEIGHT_SIDE;

            // Count the cells to the bottom right
            if (i < cols - 1)
            {
                pixel += WEIGHT_CORNER * oldImage[bot][right];
                weight += WEIGHT_CORNER;
            }

            // Produce the final result
            pixel /= weight;
            newImage[j][i] = pixel;
            check += pixel;
        }
    }

    // bot row
    top = rows - 2;
    for (i = 0; i < cols; i++)
    {
        if (!(i & 7))
        {
            __builtin_prefetch(&oldImage[j][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
            __builtin_prefetch(&oldImage[j - 1][i + PREFETCH_OFFSET], 0, PREFETCH_LOCALITY);
        }
        left = i - 1;
        right = i + 1;

        //initialize the new image
        register long pixel = WEIGHT_CENTRE * oldImage[j][i];
        register long weight = WEIGHT_CENTRE;

        // Count the cells to the top left
        if (i > 0)
        {
            pixel += WEIGHT_CORNER * oldImage[top][left];
            weight += WEIGHT_CORNER;
        }

        // Count the cells immediately above
        pixel += WEIGHT_SIDE * oldImage[top][i];
        weight += WEIGHT_SIDE;

        // Count the cells to the top right
        if (i < cols - 1)
        {
            pixel += WEIGHT_CORNER * oldImage[top][right];
            weight += WEIGHT_CORNER;
        }

        // Count the cells to the immediate left
        if (i > 0)
        {
            pixel += WEIGHT_SIDE * oldImage[j][left];
            weight += WEIGHT_SIDE;
        }

        // Count the cells to the immediate right
        if (i < cols - 1)
        {
            pixel += WEIGHT_SIDE * oldImage[j][right];
            weight += WEIGHT_SIDE;
        }

        // Produce the final result
        pixel /= weight;
        newImage[j][i] = pixel;
        check += pixel;
    }

    return check;
}

/**
 * with temp variable to store each pixel and then write once
 * with no weight array because it's a waste of time
 * with less ifs
 * with 3 big loops to eliminate some ifs
 *
 * combine some expressions
 *
The optimized implementation took:
        Best   :        57526 usec (ratio: 0.038610)
        Average:        58103 usec (ratio: 0.038300)
 */
static inline long mask13(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    register int i, j;
    register int top, bot, left, right; // TODO maybe not necessary
    register long check = 0;

    // top row
    bot = 1;
    j = 0;
    for (i = 0; i < cols; i++)
    {
        left = i - 1;
        right = i + 1;

        register long pixel;
        register long weight;

        // Count the cells immediately below and center
        pixel = WEIGHT_CENTRE * oldImage[j][i] + WEIGHT_SIDE * oldImage[bot][i];
        weight = WEIGHT_CENTRE + WEIGHT_SIDE;

        // Count the cells to the immediate left and bottom left
        if (i > 0)
        {
            pixel += WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[bot][left];
            weight += WEIGHT_SIDE + WEIGHT_CORNER;
        }

        // Count the cells to the immediate right and bottom right
        if (i < cols - 1)
        {
            pixel += WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[bot][right];
            weight += WEIGHT_SIDE + WEIGHT_CORNER;
        }

        // Produce the final result
        pixel /= weight;
        newImage[j][i] = pixel;
        check += pixel;
    }

    // middle
    for (j = 1; j < rows - 1; j++)
    {
        top = j - 1; // TODO some ternary to make bound checking easier
        bot = j + 1;
        for (i = 0; i < cols; i++)
        {
            left = i - 1;
            right = i + 1;

            register long pixel;
            register long weight;

            // Count the cells immediately below and center and top
            pixel = WEIGHT_CENTRE * oldImage[j][i] +
                    WEIGHT_SIDE * oldImage[bot][i] +
                    WEIGHT_SIDE * oldImage[top][i];
            weight = WEIGHT_CENTRE + WEIGHT_SIDE + WEIGHT_SIDE;

            // Count the cells to the immediate left and bottom left and top left
            if (i > 0)
            {
                pixel += WEIGHT_SIDE * oldImage[j][left] +
                         WEIGHT_CORNER * oldImage[bot][left] +
                         WEIGHT_CORNER * oldImage[top][left];
                weight += WEIGHT_SIDE + WEIGHT_CORNER + WEIGHT_CORNER;
            }

            // Count the cells to the immediate right and bottom right and top right
            if (i < cols - 1)
            {
                pixel += WEIGHT_SIDE * oldImage[j][right] +
                         WEIGHT_CORNER * oldImage[bot][right] +
                         WEIGHT_CORNER * oldImage[top][right];
                weight += WEIGHT_SIDE + WEIGHT_CORNER + WEIGHT_CORNER;
            }

            // Produce the final result
            pixel /= weight;
            newImage[j][i] = pixel;
            check += pixel;
        }
    }

    // bot row
    top = rows - 2;
    for (i = 0; i < cols; i++)
    {
        left = i - 1;
        right = i + 1;

        register long pixel;
        register long weight;

        // Count the cells center and top
        pixel = WEIGHT_CENTRE * oldImage[j][i] +
                WEIGHT_SIDE * oldImage[top][i];
        weight = WEIGHT_CENTRE + WEIGHT_SIDE;

        // Count the cells to the immediate left and top left
        if (i > 0)
        {
            pixel += WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[top][left];
            weight += WEIGHT_SIDE + WEIGHT_CORNER;
        }

        // Count the cells to the immediate right and top right
        if (i < cols - 1)
        {
            pixel += WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[top][right];
            weight += WEIGHT_SIDE + WEIGHT_CORNER;
        }

        // Produce the final result
        pixel /= weight;
        newImage[j][i] = pixel;
        check += pixel;
    }

    return check;
}

/**
 * with temp variable to store each pixel and then write once
 * NO IFS
 * NO WEIGHT VARIABLE
 * ALL CALCULATIONS IN ONE LINE
 *
The optimized implementation took:
        Best   :        25025 usec (ratio: 0.016830)
        Average:        25659 usec (ratio: 0.016484)
 */
static inline long mask14(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    register int i, j;
    register int top, bot, left, right; // TODO maybe not necessary
    register long check = 0;

    register long pixel;

    // TOP ROW ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // top left pixel; center, bot, right, bot right
    pixel = (WEIGHT_CENTRE * oldImage[0][0] +
             WEIGHT_SIDE * oldImage[1][0] +
             WEIGHT_SIDE * oldImage[0][1] +
             WEIGHT_CORNER * oldImage[1][1]) /
            WEIGHT_CORNER_TOTAL;

    // Produce the final result
    newImage[0][0] = pixel;
    check += pixel;

    // top row
    bot = 1;
    j = 0;
    for (i = 1; i < cols - 1; i++)
    {
        left = i - 1;
        right = i + 1;

        // Count the cells center, bot, left, bot left, right, bot right
        pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                 WEIGHT_SIDE * oldImage[bot][i] +
                 WEIGHT_SIDE * oldImage[j][left] +
                 WEIGHT_CORNER * oldImage[bot][left] +
                 WEIGHT_SIDE * oldImage[j][right] +
                 WEIGHT_CORNER * oldImage[bot][right]) /
                WEIGHT_EDGE_TOTAL;

        // Produce the final result
        newImage[j][i] = pixel;
        check += pixel;
    }

    // top right pixel
    pixel = (WEIGHT_CENTRE * oldImage[0][cols - 1] +
             WEIGHT_SIDE * oldImage[1][cols - 1] +
             WEIGHT_SIDE * oldImage[0][cols - 2] +
             WEIGHT_CORNER * oldImage[1][cols - 2]) /
            WEIGHT_CORNER_TOTAL;

    // Produce the final result
    newImage[0][cols - 1] = pixel;
    check += pixel;

    // MIDDLE ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    for (j = 1; j < rows - 1; j++)
    {
        top = j - 1;
        bot = j + 1;

        // left pixel; center, bot, right, bot right, top, top right
        pixel = (WEIGHT_CENTRE * oldImage[j][0] +
                 WEIGHT_SIDE * oldImage[bot][0] +
                 WEIGHT_SIDE * oldImage[j][1] +
                 WEIGHT_CORNER * oldImage[bot][1] +
                 WEIGHT_SIDE * oldImage[top][0] +
                 WEIGHT_CORNER * oldImage[top][1]) /
                WEIGHT_EDGE_TOTAL;

        // Produce the final result
        newImage[j][0] = pixel;
        check += pixel;

        for (i = 1; i < cols - 1; i++)
        {
            left = i - 1;
            right = i + 1;

            // all 9
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[bot][i] +
                     WEIGHT_SIDE * oldImage[top][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[bot][left] +
                     WEIGHT_CORNER * oldImage[top][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[bot][right] +
                     WEIGHT_CORNER * oldImage[top][right]) /
                    WEIGHT_CENTER_TOTAL;

            // Produce the final result
            newImage[j][i] = pixel;
            check += pixel;
        }

        // right pixel
        pixel = (WEIGHT_CENTRE * oldImage[j][cols - 1] +
                 WEIGHT_SIDE * oldImage[bot][cols - 1] +
                 WEIGHT_SIDE * oldImage[j][cols - 2] +
                 WEIGHT_CORNER * oldImage[bot][cols - 2] +
                 WEIGHT_SIDE * oldImage[top][cols - 1] +
                 WEIGHT_CORNER * oldImage[top][cols - 2]) /
                WEIGHT_EDGE_TOTAL;

        // Produce the final result
        newImage[j][cols - 1] = pixel;
        check += pixel;
    }

    // BOW ROW ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    top = rows - 2;

    // bot left pixel; center, right, top, top right
    pixel = (WEIGHT_CENTRE * oldImage[j][0] +
             WEIGHT_SIDE * oldImage[j][1] +
             WEIGHT_SIDE * oldImage[top][0] +
             WEIGHT_CORNER * oldImage[top][1]) /
            WEIGHT_CORNER_TOTAL;

    // Produce the final result
    newImage[j][0] = pixel;
    check += pixel;

    // bot row
    for (i = 1; i < cols - 1; i++)
    {
        left = i - 1;
        right = i + 1;

        // all 6
        pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                 WEIGHT_SIDE * oldImage[top][i] +
                 WEIGHT_SIDE * oldImage[j][left] +
                 WEIGHT_CORNER * oldImage[top][left] +
                 WEIGHT_SIDE * oldImage[j][right] +
                 WEIGHT_CORNER * oldImage[top][right]) /
                WEIGHT_EDGE_TOTAL;

        // Produce the final result
        newImage[j][i] = pixel;
        check += pixel;
    }

    // bot right pixel
    pixel = (WEIGHT_CENTRE * oldImage[j][cols - 1] +
             WEIGHT_SIDE * oldImage[j][cols - 2] +
             WEIGHT_SIDE * oldImage[top][cols - 1] +
             WEIGHT_CORNER * oldImage[top][cols - 2]) /
            WEIGHT_CORNER_TOTAL;

    // Produce the final result
    newImage[j][i] = pixel;
    check += pixel;

    return check;
}

/**
 * with temp variable to store each pixel and then write once
 * NO IFS
 * NO WEIGHT VARIABLE
 * ALL CALCULATIONS IN ONE LINE
 *
 * MANUALLY UNROLL LOOP INTO 8 SEQUENTIAL TO REDUCE WRONG BRANCH PREDICTION, LESS ASSEMBLY INSTRUCTIONS MAYBE
 * Duffs Device to handle image size col not multiple of 8
 * 
 * wait why isn't this better than 14
 *
The optimized implementation took:
        Best   :        24801 usec (ratio: 0.016770)
        Average:        25267 usec (ratio: 0.016711)
 */
static inline long mask15(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    register int i, j, n;
    register int n_precalculate = (cols + 5) / 8; // division is slow to do each loop
    register int top, bot, left, right;
    register long check = 0;

    register long pixel;

    // TOP ROW ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // top left pixel; center, bot, right, bot right
    pixel = (WEIGHT_CENTRE * oldImage[0][0] +
             WEIGHT_SIDE * oldImage[1][0] +
             WEIGHT_SIDE * oldImage[0][1] +
             WEIGHT_CORNER * oldImage[1][1]) /
            WEIGHT_CORNER_TOTAL;

    // Produce the final result
    newImage[0][0] = pixel;
    check += pixel;

    // top row
    bot = 1;
    i = 1;
    j = 0;
    n = n_precalculate;
    switch ((cols - 2) & 7)
    {
    case 0:
        do
        {
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[bot][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[bot][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[bot][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 7:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[bot][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[bot][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[bot][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 6:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[bot][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[bot][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[bot][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 5:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[bot][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[bot][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[bot][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 4:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[bot][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[bot][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[bot][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 3:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[bot][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[bot][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[bot][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 2:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[bot][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[bot][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[bot][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 1:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[bot][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[bot][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[bot][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        } while (--n > 0);
    }

    // top right pixel
    pixel = (WEIGHT_CENTRE * oldImage[0][cols - 1] +
             WEIGHT_SIDE * oldImage[1][cols - 1] +
             WEIGHT_SIDE * oldImage[0][cols - 2] +
             WEIGHT_CORNER * oldImage[1][cols - 2]) /
            WEIGHT_CORNER_TOTAL;

    // Produce the final result
    newImage[0][cols - 1] = pixel;
    check += pixel;

    // MIDDLE ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    for (j = 1; j < rows - 1; j++)
    {
        top = j - 1;
        bot = j + 1;

        // left pixel; center, bot, right, bot right, top, top right
        pixel = (WEIGHT_CENTRE * oldImage[j][0] +
                 WEIGHT_SIDE * oldImage[bot][0] +
                 WEIGHT_SIDE * oldImage[j][1] +
                 WEIGHT_CORNER * oldImage[bot][1] +
                 WEIGHT_SIDE * oldImage[top][0] +
                 WEIGHT_CORNER * oldImage[top][1]) /
                WEIGHT_EDGE_TOTAL;

        // Produce the final result
        newImage[j][0] = pixel;
        check += pixel;

        i = 1;
        n = n_precalculate;
        switch ((cols - 2) & 7)
        {
        case 0:
            do
            {
                left = i - 1;
                right = i + 1;
                pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                         WEIGHT_SIDE * oldImage[bot][i] +
                         WEIGHT_SIDE * oldImage[top][i] +
                         WEIGHT_SIDE * oldImage[j][left] +
                         WEIGHT_CORNER * oldImage[bot][left] +
                         WEIGHT_CORNER * oldImage[top][left] +
                         WEIGHT_SIDE * oldImage[j][right] +
                         WEIGHT_CORNER * oldImage[bot][right] +
                         WEIGHT_CORNER * oldImage[top][right]) /
                        WEIGHT_CENTER_TOTAL;
                newImage[j][i++] = pixel;
                check += pixel;
            case 7:
                left = i - 1;
                right = i + 1;
                pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                         WEIGHT_SIDE * oldImage[bot][i] +
                         WEIGHT_SIDE * oldImage[top][i] +
                         WEIGHT_SIDE * oldImage[j][left] +
                         WEIGHT_CORNER * oldImage[bot][left] +
                         WEIGHT_CORNER * oldImage[top][left] +
                         WEIGHT_SIDE * oldImage[j][right] +
                         WEIGHT_CORNER * oldImage[bot][right] +
                         WEIGHT_CORNER * oldImage[top][right]) /
                        WEIGHT_CENTER_TOTAL;
                newImage[j][i++] = pixel;
                check += pixel;

            case 6:
                left = i - 1;
                right = i + 1;
                pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                         WEIGHT_SIDE * oldImage[bot][i] +
                         WEIGHT_SIDE * oldImage[top][i] +
                         WEIGHT_SIDE * oldImage[j][left] +
                         WEIGHT_CORNER * oldImage[bot][left] +
                         WEIGHT_CORNER * oldImage[top][left] +
                         WEIGHT_SIDE * oldImage[j][right] +
                         WEIGHT_CORNER * oldImage[bot][right] +
                         WEIGHT_CORNER * oldImage[top][right]) /
                        WEIGHT_CENTER_TOTAL;
                newImage[j][i++] = pixel;
                check += pixel;

            case 5:
                left = i - 1;
                right = i + 1;
                pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                         WEIGHT_SIDE * oldImage[bot][i] +
                         WEIGHT_SIDE * oldImage[top][i] +
                         WEIGHT_SIDE * oldImage[j][left] +
                         WEIGHT_CORNER * oldImage[bot][left] +
                         WEIGHT_CORNER * oldImage[top][left] +
                         WEIGHT_SIDE * oldImage[j][right] +
                         WEIGHT_CORNER * oldImage[bot][right] +
                         WEIGHT_CORNER * oldImage[top][right]) /
                        WEIGHT_CENTER_TOTAL;
                newImage[j][i++] = pixel;
                check += pixel;

            case 4:
                left = i - 1;
                right = i + 1;
                pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                         WEIGHT_SIDE * oldImage[bot][i] +
                         WEIGHT_SIDE * oldImage[top][i] +
                         WEIGHT_SIDE * oldImage[j][left] +
                         WEIGHT_CORNER * oldImage[bot][left] +
                         WEIGHT_CORNER * oldImage[top][left] +
                         WEIGHT_SIDE * oldImage[j][right] +
                         WEIGHT_CORNER * oldImage[bot][right] +
                         WEIGHT_CORNER * oldImage[top][right]) /
                        WEIGHT_CENTER_TOTAL;
                newImage[j][i++] = pixel;
                check += pixel;

            case 3:
                left = i - 1;
                right = i + 1;
                pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                         WEIGHT_SIDE * oldImage[bot][i] +
                         WEIGHT_SIDE * oldImage[top][i] +
                         WEIGHT_SIDE * oldImage[j][left] +
                         WEIGHT_CORNER * oldImage[bot][left] +
                         WEIGHT_CORNER * oldImage[top][left] +
                         WEIGHT_SIDE * oldImage[j][right] +
                         WEIGHT_CORNER * oldImage[bot][right] +
                         WEIGHT_CORNER * oldImage[top][right]) /
                        WEIGHT_CENTER_TOTAL;
                newImage[j][i++] = pixel;
                check += pixel;

            case 2:
                left = i - 1;
                right = i + 1;
                pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                         WEIGHT_SIDE * oldImage[bot][i] +
                         WEIGHT_SIDE * oldImage[top][i] +
                         WEIGHT_SIDE * oldImage[j][left] +
                         WEIGHT_CORNER * oldImage[bot][left] +
                         WEIGHT_CORNER * oldImage[top][left] +
                         WEIGHT_SIDE * oldImage[j][right] +
                         WEIGHT_CORNER * oldImage[bot][right] +
                         WEIGHT_CORNER * oldImage[top][right]) /
                        WEIGHT_CENTER_TOTAL;
                newImage[j][i++] = pixel;
                check += pixel;

            case 1:
                left = i - 1;
                right = i + 1;
                pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                         WEIGHT_SIDE * oldImage[bot][i] +
                         WEIGHT_SIDE * oldImage[top][i] +
                         WEIGHT_SIDE * oldImage[j][left] +
                         WEIGHT_CORNER * oldImage[bot][left] +
                         WEIGHT_CORNER * oldImage[top][left] +
                         WEIGHT_SIDE * oldImage[j][right] +
                         WEIGHT_CORNER * oldImage[bot][right] +
                         WEIGHT_CORNER * oldImage[top][right]) /
                        WEIGHT_CENTER_TOTAL;
                newImage[j][i++] = pixel;
                check += pixel;

            } while (--n > 0);
        }

        // right pixel
        pixel = (WEIGHT_CENTRE * oldImage[j][cols - 1] +
                 WEIGHT_SIDE * oldImage[bot][cols - 1] +
                 WEIGHT_SIDE * oldImage[j][cols - 2] +
                 WEIGHT_CORNER * oldImage[bot][cols - 2] +
                 WEIGHT_SIDE * oldImage[top][cols - 1] +
                 WEIGHT_CORNER * oldImage[top][cols - 2]) /
                WEIGHT_EDGE_TOTAL;

        // Produce the final result
        newImage[j][cols - 1] = pixel;
        check += pixel;
    }

    // BOW ROW ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    top = rows - 2;

    // bot left pixel; center, right, top, top right
    pixel = (WEIGHT_CENTRE * oldImage[j][0] +
             WEIGHT_SIDE * oldImage[j][1] +
             WEIGHT_SIDE * oldImage[top][0] +
             WEIGHT_CORNER * oldImage[top][1]) /
            WEIGHT_CORNER_TOTAL;

    // Produce the final result
    newImage[j][0] = pixel;
    check += pixel;

    // bot row
    i = 1;
    n = n_precalculate;
    switch ((cols - 2) & 7)
    {
    case 0:
        do
        {
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[top][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[top][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[top][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 7:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[top][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[top][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[top][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 6:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[top][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[top][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[top][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 5:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[top][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[top][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[top][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 4:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[top][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[top][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[top][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 3:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[top][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[top][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[top][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        case 2:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[top][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[top][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[top][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;
        case 1:
            left = i - 1;
            right = i + 1;
            pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                     WEIGHT_SIDE * oldImage[top][i] +
                     WEIGHT_SIDE * oldImage[j][left] +
                     WEIGHT_CORNER * oldImage[top][left] +
                     WEIGHT_SIDE * oldImage[j][right] +
                     WEIGHT_CORNER * oldImage[top][right]) /
                    WEIGHT_EDGE_TOTAL;
            newImage[j][i++] = pixel;
            check += pixel;

        } while (--n > 0);
    }
    // bot right pixel
    pixel = (WEIGHT_CENTRE * oldImage[j][cols - 1] +
             WEIGHT_SIDE * oldImage[j][cols - 2] +
             WEIGHT_SIDE * oldImage[top][cols - 1] +
             WEIGHT_CORNER * oldImage[top][cols - 2]) /
            WEIGHT_CORNER_TOTAL;

    // Produce the final result
    newImage[j][i] = pixel;
    check += pixel;

    return check;
}

// ====================== Multithreaded Implementation =========================

struct params
{
    long (*oldImage)[N];
    long (*newImage)[N];
    int rows;
    int cols;
    int curRow;
    long *check;
};

void *blurTopRow(void *params);
void *blurMiddleRows(void *params);
void *blurBottomRow(void *params);

pthread_mutex_t check_mlock;

/**
 * MULTITHREADED
 * 
 * with temp variable to store each pixel and then write once
 * NO IFS
 * NO WEIGHT VARIABLE
 * ALL CALCULATIONS IN ONE LINE
 *
 */
static inline long mask16(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    long check = 0;
    register long pixel;

    if (pthread_mutex_init(&check_mlock, NULL) != 0)
    {
        printf("failed to initialize check_mlock");
        return -1;
    }

    // TOP ROW ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    pthread_t threadTop;
    struct params paramsTop = {oldImage, newImage, rows, cols, 0, &check};
    pthread_create(&threadTop, NULL, blurTopRow, (void *)&paramsTop);

    // MIDDLE ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    pthread_t threadMiddleArr[rows - 2];
    struct params paramsMiddleArr[rows - 2];
    for (j = 1; j < rows - 1; j++)
    {
        paramsMiddleArr[j - 1] = {oldImage, newImage, rows, cols, j, &check};
        pthread_create(&threadMiddleArr[j], NULL, blurMiddleRows, (void *)&paramsMiddleArr[j - 1]);
    }

    // BOW ROW ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    

    return check;
}

void *blurTopRow(void *params_v)
{
    register int i, j;
    register int bot, left, right;
    register long tempCheck = 0;
    register long pixel;

    struct params *params_ptr = (struct params *)params_v;

    // top left pixel; center, bot, right, bot right
    pixel = (WEIGHT_CENTRE * params_ptr->oldImage[0][0] +
             WEIGHT_SIDE * params_ptr->oldImage[1][0] +
             WEIGHT_SIDE * params_ptr->oldImage[0][1] +
             WEIGHT_CORNER * params_ptr->oldImage[1][1]) /
            WEIGHT_CORNER_TOTAL;

    // Produce the final result
    params_ptr->newImage[0][0] = pixel;
    tempCheck += pixel;

    // top row
    bot = 1;
    j = 0;
    for (i = 1; i < params_ptr->cols - 1; i++)
    {
        left = i - 1;
        right = i + 1;

        // Count the cells center, bot, left, bot left, right, bot right
        pixel = (WEIGHT_CENTRE * params_ptr->oldImage[j][i] +
                 WEIGHT_SIDE * params_ptr->oldImage[bot][i] +
                 WEIGHT_SIDE * params_ptr->oldImage[j][left] +
                 WEIGHT_CORNER * params_ptr->oldImage[bot][left] +
                 WEIGHT_SIDE * params_ptr->oldImage[j][right] +
                 WEIGHT_CORNER * params_ptr->oldImage[bot][right]) /
                WEIGHT_EDGE_TOTAL;

        // Produce the final result
        params_ptr->newImage[j][i] = pixel;
        tempCheck += pixel;
    }

    // top right pixel
    pixel = (WEIGHT_CENTRE * params_ptr->oldImage[0][params_ptr->cols - 1] +
             WEIGHT_SIDE * params_ptr->oldImage[1][params_ptr->cols - 1] +
             WEIGHT_SIDE * params_ptr->oldImage[0][params_ptr->cols - 2] +
             WEIGHT_CORNER * params_ptr->oldImage[1][params_ptr->cols - 2]) /
            WEIGHT_CORNER_TOTAL;

    // Produce the final result
    params_ptr->newImage[0][params_ptr->cols - 1] = pixel;

    pthread_mutex_lock(&check_mlock);
    *params_ptr->check += tempCheck + pixel;
    pthread_mutex_unlock(&check_mlock);

    return 0;
}

void *blurMiddleRows(void *params)
{
    struct params *params_ptr = (struct params *)params_v;

    register int top = params_ptr->curRow - 1;
    register int bot = params_ptr->curRow + 1;
    register long tempCheck = 0;

    // left pixel; center, bot, right, bot right, top, top right
    pixel = (WEIGHT_CENTRE * params_ptr->oldImage[params_ptr->curRow][0] +
             WEIGHT_SIDE * params_ptr->oldImage[bot][0] +
             WEIGHT_SIDE * params_ptr->oldImage[params_ptr->curRow][1] +
             WEIGHT_CORNER * params_ptr->oldImage[bot][1] +
             WEIGHT_SIDE * params_ptr->oldImage[top][0] +
             WEIGHT_CORNER * params_ptr->oldImage[top][1]) /
            WEIGHT_EDGE_TOTAL;

    // Produce the final result
    params_ptr->newImage[params_ptr->curRow][0] = pixel;
    tempCheck += pixel;

    for (int i = 1; i < params_ptr->cols - 1; i++)
    {
        register int left = i - 1;
        register int right = i + 1;

        // all 9
        pixel = (WEIGHT_CENTRE * params_ptr->oldImage[params_ptr->curRow][i] +
                 WEIGHT_SIDE * params_ptr->oldImage[bot][i] +
                 WEIGHT_SIDE * params_ptr->oldImage[top][i] +
                 WEIGHT_SIDE * params_ptr->oldImage[params_ptr->curRow][left] +
                 WEIGHT_CORNER * params_ptr->oldImage[bot][left] +
                 WEIGHT_CORNER * params_ptr->oldImage[top][left] +
                 WEIGHT_SIDE * params_ptr->oldImage[params_ptr->curRow][right] +
                 WEIGHT_CORNER * params_ptr->oldImage[bot][right] +
                 WEIGHT_CORNER * params_ptr->oldImage[top][right]) /
                WEIGHT_CENTER_TOTAL;

        // Produce the final result
        params_ptr->newImage[params_ptr->curRow][i] = pixel;
        tempCheck += pixel;
    }

    // right pixel
    pixel = (WEIGHT_CENTRE * params_ptr->oldImage[params_ptr->curRow][params_ptr->cols - 1] +
             WEIGHT_SIDE * params_ptr->oldImage[bot][params_ptr->cols - 1] +
             WEIGHT_SIDE * params_ptr->oldImage[params_ptr->curRow][params_ptr->cols - 2] +
             WEIGHT_CORNER * params_ptr->oldImage[bot][params_ptr->cols - 2] +
             WEIGHT_SIDE * params_ptr->oldImage[top][params_ptr->cols - 1] +
             WEIGHT_CORNER * params_ptr->oldImage[top][params_ptr->cols - 2]) /
            WEIGHT_EDGE_TOTAL;

    // Produce the final result
    params_ptr->newImage[params_ptr->curRow][cols - 1] = pixel;

    pthread_mutex_lock(&check_mlock);
    *params_ptr->check += tempCheck + pixel;
    pthread_mutex_unlock(&check_mlock);
}
void *blurBottomRow(void *params) {
    top = rows - 2;

    // bot left pixel; center, right, top, top right
    pixel = (WEIGHT_CENTRE * oldImage[j][0] +
             WEIGHT_SIDE * oldImage[j][1] +
             WEIGHT_SIDE * oldImage[top][0] +
             WEIGHT_CORNER * oldImage[top][1]) /
            WEIGHT_CORNER_TOTAL;

    // Produce the final result
    newImage[j][0] = pixel;
    check += pixel;

    // bot row
    for (i = 1; i < cols - 1; i++)
    {
        left = i - 1;
        right = i + 1;

        // all 6
        pixel = (WEIGHT_CENTRE * oldImage[j][i] +
                 WEIGHT_SIDE * oldImage[top][i] +
                 WEIGHT_SIDE * oldImage[j][left] +
                 WEIGHT_CORNER * oldImage[top][left] +
                 WEIGHT_SIDE * oldImage[j][right] +
                 WEIGHT_CORNER * oldImage[top][right]) /
                WEIGHT_EDGE_TOTAL;

        // Produce the final result
        newImage[j][i] = pixel;
        check += pixel;
    }

    // bot right pixel
    pixel = (WEIGHT_CENTRE * oldImage[j][cols - 1] +
             WEIGHT_SIDE * oldImage[j][cols - 2] +
             WEIGHT_SIDE * oldImage[top][cols - 1] +
             WEIGHT_CORNER * oldImage[top][cols - 2]) /
            WEIGHT_CORNER_TOTAL;

    // Produce the final result
    newImage[j][i] = pixel;
    check += pixel;
}

long mask(long oldImage[N][N], long newImage[N][N], int rows, int cols)
{
    return MASK_VERSION(oldImage, newImage, rows, cols);
}
