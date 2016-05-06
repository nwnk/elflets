all : elfp

elfp: elfp.c
	gcc -g -Wall -Werror -Wextra -O2 -o elfp elfp.c -lelf

clean:
	rm -vf *.o elfp
