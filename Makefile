all:
	gcc -Wall -ansi -std=gnu99 util.c ax25.c hdlc.c main.c -lliquid -lm -lsndfile -o liquidwolf

clean:
	rm liquidwolf
