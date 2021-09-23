.PHONY: all musl clean debug

all:
	cc --std=c99 src/main.c -o template2essb -Wall -Wextra -Werror -O3
musl:
	musl-gcc --std=c99 src/main.c -o template2essb -Wall -Wextra -Werror -O3
debug:
	cc --std=c99 src/main.c -o template2essb -Wall -Wextra -Werror -O0 -g
clean:
	rm -f template2essb
