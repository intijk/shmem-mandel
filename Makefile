m: mandelbrot.c
	oshcc -lm `pkg-config --cflags opencv glib-2.0`  mandelbrot.c `pkg-config --libs opencv glib-2.0`  -o m

output.png:m mandelbrot.conf
	salloc --exclusive -w shark25,shark26 oshrun -np 16 ./m

open:output.png
	evince ./output.png

line:
	gcc -lm `pkg-config --cflags opencv glib-2.0`  line.c `pkg-config --libs opencv glib-2.0`  -o l

.PHONY: clean
clean:
	rm m output.png
