#include <stdio.h>
#include <stdint.h>
#include "include/std.h"

void test_system_init(char *rom_file);
void test_system_shutdown();
void test_system_step_enter();
void test_system_step_exit();
void test_system_dump_header();
void test_system_dump_cpu();
void test_system_dump_operation();
void test_system_dump_memory(u16 address);
bool test_system_on_nop();

int main(int argc, char **argv) {
  if(argc < 2) {
    printf("Usage: test file.rom\n");
    return 0;
  }

  test_system_init(argv[1]);
  test_system_step_enter();

  char buff[64];
  u16 dump_address = 0;
  unsigned int dump_reader;

  test_system_dump_header();
  test_system_dump_cpu();

  while(true) {
    printf("? ");
    fgets(buff, 64, stdin);

    if(buff[0] == '?') {
      printf("  '?'      this help message\n");
      printf("  's'      step cpu one operation\n");
      printf("  'b'      run until a break (a NOP)\n");
      printf("  'bq'     run quietly until a break\n");
      printf("  'm:XXXX' dump 8 bytes from memory location 0xXXXX\n");
      printf("  'n'      dump next 8 bytes from memory\n");
      printf("  'q'      quit\n");
      continue;
    }

    if(buff[0] == '\n' || buff[0] == 's') {
      test_system_step_exit();
      test_system_step_enter();
      test_system_dump_header();
      test_system_dump_cpu();
      continue;
    }

    if(buff[0] == 'b') {
      bool quiet = buff[1] == 'q';
      if(quiet == false) { test_system_dump_header(); }
      while(test_system_on_nop() == false){
        test_system_step_exit();
        test_system_step_enter();
        if(quiet == false) { test_system_dump_cpu(); }
      }
    }

    if(buff[0] == 'n') {
      dump_address += 8;
      test_system_dump_memory(dump_address);
      continue;
    }

    if(buff[0] == 'm') {
      int result = sscanf(buff, "m:%x", &dump_reader);
      if(result < 1) {
        printf("Error reading memory address. Bad format?\n");
      } else {
        dump_address = dump_reader;
        test_system_dump_memory(dump_address);
        continue;
      }
    }

    if(buff[0] == 'q') {
      test_system_shutdown();
      exit(0);
    }
  }
}
