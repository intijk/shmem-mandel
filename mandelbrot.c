#include <highgui.h>
#include <time.h>
#include <shmem.h>
#include <stdio.h>
#include <complex.h>
#include <glib.h>

#define debug

gint imgWidth, imgHeight;
gdouble top, bottom, left, right;
gint maxIter;

#define PALLET_MAX_SIZE 100
int readConfig (void);
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
main (int argc, char const *argv[])
{
  /* init PEs */
  time_t start_time = time (NULL);
  start_pes (0);
  int me = _my_pe ();
  int npes = _num_pes ();

  time_t init_time = time (NULL);
#ifdef debug
  printf ("%d: shmem init time = %ds\n", me, init_time - start_time);
#endif

  /* Read and print configuration */
  readConfig ();
  int width = imgWidth;
  int height = imgHeight;
  int widthStep;
  int nC = 3;
  int i, j, k, nCi;
  complex z;
  complex c;
  IplImage *img;

  if (me == 0)
    {
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

  /*rowPerP: row number for every PE to tackle */
  int rowPerP = (int) (ceil ((double) height / npes));
  int blockSize = rowPerP * widthStep;
#ifdef debug
  if (me == 0)
    {
      printf
	("width=%d\n height=%d\n widthStep=%d\n imageSize=%d\n rowPerP=%d\n blockSize=%d\n",
	 width, height, widthStep, imageSize, rowPerP, blockSize);
    }
#endif

  /* allocate symmetric buffer */
  char *taskB = (char *) shmalloc (imageSize);
  memset (taskB, 0, blockSize);

  time_t shmalloc_time = time (NULL);
#ifdef debug
  printf ("%d: shmalloc time = %ds\n", me, shmalloc_time - init_time);
#endif

  /* compute on PEs */
  for (i = 0; i < rowPerP; i++)
    {
      for (j = 0; j < width; j++)
	{
	  z = 0;
	  c = (double) (right - left) / width * j + left
	    + ((double) (bottom - top) / height * (me * rowPerP + i) +
	       top) * _Complex_I;
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

  time_t compute_time = time (NULL);
#ifdef debug
  printf ("%d: compute time = %ds\n", me, compute_time - shmalloc_time);
#endif

  /* gather data from different PEs */
  if (me < npes - 1)
    {
      shmem_putmem (taskB + me * blockSize, taskB, blockSize, 0);
    }
  else
    {
      int restSize = imageSize - (npes - 1) * blockSize;

#ifdef debug
      printf ("restSize = %d\n", restSize);
#endif

      shmem_putmem (taskB + me * blockSize, taskB,
		    imageSize - (npes - 1) * blockSize, 0);
    }
  shmem_barrier_all ();


  time_t gather_time = time (NULL);

#ifdef debug
  printf ("%d: gather time = %ds\n", me, gather_time - compute_time);
#endif


  /* write to file */
  if (me == 0)
    {
      memcpy (img->imageData, taskB, imageSize);
    }


  shfree (taskB);

  /*save image */
  if (me == 0)
    {
      time_t finish_time = time (NULL);
      printf ("Total time cost:      %ds\n\n",
	      (int) (finish_time - start_time));
      cvSaveImage ("output.png", img, 0);
      cvReleaseImage (&img);
    }

  return 0;
}

int
readConfig ()
{
  GKeyFile *confile = g_key_file_new ();
  GKeyFileFlags flags =
    G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;;
  GError *error = NULL;

  if (!g_key_file_load_from_file (confile, "mandelbrot.conf", flags, &error))
    {
      g_error (error->message);
      return -1;
    }

  imgWidth =
    g_key_file_get_integer (confile, "ImageSize", "ImageWidth", &error);
  imgHeight =
    g_key_file_get_integer (confile, "ImageSize", "ImageHeight", &error);
  top = g_key_file_get_double (confile, "Area", "top", &error);
  bottom = g_key_file_get_double (confile, "Area", "bottom", &error);
  left = g_key_file_get_double (confile, "Area", "left", &error);
  right = g_key_file_get_double (confile, "Area", "right", &error);
  maxIter =
    g_key_file_get_integer (confile, "Operation", "MaxIteration", &error);

  return 0;
}
