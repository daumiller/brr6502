#pragma once
#include "include/std.h"

typedef bool  Edge;
#define positive true
#define negative false

typedef enum {
  SIGNAL_TYPE_SYNC  =  1, // cycle_start    | out | when high, a new op fetch cycle is beginning (this is the time to signal RDY)
  SIGNAL_TYPE_IRQB  =  2, // ~int_request   | inp | low to signal pending interrupt
  SIGNAL_TYPE_NMIB  =  4, // ~non_mask_int  | inp | low to signal pending non-maskable interrupt
  SIGNAL_TYPE_RDY   =  8  // ~halt          | i/o | low during SYNC to halt CPU until high edge
  // SIGNAL_TYPE_RWE      // read / ~write  | out | abstracted through BusInterface
  // SIGNAL_TYPE_BE       // bus_enable     | inp | hardware detail (tri-stating)
  // SIGNAL_TYPE_RESB     // ~reset         | inp | abstracted cpu_reset
  // SIGNAL_TYPE_MLB      // ~memory_lock   | out | not supported
  // SIGNAL_TYPE_SOB      // ~set_overflow  | inp | not supported
  // SIGNAL_TYPE_VPB      // ~vector_read   | out | not supported
} SignalType;

typedef void (*SignalCallback)(void *cpu_ptr, SignalType signal, Edge edge);

typedef struct st_signal_list {
  SignalCallback signal;
  struct st_signal_list *prev;
  struct st_signal_list *next;
} SignalList;
