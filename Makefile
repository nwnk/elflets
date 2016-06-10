TARGETS = elfp

all : $(TARGETS)

%: %.c
	gcc -g -Wall -Werror -Wextra -O2 -o $@ $< -lelf

clean:
	rm -vf *.o $(TARGETS)
