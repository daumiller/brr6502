#include "cpu/internal.h"

#define STACK_BASE 0x0100

void _cpu_status_change(CPU *cpu, u8 bit, bool set) {
  if(set) {
    cpu->p |= bit;
  } else {
    cpu->p &= ~bit;
  }
}

u16 _cpu_read_vector(CPU *cpu, u16 address) {
  // if we decide to support VPB..
  // _cpu_signal_from_cpu(cpu, SIGNAL_TYPE_VPB, negative);
  u16 value = bus_read16(address);
  //_cpu_signal_from_cpu(cpu, SIGNAL_TYPE_VPB, positive);
  return value;
}

void _cpu_stack_push(CPU *cpu, u8 value) {
  bus_write(STACK_BASE | cpu->s, value);
  cpu->s++;
}

u8 _cpu_stack_pull(CPU *cpu) {
  cpu->s--;
  return bus_read(STACK_BASE | cpu->s);
}
