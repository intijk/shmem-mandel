OPENCV_FLAGS = $(shell pkg-config --cflags opencv)
OPENCV_LIBS = $(shell pkg-config --libs opencv)

GLIB2_FLAGS = $(shell pkg-config --cflags glib-2.0)
GLIB2_LIBS = $(shell pkg-config --libs glib-2.0)

OSHCCFLAGS = -g -O3 -Wall

CC = oshcc
CFLAGS = $(OSHCCFLAGS) $(OPENCV_FLAGS) $(GLIB2_FLAGS)

CFLAGS += -DDEBUG

LIBS = $(OPENCV_LIBS) $(GLIB2_LIBS) -lm


m: mandelbrot.c
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean

clean:
	rm -f m output.png
