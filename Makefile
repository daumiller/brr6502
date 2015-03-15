CC= clang
COPTS= -std=c99 -Wall -I.
CLIBS= -lpthread
OBJECTS= obj/common.o obj/signal.o obj/operation.o obj/cpu.o

all : objects

objects : $(OBJECTS)

obj/%.o : cpu/%.c
	mkdir -p obj
	$(CC) $(COPTS) -c $^ -o $@

clean :
	rm -rf ./obj

