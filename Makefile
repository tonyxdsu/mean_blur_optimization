CFLAGS = -g -Wall -Wno-unused-function
LDFLAGS = -pthread
all: timemask

timemask.o: timemask.c mask.h
mask.o: mask.c mask.h
timemask: timemask.o mask.o

mask-%.o: mask.c mask.h
	gcc $(CFLAGS) -DMASK_VERSION=$* -c mask.c -o mask-$*.o
timemask-%: timemask.o mask-%.o
	gcc $(CFLAGS) $(LDFLAGS) -o timemask-$* timemask.o mask-$*.o

clean:
	-rm -f timemask timemask.o mask.o timemask-* mask-*.o
tidy: clean
	-rm -f *~
