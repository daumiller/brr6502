#include <string.h>
#include "include/cpu.h"
#include "cpu/operation.h"

u8 *test_ram;
u8 *test_rom;
CPU *test_cpu;
BusInterface test_bus;
pthread_mutex_t single_step_mutex;
pthread_cond_t  single_step_condition;

extern OpCode      op_code_table[256];
extern AddressMode address_mode_table[256];

void _cpu_decode_operation(CPU *cpu, Operation *op); // borrowed from cpu/cpu.c

char *op_code_string[] = {
  "ADC", "AND", "ASL",
  "BCC", "BCS", "BEQ", "BIT", "BMI", "BNE", "BPL", "BRA", "BRK", "BVC", "BVS",
  "CLC", "CLD", "CLI", "CLV", "CMP", "CPX", "CPY",
  "DEC", "DEX", "DEY",
  "INC", "INX", "INY",
  "JMP", "JSR",
  "LDA", "LDX", "LDY", "LSR",
  "NOP",
  "ORA",
  "PHA", "PHP", "PHX", "PHY", "PLA", "PLP", "PLX", "PLY",
  "ROL", "ROR", "RTI", "RTS",
  "SBC", "SEC", "SED", "SEI", "STA", "STP", "STX", "STY", "STZ",
  "TAX", "TAY", "TRB", "TSB", "TSX", "TXA", "TXS", "TYA",
  "WAI",
  "XOR",
  "BBR0", "BBR1", "BBR2", "BBR3", "BBR4", "BBR5", "BBR6", "BBR7",
  "BBS0", "BBS1", "BBS2", "BBS3", "BBS4", "BBS5", "BBS6", "BBS7",
  "RMB0", "RMB1", "RMB2", "RMB3", "RMB4", "RMB5", "RMB6", "RMB7",
  "SMB0", "SMB1", "SMB2", "SMB3", "SMB4", "SMB5", "SMB6", "SMB7",
  "invl"
};

char *address_mode_string[] = {
  "absolute",
  "(absolute, x)",
  "absolute, x",
  "absolute, y",
  "(absolute)",
  "accumulator",
  "immediate",
  "implied",
  "relative",
  "stack",
  "zp",
  "(zp, x)",
  "zp, x",
  "zp, y",
  "(zp)",
  "(zp), y",
  "zp, relative",
  "invalid"
};

u8 test_io_read(u16 address) {
  return 0;
}

void test_io_write(u16 address, u8 data) {
  return;
}

u8 test_bus_read(u16 address) {
  if(address < 0x8000) { return test_ram[address]; }
  if(address < 0x8004) { return test_io_read(address - 0x8000); }
  return test_rom[address - 0x8000]; // ROM is 32k, but 4 lowest bytes aren't available
}

void test_bus_write(u16 address, u8 data) {
  if(address < 0x8000) { test_ram[address] = data; return; }
  if(address < 0x8004) { test_io_write(address - 0x8000, data); return; }
  // ROM write attempts are ignored
}

void _test_system_load_rom(char *rom_file) {
  test_rom = (u8 *)malloc(0x8000);

  FILE *fin = fopen(rom_file, "rb");
  if(!fin) {
    free(test_rom);
    printf("Test System Init Error - Unable to Open ROM File \"%s\".", rom_file);
    exit(-1);
  }

  u8 *read_ptr = test_rom;
  size_t read = 1, total = 0, chunk;
  while(read && (total < 0x8000)) {
    chunk = ((total + 4096) <= 0x8000) ? 4096 : (0x8000 - total);
    read = fread(read_ptr, 1, chunk, fin);
    read_ptr += read;
    total += read;
  }

  fclose(fin);
}

void test_signal_handler_sync(void *cpu, SignalType signal, Edge edge) {
  if(edge == positive) {
    cpu_signal_to_cpu((CPU *)cpu, SIGNAL_TYPE_RDY, negative);
    pthread_mutex_lock(&single_step_mutex);
    pthread_cond_broadcast(&single_step_condition);
    pthread_mutex_unlock(&single_step_mutex);
  }
}

