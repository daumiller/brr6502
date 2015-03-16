#include "cpu/internal.h"

void cpu_signal_to_cpu(CPU *cpu, SignalType signal, Edge edge) {
  u8 current_value = cpu->_signal_state & signal;
  if((current_value > 0) == edge) { return; }

  cpu->_signal_status |= signal;
  if(edge == positive) {
    cpu->_signal_state |= signal;
  } else {
    cpu->_signal_state &= ~signal;
  }

  // if halted, and RDY transition from low to high, signal CPU to resume running
  if(cpu->_halted && signal_changed_to(SIGNAL_TYPE_RDY, SIGNAL_TYPE_RDY)) {
    pthread_mutex_lock    (&(cpu->_unhalt_mutex));
    pthread_cond_broadcast(&(cpu->_unhalt_condition));
    pthread_mutex_unlock  (&(cpu->_unhalt_mutex));
  }

  // if waiting, and IRQB transition from high to low, signal CPU to resume running
  if(cpu->_waiting && signal_changed_to(SIGNAL_TYPE_IRQB, 0)) {
    pthread_mutex_lock    (&(cpu->_waiting_mutex));
    pthread_cond_broadcast(&(cpu->_waiting_condition));
    pthread_mutex_unlock  (&(cpu->_waiting_mutex));
  }
}

void cpu_signal_handler_add(CPU *cpu, SignalType signal, SignalCallback callback) {
  SignalList **list = NULL;
  switch(signal) {
    case SIGNAL_TYPE_RDY : list = &(cpu->_edge_RDY) ; break;
    case SIGNAL_TYPE_SYNC: list = &(cpu->_edge_SYNC); break;
    default: return;
  }

  SignalList *new_handler = (SignalList *)malloc(sizeof(SignalList));
  new_handler->prev   = NULL;
  new_handler->next   = NULL;
  new_handler->signal = callback;

  if(*list == NULL) {
    *list = new_handler;
  } else {
    SignalList *curr = *list;
    while(curr->next != NULL) { curr = curr->next; }
    curr->next = new_handler;
    new_handler->prev = curr;
  }
}

void cpu_signal_handler_remove(CPU *cpu, SignalType signal, SignalCallback callback) {
  SignalList **list = NULL;
  switch(signal) {
    case SIGNAL_TYPE_RDY : list = &(cpu->_edge_RDY) ; break;
    case SIGNAL_TYPE_SYNC: list = &(cpu->_edge_SYNC); break;
    default: return;
  }

  if(*list == NULL) { return; }

  SignalList *curr = *list, *tmp;
  while(curr) {
    if(curr->signal == callback) {
      if(curr->next) { curr->next->prev = curr->prev; }
      if(curr->prev) { curr->prev->next = curr->next; } else { *list = curr->next; }
      tmp = curr; curr = curr->next; free(tmp);
    } else {
      curr = curr->next;
    }
  }
}

void _cpu_signal_from_cpu(CPU *cpu, SignalType signal, Edge edge) {
  SignalList *handler = NULL;
  switch(signal) {
    case SIGNAL_TYPE_RDY : handler = cpu->_edge_RDY;  break;
    case SIGNAL_TYPE_SYNC: handler = cpu->_edge_SYNC; break;
    default: return;
  }

  while(handler) {
    handler->signal((void *)cpu, signal, edge);
    handler = handler->next;
  }
}
