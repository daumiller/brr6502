#include "cpu/internal.h"

//==================================================================
extern OpCode      op_code_table[256];
extern AddressMode address_mode_table[256];

//==================================================================
static void  _cpu_reset_shutdown(CPU *cpu, bool reset);
static void *_cpu_cycle_thread(void *cpu_ptr);
static void  _cpu_interrupt_prepare(CPU *cpu, bool maskable, bool soft);
static void  _cpu_fetch_operation(CPU *cpu, Operation *op);
static void  _cpu_execute_operation(CPU *cpu, Operation *op);
static u8 _op_read_operand(CPU *cpu, Operation *op);

//==================================================================

CPU *cpu_create(BusInterface *bus) {
  CPU *cpu = (CPU *)calloc(sizeof(CPU), 1);
  cpu->_signal_state = SIGNALS_AT_BOOT;
  cpu->_bus = bus;
  cpu->_running = false;
  cpu->_waiting = false;
  cpu->_halted  = false;
  pthread_mutex_init(&(cpu->_unhalt_mutex)     , NULL);
  pthread_cond_init (&(cpu->_unhalt_condition) , NULL);
  pthread_mutex_init(&(cpu->_waiting_mutex)    , NULL);
  pthread_cond_init (&(cpu->_waiting_condition), NULL);
  pthread_mutex_init(&(cpu->_running_mutex)    , NULL);
  pthread_cond_init (&(cpu->_running_condition), NULL);
  return cpu;
}

void cpu_free(CPU *cpu) {
  pthread_mutex_destroy(&(cpu->_unhalt_mutex));
  pthread_cond_destroy (&(cpu->_unhalt_condition));
  pthread_mutex_destroy(&(cpu->_waiting_mutex));
  pthread_cond_destroy (&(cpu->_waiting_condition));
  pthread_mutex_destroy(&(cpu->_running_mutex));
  pthread_cond_destroy (&(cpu->_running_condition));
  free(cpu);
}

void cpu_wait(CPU *cpu) {
  if(cpu->_running == false) { return; }

  pthread_mutex_lock(&(cpu->_running_mutex));
  pthread_cond_wait(&(cpu->_running_condition), &(cpu->_running_mutex));
  pthread_mutex_unlock(&(cpu->_running_mutex));
}

void cpu_reset(CPU *cpu) {
  _cpu_reset_shutdown(cpu, true);
}

void cpu_shutdown(CPU *cpu) {
  _cpu_reset_shutdown(cpu, false);
}

void _cpu_reset_shutdown(CPU *cpu, bool reset) {
  if(cpu->_halted) {
    pthread_mutex_lock    (&(cpu->_unhalt_mutex));
    pthread_cond_broadcast(&(cpu->_unhalt_condition));
    pthread_mutex_unlock  (&(cpu->_unhalt_mutex));
  }
  if(cpu->_running == true) {
    pthread_mutex_lock(&(cpu->_running_mutex));
    cpu->_running = false;
    pthread_cond_wait(&(cpu->_running_condition), &(cpu->_running_mutex));
    pthread_mutex_unlock(&(cpu->_running_mutex));
  }

  if(reset == false) { return; }
  pthread_create(&(cpu->_cycle_thread), NULL, &_cpu_cycle_thread, (void *)cpu);
}

//==================================================================

void *_cpu_cycle_thread(void *cpu_ptr) {
  CPU *cpu = (CPU *)cpu_ptr;
  cpu->_running = true;

  Operation op;
  cpu->pc = _cpu_read_vector(cpu, VECTOR_RESET);
  cpu->p  = STATUS_AT_BOOT;

  while(cpu->_running) {
    _cpu_fetch_operation(cpu, &op);
    _cpu_execute_operation(cpu, &op);
  }

  // _running has been set to false
  // signal listener that we are exiting the thread
  pthread_mutex_lock(&(cpu->_running_mutex));
  pthread_cond_broadcast(&(cpu->_running_condition));
  pthread_mutex_unlock(&(cpu->_running_mutex));

  return NULL;
}

//==================================================================

