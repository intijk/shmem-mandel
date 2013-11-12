#include <highgui.h>
#include <time.h>
#include <shmem.h>
#include <stdio.h>
#include <complex.h>
#include <glib.h>

gint imgWidth, imgHeight;
gdouble top, bottom, left, right;
gint maxIter;

char *config_filename;
char *output_filename;

#define PALLET_MAX_SIZE 100

static int readConfig (void);

/* Color schema original */
/*
int palletSize=7;
int pallet[PALLET_MAX_SIZE][3]={
  {255,0,0},
  {255,128,0},
  {255,255,0},
  {0,255,0},
  {0,255,255},
  {0,0,255},
  {255,0,255}
};
*/

/* Color Schema gray and yellow */
/*
int palletSize=5;
int pallet[PALLET_MAX_SIZE][3]={
  {0x71,0x27,0x04},
  {0xbd,0x78,0x03},
  {0xfe,0x9d,0x01},
  {0xff,0xbb,0x1c},
  {0xee,0xd2,0x05}
};
*/

/*Color schema jiaomei c2 */
/*
int palletSize=7;
int pallet[PALLET_MAX_SIZE][3]={
  {0x63,0xbd,0x84},
  {0xd6,0xe6,0xAD},
  {0xff,0xf7,0xd6},
  {0xff,0xff,0xef},
  {0xff,0xe6,0xde},
  {0xff,0xbd,0x9c},
  {0xef,0x9c,0x94},
};
*/

/* Color schema qingkuai c2 */
int palletSize = 7;

int pallet[PALLET_MAX_SIZE][3] = {
  {0xbd, 0x21, 0x19},
  {0xf7, 0x94, 0x63},
  {0xff, 0xe6, 0x08},
  {0xd6, 0x94, 0x19},
  {0x29, 0xb5, 0xce},
  {0xf7, 0xff, 0xf7},
  {0x73, 0x7b, 0xb5},
};


/* Color schema donggan c 1 */
/*
int palletSize=7;
int pallet[PALLET_MAX_SIZE][3]={
    {0x63,0x73,0xb5},
    {0xff,0x52,0x00},
    {0x4a,0xa5,0x4a},
    {0xff,0xff,0xff},
    {0xff,0xce,0x10},
    {0xef,0x3a,0x21},
    {0x00,0x00,0x00},
};
*/

