#include <highgui.h>
#include <time.h>
#include <stdio.h>
#include <complex.h>
#include <glib.h>
#include <getopt.h>

#include <shmem.h>

/*
 * image dimensions
 */
gint imgWidth, imgHeight;
gdouble top, bottom, left, right;
gint maxIter;

#include "colorschema.h"


static
int
readConfig (char *cfg)
{
  GKeyFile *confile = g_key_file_new ();
  const GKeyFileFlags flags =
    G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
  GError *error = NULL;

  if (!g_key_file_load_from_file (confile, cfg, flags, &error))
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

static
struct option
long_options[] =
  {
    { "config", required_argument, NULL, 'c' },
    { "output", required_argument, NULL, 'o' },
    { "debug",  optional_argument, NULL, 'd' },
    { NULL,     no_argument,       NULL, 0   },
  };

int
main (int argc, char *argv[])
{
  int debug = 0;
  char *config_filename = "mandelbrot.conf";
  char *output_filename = "output.png";
  time_t start_time, init_time, shmalloc_time,
    compute_time, gather_time, finish_time;

  while (1)
    {
      int oidx;
      const int c = getopt_long (argc, argv,
				 "c:o:d::",
				 long_options,
				 &oidx
				 );
      if (c == -1)
	{
	  break;
	}

      switch (c)
	{
	case 'c':
	  config_filename = optarg;
	  break;
	case 'o':
	  output_filename = optarg;
	  break;
	case 'd':
	  debug = 1;
	  break;
	default:
	  break;
	}
    }

  if (debug)
    {
      start_time = time (NULL);
    }

  /*
   *  start OpenSHMEM
   */
  start_pes (0);

  /*
   * discover program layout
   */
  const int me = _my_pe ();
  const int npes = _num_pes ();

  if (debug)
    {
      init_time = time (NULL);
      printf ("%d: shmem init time = %lds\n", me, init_time - start_time);
    }

  /*
   * Read and print configuration
   */
  (void) readConfig (config_filename);

  /*
   * width:  target image output width, in pixel
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

  /*
   * create image container
   */
  if (me == 0)
    {
      img = cvCreateImage (cvSize (width, height), IPL_DEPTH_8U, nC);
    }
  widthStep = width * nC;

  const int imageSize = widthStep * height;

  /*
   * rowPerP: row number for every PE to tackle
   */
  const int rowPerP = (int) (floor ((double) height / npes));

  /*
   * blockSize: the actual parted image size for each PE (except one
   * when dividing is not even) to compute
   * restRow: the extra number of rows to tackle for PE n-1
   */
  const int blockSize = rowPerP * widthStep;
  const int restRow = height - rowPerP * npes;
  const int myRows = rowPerP + (me==npes-1?restRow:0);
  const int myBlockSize = myRows+widthStep;

  if (debug) {
    if (me == 0)
      {
	printf
	  ("width=%d\n height=%d\n widthStep=%d\n imageSize=%d\n rowPerP=%d\n blockSize=%d\n",
	   width, height, widthStep, imageSize, rowPerP, blockSize);
      }
  }

  /*
   * allocate symmetric work area
   */
  char *taskB = (char *) shmalloc (imageSize);
  int  *taskL = (int *)shmalloc(sizeof(int) * height);
  char *workB;



  if(me==0){
	/* randomize task list */
	  for(i=0;i<height;i++){
		  taskL[i]=i;
	  }
	  for(i=0;i<height;i++){
		  int b=taskL[i];
		  int r=rand()%height;
		  taskL[i]=taskL[r];
		  taskL[r]=b;
	  }
	  workB=(char *)malloc(myRows*widthStep);
  }else{
		workB=taskB;
  }
  shmem_barrier_all();
  if(me > 0){
  	shmem_getmem(taskL, taskL+rowPerP*me, sizeof(int) * myRows, 0);
  }

  /*
   * initial image is completley blank
   */

  (void) memset (workB, 0, myBlockSize);
  

  if (debug)
    {
      shmalloc_time = time (NULL);
      printf ("%d: shmalloc time = %lds\n", me, shmalloc_time - init_time);
    }

  /* compute on PEs k is the iteration times which will decide the
   * pixel's color, after iterations, |z| will great than 2 the
   * pixel's color will pickby k, as the color number with k mod
   * palletSize.
   */
  for (i = 0; i < myRows; i++)
  {
	  for (j = 0; j < width; j++)
	  {
		  const complex c = (double) (right - left) / width * j + left
			  + ((double) (bottom - top) / height * taskL[i] +
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
				  workB[i * widthStep + j * nC + nCi] =
					  pallet[(k % palletSize)][nC - 1 - nCi];
			  }
		  }
	  }
  }

  if (debug)
    {
      compute_time = time (NULL);
      printf ("%d: compute time = %lds\n", me, compute_time - shmalloc_time);
    }

  /*
   * gather data from different PEs
   */

  for(i=0;i<myRows;i++){
  	shmem_putmem (taskB+taskL[i]*widthStep, workB+i*widthStep, widthStep, 0);
  }

  /*
   * synchronize after image collection
   */
  shmem_barrier_all ();


  if (debug)
    {
      gather_time = time (NULL);
      printf ("%d: gather time = %lds\n", me, gather_time - compute_time);
    }

  /*
   * save image
   */
  if (me == 0)
    {
      (void) memcpy (img->imageData, taskB, imageSize);

      if (debug)
	{
	  printf ("OpenSHMEM time cost:      %lds\n\n",
		  gather_time - start_time
		  );
	}

      cvSaveImage (output_filename, img, 0);
      cvReleaseImage (&img);

      printf ("Wrote image to \"%s\"\n", output_filename);

      if (debug)
	{
	  finish_time = time (NULL);
	  printf ("Total time cost:      %lds\n\n",
		  finish_time - start_time
		  );
	}
    }

  /*
   * clean up work area
   */
  shfree (taskB);

  return 0;
}