void _cpu_interrupt_prepare(CPU *cpu, bool maskable, bool soft) {
  _cpu_status_change(cpu, STATUS_BREAK, soft);
  _cpu_stack_push(cpu, BYTE_LO(cpu->pc));
  _cpu_stack_push(cpu, BYTE_HI(cpu->pc));
  _cpu_stack_push(cpu, cpu->p);
  if(maskable) { status_set(STATUS_IRQB); }
  u16 address = maskable ? VECTOR_IRQ : VECTOR_NMI;
  cpu->pc = bus_read16(address);
}

//==================================================================

void _cpu_fetch_operation(CPU *cpu, Operation *op) {
  _cpu_signal_from_cpu(cpu, SIGNAL_TYPE_SYNC, positive);
  if(signal_changed_to(SIGNAL_TYPE_RDY, 0)) {
    bit_clear(cpu->_signal_status, SIGNAL_TYPE_RDY);
    // RDY pulled low during SYNC high - halt until signal returns
    pthread_mutex_lock(&(cpu->_unhalt_mutex));
    cpu->_halted = true;
    pthread_cond_wait(&(cpu->_unhalt_condition), &(cpu->_unhalt_mutex));
    cpu->_halted = false;
    pthread_mutex_unlock(&(cpu->_unhalt_mutex));
    bit_clear(cpu->_signal_status, SIGNAL_TYPE_RDY);
  }
  _cpu_signal_from_cpu(cpu, SIGNAL_TYPE_SYNC, negative);

  if(signal_changed_to(SIGNAL_TYPE_NMIB, 0)) {
    bit_clear(cpu->_signal_status, SIGNAL_TYPE_NMIB);
    _cpu_interrupt_prepare(cpu, false, false);
  } else if(status_is_clear(STATUS_IRQB) && signal_changed_to(SIGNAL_TYPE_IRQB, 0)) {
    bit_clear(cpu->_signal_status, SIGNAL_TYPE_IRQB);
    _cpu_interrupt_prepare(cpu, true, false);
  }

  u8 byte0 = bus_read(cpu->pc); cpu->pc++;
  op->op   = op_code_table[byte0];
  op->mode = VALUE_MODE_ADDRESS;

  // these values must be in the same order as the AddressMode enum
  static void *jump_table[] = {
    &&jmp_ABSOLUTE,
    &&jmp_ABSOLUTE_INDEXED_INDIRECT,
    &&jmp_ABSOLUTE_INDEXED_X,
    &&jmp_ABSOLUTE_INDEXED_Y,
    &&jmp_ABSOLUTE_INDRECT,
    &&jmp_ACCUMULATOR,
    &&jmp_IMMEDIATE,
    &&jmp_IMPLIED,
    &&jmp_RELATIVE,
    &&jmp_STACK,
    &&jmp_ZERO_PAGE,
    &&jmp_ZERO_PAGE_INDEXED_INDIRECT,
    &&jmp_ZERO_PAGE_INDEXED_X,
    &&jmp_ZERO_PAGE_INDEXED_Y,
    &&jmp_ZERO_PAGE_INDIRECT,
    &&jmp_ZERO_PAGE_INDIRECT_INDEXED_Y,
    &&jmp_ZERO_PAGE_RELATIVE,
    &&jmp_xxx
  };

  goto *jump_table[address_mode_table[byte0]];

  jmp_ABSOLUTE:
    op->address = bus_read16(cpu->pc);
    cpu->pc += 2;
    return;

  jmp_ABSOLUTE_INDEXED_INDIRECT:
    op->address = bus_read16(cpu->pc) + cpu->x;
    cpu->pc += 2;
    op->address = bus_read16(op->address);
    return;

  jmp_ABSOLUTE_INDEXED_X:
    op->address = bus_read16(cpu->pc) + cpu->x;
    cpu->pc += 2;
    return;

  jmp_ABSOLUTE_INDEXED_Y:
    op->address = bus_read16(cpu->pc) + cpu->y;
    cpu->pc += 2;
    return;

  jmp_ABSOLUTE_INDRECT:
    op->address = bus_read16(cpu->pc);
    cpu->pc += 2;
    op->address = bus_read16(op->address);
    return;

  jmp_ACCUMULATOR:
    op->reference = &(cpu->a);
    op->mode = VALUE_MODE_REGISTER;
    return;

  jmp_IMMEDIATE:
    op->value = bus_read(cpu->pc);
    cpu->pc++;
    op->mode = VALUE_MODE_IMMEDIATE;
    return;

  jmp_IMPLIED:
    return;

  jmp_RELATIVE:
    op->value = bus_read(cpu->pc);
    cpu->pc++;
    op->address = cpu->pc + u8_as_i8(op->value);
    return;

  jmp_STACK:
    // this is really identical to AddressMode.IMPLIED
    return;

  jmp_ZERO_PAGE:
    op->address = (u16)bus_read(cpu->pc);
    cpu->pc++;
    return;

  jmp_ZERO_PAGE_INDEXED_INDIRECT:
    op->address = (u16)(bus_read(cpu->pc) + cpu->x);
    cpu->pc++;
    op->address = bus_read16(op->address);
    return;

  jmp_ZERO_PAGE_INDEXED_X:
    op->address = (u16)(bus_read(cpu->pc) + cpu->x);
    cpu->pc++;
    return;

  jmp_ZERO_PAGE_INDEXED_Y:
    op->address = (u16)(bus_read(cpu->pc) + cpu->y);
    cpu->pc++;
    return;

  jmp_ZERO_PAGE_INDIRECT:
    op->address = (u16)(bus_read(cpu->pc));
    cpu->pc++;
    op->address = bus_read16(op->address);
    return;

  jmp_ZERO_PAGE_INDIRECT_INDEXED_Y:
    op->address = (u16)(bus_read(cpu->pc));
    cpu->pc++;
    op->address = bus_read16(op->address) + (u16)cpu->y;
    return;

  jmp_ZERO_PAGE_RELATIVE:
    op->address = (u16)(bus_read(cpu->pc));
    cpu->pc++;
    op->value = bus_read(cpu->pc);
    cpu->pc++;
    return;

  jmp_xxx:
    printf("Invalid Address Mode for Byte Code %02X\n", byte0);
    exit(-1);
}