int
main (int argc, char *argv[])
{
#ifdef DEBUG
  const time_t start_time = time (NULL);
#endif

  if (argc > 1)
    {
      config_filename = argv[1];
    }
  else
    {
      config_filename = "mandelbrot.conf";
    }

  if (argc > 2)
    {
      output_filename = argv[2];
    }
  else
    {
      output_filename = "output.png";
    }

  /* init PEs */
  start_pes (0);
  int me = _my_pe ();
  int npes = _num_pes ();

#ifdef DEBUG
  const time_t init_time = time (NULL);
  printf ("%d: shmem init time = %lds\n", me, init_time - start_time);
#endif

  /* Read and print configuration */
  readConfig ();

  /* width:  target image output width, in pixel
   * height: target image output height, in pixel
   * widthStep: number of bytes in one row of image
   * nC: color depth, 3 means 3 bytes, ie. 24 bits
   */
  const int width = imgWidth;
  const int height = imgHeight;
  int widthStep;
  const int nC = 3;
  int i, j, k, nCi;
  IplImage *img;

  if (me == 0)
    {
      printf ("Reading configuration from file \"%s\"\n", config_filename);

      /*
       * Interest image area is described by top, bottom, left and
       * right, they are coordinates on 2d cartesian space.
       */
      printf ("Image size:\n      %d x %d\n\n", imgWidth, imgHeight);
      printf ("Interest area:\n");
      printf ("      +-------- %-10f --------+\n", top);
      printf ("      |                            |\n");
      printf ("      |                            |\n");
      printf (" %-10f                  %-10f\n", left, right);
      printf ("      |                            |\n");
      printf ("      |                            |\n");
      printf ("      +-------- %-10f --------+\n\n", bottom);
      printf ("Max iteration time:\n      %d\n\n", maxIter);
      printf ("npes:\n      %d\n\n", npes);
    }

  /* create image */
  if (me == 0)
    {
      img = cvCreateImage (cvSize (width, height), IPL_DEPTH_8U, nC);
    }
  widthStep = width * nC;

  int imageSize = widthStep * height;

  /*
   * rowPerP: row number for every PE to tackle
   */
  const int rowPerP = (int) (ceil ((double) height / npes));

  /*
   * blockSize: the actual parted image size for each PE (except one
   * when dividing is not even) to compute
   */
  const int blockSize = rowPerP * widthStep;

#ifdef DEBUG
  if (me == 0)
    {
      printf
	("width=%d\n height=%d\n widthStep=%d\n imageSize=%d\n rowPerP=%d\n blockSize=%d\n",
	 width, height, widthStep, imageSize, rowPerP, blockSize);
    }
#endif

  /* allocate symmetric buffer */
  char *taskB = (char *) shmalloc (imageSize);
  // memset (taskB, 0, blockSize);

#ifdef DEBUG
  time_t shmalloc_time = time (NULL);
  printf ("%d: shmalloc time = %lds\n", me, shmalloc_time - init_time);
#endif

  /* compute on PEs k is the iteration times which will decide the
   * pixel's color, after iterations, |z| will great than 2 the
   * pixel's color will pickby k, as the color number with k mod
   * palletSize.
   */
  for (i = 0; i < rowPerP; i++)
    {
      for (j = 0; j < width; j++)
	{
	  const complex c = (double) (right - left) / width * j + left
	    + ((double) (bottom - top) / height * (me * rowPerP + i) +
	       top) * _Complex_I;
	  complex z = 0;

	  for (k = 0; k < maxIter; k++)
	    {
	      z = cpow (z, 2) + c;
	      if (cabs (z) > 2)
		{
		  break;
		}
	    }
	  if (cabs (z) > 2)
	    {
	      for (nCi = 0; nCi < nC; nCi++)
		{
		  taskB[i * widthStep + j * nC + nCi] =
		    pallet[(k % palletSize)][nC - 1 - nCi];
		}
	    }
	}
    }

#ifdef DEBUG
  time_t compute_time = time (NULL);
  printf ("%d: compute time = %lds\n", me, compute_time - shmalloc_time);
#endif

  /* gather data from different PEs */
  if (me < npes - 1)
    {
      shmem_putmem (taskB + me * blockSize, taskB, blockSize, 0);
    }
  else
    {
      const int restSize = imageSize - (npes - 1) * blockSize;

#ifdef DEBUG
      printf ("restSize = %d\n", restSize);
#endif

      shmem_putmem (taskB + me * blockSize, taskB,
		    imageSize - (npes - 1) * blockSize, 0);
    }
  shmem_barrier_all ();


#ifdef DEBUG
  const time_t gather_time = time (NULL);
  printf ("%d: gather time = %lds\n", me, gather_time - compute_time);
#endif

  /* save image */
  if (me == 0)
    {
      memcpy (img->imageData, taskB, imageSize);

#ifdef DEBUG
      printf ("OpenSHMEM time cost:      %lds\n\n",
	      gather_time - start_time
	      );
#endif
      cvSaveImage (output_filename, img, 0);
      cvReleaseImage (&img);

#ifdef DEBUG
      const time_t finish_time = time (NULL);
      printf ("Total time cost:      %lds\n\n",
	      finish_time - start_time
	      );
#endif
    }

  shfree (taskB);

  return 0;
}

static
int
readConfig ()
{
  GKeyFile *confile = g_key_file_new ();
  const GKeyFileFlags flags =
    G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
  GError *error = NULL;

  if (!g_key_file_load_from_file (confile, config_filename, flags, &error))
    {
      g_error (error->message);
      return -1;
    }

  imgWidth =
    g_key_file_get_integer (confile, "ImageSize", "ImageWidth", &error);
  imgHeight =
    g_key_file_get_integer (confile, "ImageSize", "ImageHeight", &error);
  top =
    g_key_file_get_double (confile, "Area", "top", &error);
  bottom =
    g_key_file_get_double (confile, "Area", "bottom", &error);
  left =
    g_key_file_get_double (confile, "Area", "left", &error);
  right =
    g_key_file_get_double (confile, "Area", "right", &error);
  maxIter =
    g_key_file_get_integer (confile, "Operation", "MaxIteration", &error);

  return 0;
}
