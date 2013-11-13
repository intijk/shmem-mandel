OPENCV_FLAGS = $(shell pkg-config --cflags opencv)
OPENCV_LIBS = $(shell pkg-config --libs opencv)

GLIB2_FLAGS = $(shell pkg-config --cflags glib-2.0)
GLIB2_LIBS = $(shell pkg-config --libs glib-2.0)

OSHCCFLAGS = -g -O3 -Wall

CC = oshcc
CFLAGS = $(OSHCCFLAGS) $(OPENCV_FLAGS) $(GLIB2_FLAGS)

#
# to show timings and commentary
#
CFLAGS += -DDEBUG

LD = $(CC)
LDFLAGS =

LIBS = $(OPENCV_LIBS) $(GLIB2_LIBS) -lm

# -------------------------------------------------

TARGET = m

$(TARGET): mandelbrot.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: tidy clean

tidy:
	rm -f *.o

clean:	tidy
	rm -f $(TARGET)
