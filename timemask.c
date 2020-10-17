#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "mask.h"
#include <unistd.h>
#include <limits.h>
#include <ctype.h>

#define VERBOSE 1
#define PARANOID 1

//Various data spaces to hold different images

long baseImage[N][N], baseAnswer[N][N], trialImage[N][N], trialAnswer[N][N];

static long usecClock(struct timeval *start_ts, struct timeval *end_ts)
{
  struct timeval res;
  timersub(end_ts, start_ts, &res);
  return res.tv_usec + 1000000 * res.tv_sec;
}

static void printImage(long image[N][N], int rows, int cols)
{
  int i, j;

  for (i = 0; i < N; i++)
  {
    for (j = 0; j < N; j++)
    {
      printf("%ld ", image[i][j]);
    }
    printf("\n");
  }
  printf("-----------------------------------\n");
}

// The basic, not so smart implementation of the routine that
// takes an existing image and applies the mask to it.
long baseMask(long oldImage[N][N], long newImage[N][N], int rows, int cols)
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
  for (i = 1; i < cols; i++)
  {
    col = i - 1;
    for (j = 1; j < rows; j++)
    {
      row = j - 1;
      newImage[j][i] += WEIGHT_CORNER * oldImage[row][col];
      weight[j][i] += WEIGHT_CORNER;
    }
  }

  // Count the cells immediately above
  for (i = 0; i < cols; i++)
  {
    for (j = 1; j < rows; j++)
    {
      row = j - 1;
      newImage[j][i] += WEIGHT_SIDE * oldImage[row][i];
      weight[j][i] += WEIGHT_SIDE;
    }
  }

  // Count the cells to the top right
  for (i = 0; i < cols - 1; i++)
  {
    col = i + 1;
    for (j = 1; j < rows; j++)
    {
      row = j - 1;
      newImage[j][i] += WEIGHT_CORNER * oldImage[row][col];
      weight[j][i] += WEIGHT_CORNER;
    }
  }

  // Count the cells to the immediate left
  for (i = 1; i < cols; i++)
  {
    col = i - 1;
    for (j = 0; j < rows; j++)
    {
      newImage[j][i] += WEIGHT_SIDE * oldImage[j][col];
      weight[j][i] += WEIGHT_SIDE;
    }
  }

  // Count the cells to the immediate right
  for (i = 0; i < cols - 1; i++)
  {
    col = i + 1;
    for (j = 0; j < rows; j++)
    {
      newImage[j][i] += WEIGHT_SIDE * oldImage[j][col];
      weight[j][i] += WEIGHT_SIDE;
    }
  }

  // Count the cells to the bottom left
  for (i = 1; i < cols; i++)
  {
    col = i - 1;
    for (j = 0; j < rows - 1; j++)
    {
      row = j + 1;
      newImage[j][i] += WEIGHT_CORNER * oldImage[row][col];
      weight[j][i] += WEIGHT_CORNER;
    }
  }

  // Count the cells immediately below
  for (i = 0; i < cols; i++)
  {
    for (j = 0; j < rows - 1; j++)
    {
      row = j + 1;
      newImage[j][i] += WEIGHT_SIDE * oldImage[row][i];
      weight[j][i] += WEIGHT_SIDE;
    }
  }

  // Count the cells to the bottom right
  for (i = 0; i < cols - 1; i++)
  {
    col = i + 1;
    for (j = 0; j < rows - 1; j++)
    {
      row = j + 1;
      newImage[j][i] += WEIGHT_CORNER * oldImage[row][col];
      weight[j][i] += WEIGHT_CORNER;
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

// Check if two images are identical
static int countDifferentPixels(long A[N][N], long B[N][N], int rows, int cols)
{
  int i, j;
  int ret_val = 0;
  for (i = 0; i < rows; ++i)
  {
    for (j = 0; j < cols; ++j)
    {
      if (A[i][j] != B[i][j])
      {
        printf("Differs at [%d][%d]: %ld vs %ld \n", i, j, A[i][j], B[i][j]);
        ret_val++;
      }
    }
  }
  return ret_val;
}

// copy one image to another image. The assumption is that they are the
// same size.
static void copy(long src[N][N], long dst[N][N], int rows, int cols)
{
  int i, j;
  for (i = 0; i < rows; ++i)
  {
    for (j = 0; j < cols; ++j)
    {
      dst[i][j] = src[i][j];
    }
  }
}

static int readPBMNumber(FILE *file)
{

  char str[10];
  int val;

  if (!fscanf(file, "%9s", str))
    return -1;

  if (str[0] == '#')
  {
    while (fgetc(file) != '\n')
      ;
    return readPBMNumber(file);
  }

  if (sscanf(str, "%d", &val))
    return val;
  else
    return -1;
}

static int readImage(char *filename, long A[N][N], int *rows, int *cols, int *white)
{

  FILE *file = fopen(filename, "rb");
  int i, j;

  if (!file)
  {
    perror(filename);
    return 1;
  }

  char str[10];
  if (fread(str, 1, 2, file) < 2 || str[0] != 'P' || str[1] != '5')
  {
    fprintf(stderr, "%s: Not a PGM file.\n", filename);
    fclose(file);
    return 2;
  }

  if ((*cols = readPBMNumber(file)) < 0 ||
      (*rows = readPBMNumber(file)) < 0 ||
      (*white = readPBMNumber(file)) < 0 || *white >= 65536 ||
      !isspace(fgetc(file)))
  {
    fprintf(stderr, "%s: Invalid PGM format.\n", filename);
    fclose(file);
    return 3;
  }

  if (*cols > N || *rows > N)
  {
    fprintf(stderr, "This program supports files up to %dx%d pixels, "
                    "but the file contains %dx%d pixels.\n",
            N, N, *rows, *cols);
    fclose(file);
    return 4;
  }

  for (i = 0; i < *rows; i++)
    for (j = 0; j < *cols; j++)
      if (*white < 0x100)
        A[i][j] = fgetc(file);
      else
        A[i][j] = fgetc(file) << 8 | fgetc(file);

  fclose(file);
  return 0;
}

int main(int argc, char **argv)
{

  struct timeval start_ts, end_ts;

  int rows, cols, white;

  // Number trials to to run
  int trial, ntrials = 10;
  long thistime, besttime = LONG_MAX, totaltime = 0, besttimebase, totaltimebase;
  long check;

  if (argc < 2 || argc > 4)
  {
    fprintf(stderr, "usage:\n\t%s filename.pgm [numtrials [outfile.pgm]]\n", argv[0]);
    return -1;
  }

  if (argc > 2)
    ntrials = strtol(argv[2], NULL, 10);

  if (readImage(argv[1], baseImage, &rows, &cols, &white))
  {
    return -2;
  }

  copy(baseImage, trialImage, rows, cols);
  if (PARANOID && countDifferentPixels(baseImage, trialImage, rows, cols))
    printf("Got the wrong answer on copy\n");

  check = baseMask(baseImage, baseAnswer, rows, cols);

  printf("Processing a mask for a %dx%d image for %d trials (check must be: %ld)\n\n",
         rows, cols, ntrials, check);

  for (trial = 0; trial < ntrials; ++trial)
  {
    gettimeofday(&start_ts, 0);
    check = baseMask(trialImage, trialAnswer, rows, cols);
    gettimeofday(&end_ts, 0);
    thistime = usecClock(&start_ts, &end_ts);

    printf("Base run %2d done, check %12ld, took %12ld usec\n", trial, check, thistime);

    // Check that things weren't corrupted
    if (PARANOID && countDifferentPixels(baseImage, trialImage, rows, cols))
    {
      copy(baseImage, trialImage, rows, cols);
      printf("PROBLEM: Original image modified\n");
    }

    if (PARANOID && countDifferentPixels(trialAnswer, baseAnswer, rows, cols))
      printf("PROBLEM: Did not produce the same result\n");

    if (thistime < besttime)
      besttime = thistime;
    totaltime += thistime;
  }

  besttimebase = besttime;
  totaltimebase = totaltime;
  printf("\nThe base implementation took:\n"
         "\tBest   : %12ld usec\n\tAverage: %12ld usec\n",
         besttime, totaltime / ntrials);

  printf("\nStarting the optimized version\n\n");

  besttime = LONG_MAX;
  totaltime = 0;

  for (trial = 0; trial < ntrials; ++trial)
  {
    gettimeofday(&start_ts, 0);
    check = mask(trialImage, trialAnswer, rows, cols);
    gettimeofday(&end_ts, 0);
    thistime = usecClock(&start_ts, &end_ts);

    printf("Optimized run %2d done, check %12ld, took %12ld usec\n", trial, check, thistime);

    // Check that things weren't corrupted
    if (PARANOID && countDifferentPixels(baseImage, trialImage, rows, cols))
    {
      copy(baseImage, trialImage, rows, cols);
      printf("PROBLEM: Original image modified\n");
    }

    if (PARANOID && countDifferentPixels(trialAnswer, baseAnswer, rows, cols))
      printf("PROBLEM: Did not produce the same result\n");

    if (thistime < besttime)
      besttime = thistime;
    totaltime += thistime;
  }

  printf("\nThe optimized implementation took:\n"
         "\tBest   : %12ld usec (ratio: %f)\n"
         "\tAverage: %12ld usec (ratio: %f)\n",
         besttime, (double)besttime / besttimebase,
         totaltime / ntrials, (double)totaltime / totaltimebase);

  if (argc > 3)
  {

    printf("\nSaving result of the last run...\n");

    FILE *file = fopen(argv[3], "wb");
    if (file)
    {
      int i, j;
      fprintf(file, "P5 %d %d %d\n", cols, rows, white);
      for (i = 0; i < rows; ++i)
      {
        for (j = 0; j < cols; ++j)
        {
          fputc(trialAnswer[i][j], file);
        }
      }

      fclose(file);
    }
    else
      perror(argv[3]);
  }

  return 0;
}
