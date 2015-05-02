

OBJS=playmusic.o wave_header.o divide.o

LFLAGS=

CFLAGS=-I.

SFLAGS=

playmusic: $(OBJS)
	gcc -o3 -o playmusic $(OBJS) -lm
.S.o:
	gcc $(SFLAGS) -c $< 

.c.o:
	gcc $(CFLAGS) -c $<

clean:
	rm -f *.o *~ playmusic tmp.data