//==================================================================

static u8 inline _op_read_operand(CPU *cpu, Operation *op) {
  switch(op->mode) {
    case VALUE_MODE_ADDRESS   : return bus_read(op->address);
    case VALUE_MODE_IMMEDIATE : return op->value;
    case VALUE_MODE_REGISTER  : return *(op->reference);
  }
}

static void inline _op_write_operand(CPU *cpu, Operation *op, u8 value) {
  switch(op->mode) {
    case VALUE_MODE_ADDRESS   : bus_write(op->address, value); return;
    case VALUE_MODE_IMMEDIATE : return; // intentionally ignored
    case VALUE_MODE_REGISTER  : *(op->reference) = value;      return;
  }
}

static void inline _op_branch_if_status(CPU *cpu, Operation *op, u8 status, u8 value) {
  if((cpu->p & status) == value) { cpu->pc = op->address; }
}

#define STATUS_CHECK_CARRY(x)      status_change(STATUS_CARRY    , x > 0x00FF)
#define STATUS_CHECK_ZERO(x)       status_change(STATUS_ZERO     , x == 0)
#define STATUS_CHECK_OVERFLOW(x,y) status_change(STATUS_OVERFLOW , (x & 0x80) != (y & 0x80))
#define STATUS_CHECK_NEGATIVE(x)   status_change(STATUS_NEGATIVE , (x & 0x80) > 0)

