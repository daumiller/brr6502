#pragma once
#include "include/std.h"

typedef struct {
  u8   (*read) (u16 address);
  void (*write)(u16 address, u8 data);
} BusInterface;
