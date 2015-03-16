CC= clang
COPTS= -std=c99 -Wall -I.
CLIBS= -lpthread
OBJECTS= obj/common.o obj/signal.o obj/operation.o obj/cpu.o
SYSTEMS= system/sys_test
ROMS=    nop_loop.rom

all : objects systems

objects : $(OBJECTS)

systems : $(SYSTEMS)

roms : $(ROMS)

obj/%.o : cpu/%.c
	mkdir -p obj
	$(CC) $(COPTS) -c $^ -o $@

nop_loop.rom : system/test/nop_loop.s
	xa -bt32768 system/test/nop_loop.s -o nop_loop.rom

system/sys_test : $(OBJECTS) system/test/test_system.c system/test/test_main.c
	mkdir -p system/test/obj
	$(CC) $(COPTS) -c system/test/test_system.c -o system/test/obj/test_system.o
	$(CC) $(COPTS) -c system/test/test_main.c   -o system/test/obj/test_main.o
	$(CC) $(OBJECTS) system/test/obj/*.o $(CLIBS) -o system/sys_test

clean :
	rm -rf ./obj

veryclean : clean
	rm -rf system/test/obj
	rm -f $(ROMS)
	rm -f $(SYSTEMS)
