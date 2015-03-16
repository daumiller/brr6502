#pragma once
#include "include/std.h"
#include "include/cpu.h"
#include "cpu/operation.h"

//==================================================================
// status defines
#define STATUS_UNUSED 32
#define STATUS_AT_BOOT (STATUS_UNUSED | STATUS_BREAK | STATUS_IRQB)
#define SIGNALS_AT_BOOT (SIGNAL_TYPE_IRQB | SIGNAL_TYPE_NMIB | SIGNAL_TYPE_RDY)

#define VECTOR_IRQ   0xFFFE
#define VECTOR_RESET 0xFFFC
#define VECTOR_NMI   0xFFFA

//==================================================================
// data type defines
#define U16_LE(x, y) ((u16)(x) | (u16)((y) << 8))
#define U16_BE(x, y) ((u16)(y) | (u16)((x) << 8))
#define BYTE_LO(x)   (x & 0x00FF)
#define BYTE_HI(x)   ((x & 0xFF00) >> 8)
#define u8_as_i8(x)  (*((i8 *)(&(x))))
#define u8_as_i16(x) ((i16)(*((i8 *)(&(x)))))

//==================================================================
// bus i/o defines
#define bus_read(x)      cpu->_bus->read(x)
#define bus_write(x, y)  cpu->_bus->write(x, y)
#define bus_read16(x)    U16_LE(cpu->_bus->read(x), cpu->_bus->read(x+1))

//==================================================================
// bitwise defines
#define bit_set(x, y)            x |= y
#define bit_clear(x, y)          x &= ~y
#define status_set(x)            cpu->p |= x
#define status_clear(x)          cpu->p &= ~x
#define status_change(x, y)      _cpu_status_change(cpu, x, y)
#define status_is_set(x)         ((cpu->p & x) > 0)
#define status_is_clear(x)       ((cpu->p & x) == 0)
#define signal_changed_to(x, y)  ((cpu->_signal_status & x) && ((cpu->_signal_state & x) == y))

//==================================================================
// common functions
void _cpu_status_change(CPU *cpu, u8 bit, bool set);
u16  _cpu_read_vector(CPU *cpu, u16 address);
void _cpu_stack_push(CPU *cpu, u8 value);
u8   _cpu_stack_pull(CPU *cpu);

//==================================================================
// signal functions
void _cpu_signal_from_cpu(CPU *cpu, SignalType signal, Edge edge);
