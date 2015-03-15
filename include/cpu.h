#pragma once
#include "include/std.h"
#include "include/bus_interface.h"
#include "include/signal.h"
#include <pthread.h>

#define STATUS_N 128
#define STATUS_V  64
#define STATUS_B  16
#define STATUS_D   8
#define STATUS_I   4
#define STATUS_Z   2
#define STATUS_C   1

#define STATUS_NEGATIVE 128  // N
#define STATUS_OVERFLOW  64  // O
#define STATUS_BREAK     16  // B
#define STATUS_DECIMAL    8  // D
#define STATUS_IRQB       4  // I
#define STATUS_ZERO       2  // Z
#define STATUS_CARRY      1  // C

typedef struct {
  u8 a, x, y, s, p;
  u16 pc;

  BusInterface *_bus;

  u8 _signal_state;
  u8 _signal_status;

  SignalList *_edge_RDY;
  SignalList *_edge_SYNC;

  pthread_t       _cycle_thread;
  pthread_mutex_t _unhalt_mutex;
  pthread_cond_t  _unhalt_condition;
  pthread_mutex_t _waiting_mutex;
  pthread_cond_t  _waiting_condition;
  pthread_mutex_t _running_mutex;
  pthread_cond_t  _running_condition;

  bool _halted;
  bool _waiting;
  bool _running;
} CPU;

CPU *cpu_create(BusInterface *bus);
void cpu_free    (CPU *cpu);
void cpu_reset   (CPU *cpu);
void cpu_wait    (CPU *cpu);
void cpu_shutdown(CPU *cpu);

void cpu_signal_to_cpu(CPU *cpu, SignalType signal, Edge edge);
void cpu_signal_handler_add(CPU *cpu, SignalType signal, SignalCallback callback);
void cpu_signal_handler_remove(CPU *cpu, SignalType signal, SignalCallback callback);