void _cpu_execute_operation(CPU *cpu, Operation *op) {

  // these values must be in the same order as the OpCode enum
  static void *jump_table[] = {
    &&jmp_ADC, &&jmp_AND, &&jmp_ASL, &&jmp_BCC, &&jmp_BCS, &&jmp_BEQ, &&jmp_BIT, &&jmp_BMI,
    &&jmp_BNE, &&jmp_BPL, &&jmp_BRA, &&jmp_BRK, &&jmp_BVC, &&jmp_BVS, &&jmp_CLC, &&jmp_CLD,
    &&jmp_CLI, &&jmp_CLV, &&jmp_CMP, &&jmp_CPX, &&jmp_CPY, &&jmp_DEC, &&jmp_DEX, &&jmp_DEY,
    &&jmp_INC, &&jmp_INX, &&jmp_INY, &&jmp_JMP, &&jmp_JSR, &&jmp_LDA, &&jmp_LDX, &&jmp_LDY,
    &&jmp_LSR, &&jmp_NOP, &&jmp_ORA, &&jmp_PHA, &&jmp_PHP, &&jmp_PHX, &&jmp_PHY, &&jmp_PLA,
    &&jmp_PLP, &&jmp_PLX, &&jmp_PLY, &&jmp_ROL, &&jmp_ROR, &&jmp_RTI, &&jmp_RTS, &&jmp_SBC,
    &&jmp_SEC, &&jmp_SED, &&jmp_SEI, &&jmp_STA, &&jmp_STP, &&jmp_STX, &&jmp_STY, &&jmp_STZ,
    &&jmp_TAX, &&jmp_TAY, &&jmp_TRB, &&jmp_TSB, &&jmp_TSX, &&jmp_TXA, &&jmp_TXS, &&jmp_TYA,
    &&jmp_WAI, &&jmp_XOR,
    &&jmp_BBR0, &&jmp_BBR1, &&jmp_BBR2, &&jmp_BBR3, &&jmp_BBR4, &&jmp_BBR5, &&jmp_BBR6, &&jmp_BBR7,
    &&jmp_BBS0, &&jmp_BBS1, &&jmp_BBS2, &&jmp_BBS3, &&jmp_BBS4, &&jmp_BBS5, &&jmp_BBS6, &&jmp_BBS7,
    &&jmp_RMB0, &&jmp_RMB1, &&jmp_RMB2, &&jmp_RMB3, &&jmp_RMB4, &&jmp_RMB5, &&jmp_RMB6, &&jmp_RMB7,
    &&jmp_SMB0, &&jmp_SMB1, &&jmp_SMB2, &&jmp_SMB3, &&jmp_SMB4, &&jmp_SMB5, &&jmp_SMB6, &&jmp_SMB7,
    &&jmp_xxx
  };

  goto *jump_table[op->op];

  jmp_ADC: {
    // Add Memory to Accumulator with Carry
    // N V Z C
    u8 operand = _op_read_operand(cpu, op);
    u16 result = ((u16)operand) + ((u16)cpu->a) + (status_is_set(STATUS_CARRY) ? 1 : 0);

    STATUS_CHECK_NEGATIVE(result);
    STATUS_CHECK_OVERFLOW(result, (u16)cpu->a);
    STATUS_CHECK_ZERO(result);
    STATUS_CHECK_CARRY(result);

    cpu->a = (u8)(result & 0x00FF);
    return;
  }

  jmp_AND: {
    // And Memory with Accumulator
    // N Z
    cpu->a &= _op_read_operand(cpu, op);
    STATUS_CHECK_NEGATIVE(cpu->a);
    STATUS_CHECK_ZERO(cpu->a);
    return;
  }

  jmp_ASL: {
    // Arithmetic Shift Left, Memory or Accumulator
    // N Z C
    u8 operand = _op_read_operand(cpu, op);
    status_change(STATUS_CARRY, (operand & 0x80) == 0x80);
    operand <<= 1;
    STATUS_CHECK_NEGATIVE(operand);
    STATUS_CHECK_ZERO(operand);
    _op_write_operand(cpu, op, operand);
    return;
  }

  jmp_BCC: {
    // Branch on Carry Clear
    _op_branch_if_status(cpu, op, STATUS_CARRY, 0);
    return;
  }

  jmp_BCS: {
    // Branch on Carry Set
    _op_branch_if_status(cpu, op, STATUS_CARRY, STATUS_CARRY);
    return;
  }

  jmp_BEQ: {
    // Branch if Equal
    _op_branch_if_status(cpu, op, STATUS_ZERO, STATUS_ZERO);
    return;
  }

  jmp_BIT: {
    // Bit Test
    // N V Z  not-immediate
    // - - Z  immediate
    u8 operand = _op_read_operand(cpu, op);
    if(op->mode != VALUE_MODE_IMMEDIATE) {
      status_change(STATUS_NEGATIVE, (operand & 0x80) == 0x80);
      status_change(STATUS_OVERFLOW, (operand & 0x40) == 0x40);
    }
    status_change(STATUS_ZERO, (operand & cpu->a) == 0);
    return;
  }

  jmp_BMI: {
    // Branch if result Minus/negative
    _op_branch_if_status(cpu, op, STATUS_NEGATIVE, STATUS_NEGATIVE);
    return;
  }

  jmp_BNE: {
    // Branch if Not Equal
    _op_branch_if_status(cpu, op, STATUS_ZERO, 0);
    return;
  }

  jmp_BPL: {
    // Branch if result Plus/positive
    _op_branch_if_status(cpu, op, STATUS_NEGATIVE, 0);
    return;
  }

  jmp_BRA: {
    // Branch Always / unconditional branch
    cpu->pc = op->address;
    return;
  }

  jmp_BRK: {
    // Break / soft interrupt
    // B
    cpu->pc += 1; // BRK always adds 2 to PC (one byte used in fetch, one added here)
    _cpu_interrupt_prepare(cpu, true, true);
    return;
  }

  jmp_BVC: {
    // Branch if Overflow Clear
    _op_branch_if_status(cpu, op, STATUS_OVERFLOW, 0);
    return;
  }

  jmp_BVS: {
    // Branch if Overflow Set
    _op_branch_if_status(cpu, op, STATUS_OVERFLOW, STATUS_OVERFLOW);
    return;
  }

  jmp_CLC: {
    // Clear Carry Flag
    // C
    status_clear(STATUS_CARRY);
    return;
  }

  jmp_CLD: {
    // Clear Decimal Flag - UNSUPPORTED
    // D
    return;
  }

  jmp_CLI: {
    // Clear Interrupt Disable Flag
    // I
    status_clear(STATUS_IRQB);
    return;
  }

  jmp_CLV: {
    // Clear Overflow Flag
    // V
    status_clear(STATUS_OVERFLOW);
    return;
  }

  jmp_CMP: {
    // Compare Memory and Accumulator
    // N Z C
    u8 operand = _op_read_operand(cpu, op);
    u8 result = cpu->a - operand;
    STATUS_CHECK_NEGATIVE(result);
    STATUS_CHECK_ZERO(result);
    status_change(STATUS_CARRY, cpu->a >= operand);
    return;
  }

  jmp_CPX: {
    // Compare Memory and X
    // N Z C
    u8 operand = _op_read_operand(cpu, op);
    u8 result = cpu->x - operand;
    STATUS_CHECK_NEGATIVE(result);
    STATUS_CHECK_ZERO(result);
    status_change(STATUS_CARRY, cpu->x >= operand);
    return;
  }

  jmp_CPY: {
    // Compare Memory and Y
    // N Z C
    u8 operand = _op_read_operand(cpu, op);
    u8 result = cpu->y - operand;
    STATUS_CHECK_NEGATIVE(result);
    STATUS_CHECK_ZERO(result);
    status_change(STATUS_CARRY, cpu->y >= operand);
    return;
  }

  jmp_DEC: {
    // Decrement Memory or Accumulator
    // N Z
    u8 result = _op_read_operand(cpu, op) - 1;
    _op_write_operand(cpu, op, result);
    STATUS_CHECK_NEGATIVE(result);
    STATUS_CHECK_ZERO(result);
    return;
  }

  jmp_DEX: {
    // Decrement X
    // N Z
    cpu->x -= 1;
    STATUS_CHECK_NEGATIVE(cpu->x);
    STATUS_CHECK_ZERO(cpu->x);
    return;
  }

  jmp_DEY: {
    // Decrement Y
    // N Z
    cpu->y -= 1;
    STATUS_CHECK_NEGATIVE(cpu->y);
    STATUS_CHECK_ZERO(cpu->y);
    return;
  }

  jmp_INC: {
    // Increment Memory or Accumulator
    // N Z
    u8 result = _op_read_operand(cpu, op) + 1;
    _op_write_operand(cpu, op, result);
    STATUS_CHECK_NEGATIVE(result);
    STATUS_CHECK_ZERO(result);
    return;
  }

  jmp_INX: {
    // Increment X
    // N Z
    cpu->x += 1;
    STATUS_CHECK_NEGATIVE(cpu->x);
    STATUS_CHECK_ZERO(cpu->x);
    return;
  }

  jmp_INY: {
    // Increment Y
    // N Z
    cpu->y += 1;
    STATUS_CHECK_NEGATIVE(cpu->y);
    STATUS_CHECK_ZERO(cpu->y);
    return;
  }

  jmp_JMP: {
    // Jump to location
    cpu->pc = op->address;
    return;
  }

  jmp_JSR: {
    // Jump to Subroutine
    cpu->pc -= 1;  // Subroutines deal with (next_ins_address-1), as little sense as that may make...
    _cpu_stack_push(cpu, BYTE_LO(cpu->pc));
    _cpu_stack_push(cpu, BYTE_HI(cpu->pc));
    cpu->pc = op->address;
    return;
  }

  jmp_LDA: {
    // Load Accumulator with Memory
    // N Z
    cpu->a = _op_read_operand(cpu, op);
    STATUS_CHECK_NEGATIVE(cpu->a);
    STATUS_CHECK_ZERO(cpu->a);
    return;
  }

  jmp_LDX: {
    // Load X with Memory
    // N Z
    cpu->x = _op_read_operand(cpu, op);
    STATUS_CHECK_NEGATIVE(cpu->x);
    STATUS_CHECK_ZERO(cpu->x);
    return;
  }

  jmp_LDY: {
    // Load Y with Memory
    // N Z
    cpu->y = _op_read_operand(cpu, op);
    STATUS_CHECK_NEGATIVE(cpu->y);
    STATUS_CHECK_ZERO(cpu->y);
    return;
  }

  jmp_LSR: {
    // Logical Shift Right, Memory or Accumulator
    // N Z C
    u8 operand = _op_read_operand(cpu, op);
    status_change(STATUS_CARRY, (operand & 1) == 1);
    operand >>= 1;
    STATUS_CHECK_NEGATIVE(operand);
    STATUS_CHECK_ZERO(operand);
    _op_write_operand(cpu, op, operand);
    return;
  }

  jmp_NOP: {
    // No Operation / stall
    return;
  }

  jmp_ORA: {
    // Or Memory with Accumulator
    // N Z
    cpu->a |= _op_read_operand(cpu, op);
    STATUS_CHECK_NEGATIVE(cpu->a);
    STATUS_CHECK_ZERO(cpu->a);
    return;
  }

  jmp_PHA: {
    // Push Accumulator to Stack
    _cpu_stack_push(cpu, cpu->a);
    return;
  }

  jmp_PHP: {
    // Push P to Stack
    _cpu_stack_push(cpu, cpu->p);
    return;
  }

  jmp_PHX: {
    // Push X to Stack
    _cpu_stack_push(cpu, cpu->x);
    return;
  }

  jmp_PHY: {
    // Push Y to Stack
    _cpu_stack_push(cpu, cpu->y);
    return;
  }

  jmp_PLA: {
    // Pull Accumulator from Stack
    // N Z
    cpu->a = _cpu_stack_pull(cpu);
    STATUS_CHECK_NEGATIVE(cpu->a);
    STATUS_CHECK_ZERO(cpu->a);
    return;
  }

  jmp_PLP: {
    // Pull P from Stack
    // N V B D I Z C
    cpu->p = _cpu_stack_pull(cpu);
    return;
  }

  jmp_PLX: {
    // Pull X from Stack
    // N Z
    cpu->x = _cpu_stack_pull(cpu);
    STATUS_CHECK_NEGATIVE(cpu->x);
    STATUS_CHECK_ZERO(cpu->x);
    return;
  }

  jmp_PLY: {
    // Pull Y from Stack
    // N Z
    cpu->y = _cpu_stack_pull(cpu);
    STATUS_CHECK_NEGATIVE(cpu->y);
    STATUS_CHECK_ZERO(cpu->y);
    return;
  }

  jmp_ROL: {
    // Rotate Bit Left, Memory or Accumulator
    // N Z C
    u8 operand = _op_read_operand(cpu, op);
    bool set_carry = ((operand & 0x80) == 0x80);
    operand <<= 1;
    if(status_is_set(STATUS_CARRY)) { operand |= 1; }
    status_change(STATUS_CARRY, set_carry);
    STATUS_CHECK_NEGATIVE(operand);
    STATUS_CHECK_ZERO(operand);
    _op_write_operand(cpu, op, operand);
    return;
  }

  jmp_ROR: {
    // Rotate Bit Right, Memory or Accumulator
    // N Z C
    u8 operand = _op_read_operand(cpu, op);
    bool set_carry = ((operand & 1) == 1);
    operand >>= 1;
    if(status_is_set(STATUS_CARRY)) { operand |= 0x80; }
    status_change(STATUS_CARRY, set_carry);
    STATUS_CHECK_NEGATIVE(operand);
    STATUS_CHECK_ZERO(operand);
    _op_write_operand(cpu, op, operand);
    return;
  }

  jmp_RTI: {
    // Return from Interrupt
    // N V B D I Z C
    status_clear(STATUS_IRQB);
    cpu->p = _cpu_stack_pull(cpu);
    cpu->pc = _cpu_stack_pull(cpu);
    cpu->pc <<= 8;
    cpu->pc |= _cpu_stack_pull(cpu);
    return;
  }

  jmp_RTS: {
    // Return from Subroutine
    cpu->pc = _cpu_stack_pull(cpu);
    cpu->pc <<= 8;
    cpu->pc |= _cpu_stack_pull(cpu);
    cpu->pc += 1;
    return;
  }

  jmp_SBC: {
    // Subtract Memory from Accumulator with Borrow (Carry)
    // N V Z C
    u8 operand = _op_read_operand(cpu, op);
    u16 result = ((u16)cpu->a) - ((u16)operand);
    if(status_is_clear(STATUS_CARRY)) { result -= 1; }

    STATUS_CHECK_NEGATIVE(result);
    STATUS_CHECK_OVERFLOW(result, (u16)cpu->a);
    STATUS_CHECK_ZERO(result);
    STATUS_CHECK_CARRY(result);

    cpu->a = (u8)(result & 0x00FF);
    return;
  }

  jmp_SEC: {
    // Set Carry Flag
    status_set(STATUS_CARRY);
    return;
  }

  jmp_SED: {
    // Set Decimal Flag - UNSUPPORTED
    return;
  }

  jmp_SEI: {
    // Set Interrupt Disable Flag
    status_set(STATUS_IRQB);
    return;
  }

  jmp_STA: {
    // Store Accumulator in Memory
    _op_write_operand(cpu, op, cpu->a);
    return;
  }

  jmp_STP: {
    // Stop Mode
    // for 65c02, halt cpu, waiting for reset
    // we basically do the same thing, but we exit the cycle thread
    cpu->_running = false;
    return;
  }

  jmp_STX: {
    // Store X in Memory
    _op_write_operand(cpu, op, cpu->x);
    return;
  }

  jmp_STY: {
    // Store Y in Memory
    _op_write_operand(cpu, op, cpu->y);
    return;
  }

  jmp_STZ: {
    // Store Zero in Memory
    _op_write_operand(cpu, op, 0);
    return;
  }

  jmp_TAX: {
    // Transfer Accumulator to X
    // N Z
    cpu->x = cpu->a;
    STATUS_CHECK_NEGATIVE(cpu->x);
    STATUS_CHECK_ZERO(cpu->x);
    return;
  }

  jmp_TAY: {
    // Transfer Accumulator to Y
    cpu->y = cpu->a;
    STATUS_CHECK_NEGATIVE(cpu->y);
    STATUS_CHECK_ZERO(cpu->y);
    return;
  }

  jmp_TRB: {
    // Test and Reset Memory Bit
    // Z
    u8 operand = _op_read_operand(cpu, op);
    status_change(STATUS_ZERO, (operand & cpu->a) == 0); // p.Z based on seperate operation than the saved result
    operand &= ~(cpu->a);
    _op_write_operand(cpu, op, operand);
    return;
  }

  jmp_TSB: {
    // Test and Set Memory Bit
    // Z
    u8 operand = _op_read_operand(cpu, op);
    status_change(STATUS_ZERO, (operand & cpu->a) == 0); // p.Z based on seperate operation than the saved result
    operand |= cpu->a;
    _op_write_operand(cpu, op, operand);
    return;
  }

  jmp_TSX: {
    // Transfer Stack Pointer to X
    // N Z
    cpu->x = cpu->s;
    STATUS_CHECK_NEGATIVE(cpu->x);
    STATUS_CHECK_ZERO(cpu->x);
    return;
  }

  jmp_TXA: {
    // Transfer X to Accumulator
    // N Z
    cpu->a = cpu->x;
    STATUS_CHECK_NEGATIVE(cpu->a);
    STATUS_CHECK_ZERO(cpu->a);
    return;
  }

  jmp_TXS: {
    // Transfer X to Stack Pointer
    // N Z
    cpu->s = cpu->x;
    return;
  }

  jmp_TYA: {
    // Transfer Y to Accumulator
    // N Z
    cpu->a = cpu->y;
    STATUS_CHECK_NEGATIVE(cpu->a);
    STATUS_CHECK_ZERO(cpu->a);
    return;
  }

  jmp_WAI: {
    // Halt execution until next IRQB signal
    _cpu_signal_from_cpu(cpu, SIGNAL_TYPE_RDY, negative);
    pthread_mutex_lock(&(cpu->_waiting_mutex));
    cpu->_waiting = true;
    pthread_cond_wait(&(cpu->_waiting_condition), &(cpu->_waiting_mutex));
    cpu->_waiting = false;
    pthread_mutex_unlock(&(cpu->_waiting_mutex));
    _cpu_signal_from_cpu(cpu, SIGNAL_TYPE_RDY, positive);
    return;
  }

  jmp_XOR: {
    // XOR Memory with Accumulator
    // N Z
    cpu->a ^= _op_read_operand(cpu, op);
    STATUS_CHECK_NEGATIVE(cpu->a);
    STATUS_CHECK_ZERO(cpu->a);
    return;
  }

  jmp_BBR0: {
    // Branch if Bit 0 Reset
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if((operand & 1) == 0) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBR1: {
    // Branch if Bit 1 Reset
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if((operand & 2) == 0) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBR2: {
    // Branch if Bit 2 Reset
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if((operand & 4) == 0) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBR3: {
    // Branch if Bit 3 Reset
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if((operand & 8) == 0) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBR4: {
    // Branch if Bit 4 Reset
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if((operand & 16) == 0) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBR5: {
    // Branch if Bit 5 Reset
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if((operand & 32) == 0) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBR6: {
    // Branch if Bit 6 Reset
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if((operand & 64) == 0) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBR7: {
    // Branch if Bit 7 Reset
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if((operand & 128) == 0) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBS0: {
    // Branch if Bit 0 Set
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if(operand & 1) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBS1: {
    // Branch if Bit 1 Set
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if(operand & 2) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBS2: {
    // Branch if Bit 2 Set
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if(operand & 4) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBS3: {
    // Branch if Bit 3 Set
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if(operand & 8) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBS4: {
    // Branch if Bit 4 Set
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if(operand & 16) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBS5: {
    // Branch if Bit 5 Set
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if(operand & 32) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBS6: {
    // Branch if Bit 6 Set
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if(operand & 64) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_BBS7: {
    // Branch if Bit 7 Set
    u8 operand = _op_read_operand(cpu, op);
    i16 displacement = u8_as_i16(op->value);
    if(operand & 128) { cpu->pc = (u16)((i32)cpu->pc + (i32)displacement); }
    return;
  }

  jmp_RMB0: {
    // Reset Memory Bit 0
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) & (~1));
    return;
  }

  jmp_RMB1: {
    // Reset Memory Bit 1
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) & (~2));
    return;
  }

  jmp_RMB2: {
    // Reset Memory Bit 2
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) & (~4));
    return;
  }

  jmp_RMB3: {
    // Reset Memory Bit 3
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) & (~8));
    return;
  }

  jmp_RMB4: {
    // Reset Memory Bit 4
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) & (~16));
    return;
  }

  jmp_RMB5: {
    // Reset Memory Bit 5
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) & (~32));
    return;
  }

  jmp_RMB6: {
    // Reset Memory Bit 6
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) & (~64));
    return;
  }

  jmp_RMB7: {
    // Reset Memory Bit 7
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) & (~128));
    return;
  }

  jmp_SMB0: {
    // Set Memory Bit 0
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) | 1);
    return;
  }

  jmp_SMB1: {
    // Set Memory Bit 1
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) | 2);
    return;
  }

  jmp_SMB2: {
    // Set Memory Bit 2
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) | 4);
    return;
  }

  jmp_SMB3: {
    // Set Memory Bit 3
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) | 8);
    return;
  }

  jmp_SMB4: {
    // Set Memory Bit 4
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) | 16);
    return;
  }

  jmp_SMB5: {
    // Set Memory Bit 5
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) | 32);
    return;
  }

  jmp_SMB6: {
    // Set Memory Bit 6
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) | 64);
    return;
  }

  jmp_SMB7: {
    // Set Memory Bit 7
    _op_write_operand(cpu, op, _op_read_operand(cpu, op) | 128);
    return;
  }

  jmp_xxx: {
    printf("Invalid OpCode for Byte Code %02X\n", op->op);
    exit(-1);
  }
}