void test_system_init(char *rom_file) {
  _test_system_load_rom(rom_file);

  test_ram = (u8 *)malloc(0x8000);
  test_bus.read  = &test_bus_read;
  test_bus.write = &test_bus_write;

  test_cpu = cpu_create(&test_bus);
  cpu_signal_handler_add(test_cpu, SIGNAL_TYPE_SYNC, &test_signal_handler_sync);

  pthread_mutex_init(&single_step_mutex    , NULL);
  pthread_cond_init (&single_step_condition, NULL);

  cpu_reset(test_cpu);
}

void test_system_shutdown() {
  cpu_shutdown(test_cpu);
  cpu_wait(test_cpu);
}

void test_system_step_enter() {
  pthread_mutex_lock(&single_step_mutex);
  pthread_cond_wait(&single_step_condition, &single_step_mutex);
  pthread_mutex_unlock(&single_step_mutex);
}

void test_system_step_exit() {
  cpu_signal_to_cpu((CPU *)test_cpu, SIGNAL_TYPE_RDY, positive);
}

void test_system_value_string(CPU *cpu, Operation *op, char *buffer) {
  switch(op->mode) {
    case VALUE_MODE_IMPLIED:
      sprintf(buffer, "implied  ");
      return;

    case VALUE_MODE_ADDRESS:
      sprintf(buffer, "%02X (%04X)", test_bus_read(op->address), op->address);
      return;

    case VALUE_MODE_IMMEDIATE:
      sprintf(buffer, "%02X       ", op->value);
      return;

    case VALUE_MODE_REGISTER:
      sprintf(buffer, "%02X (", *(op->reference));
      if(op->reference == &(cpu->a)) { printf("A)   "); }
      else if(op->reference == &(cpu->x)) { printf("X)   "); }
      else if(op->reference == &(cpu->y)) { printf("Y)   "); }
      else if(op->reference == &(cpu->s)) { printf("S)   "); }
      else if(op->reference == &(cpu->p)) { printf("P)   "); }
      else { printf("?)   "); }
      return;
  }
}

bool test_system_on_nop() {
  return test_bus_read(test_cpu->pc) == 0xEA;
}

void test_system_dump_header() {
  printf("  | A  | X  | Y  | P  | S  |  PC  | NV-BDIZC | Next Op | Value     | Mode            |\n");
}

void test_system_dump_cpu() {
  static CPU dummy;
  static Operation op;
  static char value_str[10];
  static char *mode_str;
  static u8 opcode;

  opcode = test_bus_read(test_cpu->pc);
  dummy.a    = test_cpu->a;
  dummy.x    = test_cpu->x;
  dummy.y    = test_cpu->y;
  dummy.s    = test_cpu->s;
  dummy.p    = test_cpu->p;
  dummy.pc   = test_cpu->pc;
  dummy._bus = test_cpu->_bus;
  _cpu_decode_operation(&dummy, &op);

  char *op_str = op_code_string[op_code_table[opcode]];
  char *op_pad = (op_str[3] == 0x00) ? " " : "";
  test_system_value_string(&dummy, &op, (char *)&value_str);
  mode_str = address_mode_string[address_mode_table[opcode]];

  //        | XX | XX | XX | XX | XX | XXXX | 11111111 |  OPC+   | XX (XXXX) | 123456789ABCDEF |
  //        | A  | X  | Y  | P  | S  |  PC  | NV-BDIZC | Next Op | Value     | Mode            |
  printf("  | %02X | %02X | %02X | %02X | %02X | %04X | %d%d-%d%d%d%d%d |   %s%s  | %s | %s",
         test_cpu->a, test_cpu->x, test_cpu->y, test_cpu->p, test_cpu->s, test_cpu->pc,
         (test_cpu->p & STATUS_N) > 0, (test_cpu->p & STATUS_V) > 0, (test_cpu->p & STATUS_B) > 0, (test_cpu->p & STATUS_D) > 0,
         (test_cpu->p & STATUS_I) > 0, (test_cpu->p & STATUS_Z) > 0, (test_cpu->p & STATUS_C) > 0,
         op_str, op_pad, value_str, mode_str);
  for(int i=strlen(mode_str); i<15; i++) { printf(" "); } printf(" |\n");
}

void test_system_dump_memory(u16 address) {
  for(int i=0; i<8; i++) {
    printf("%02X ", test_bus_read(address + i));
  }
  printf("\n");
}
