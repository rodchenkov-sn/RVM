#ifndef RVM_HPP
#define RVM_HPP

#include <array>
#include <vector>
#include <string>
#include <cassert>
#include <iostream>

#pragma warning( push             )
#pragma warning( disable : C26451 )
#pragma warning( disable : C26812 )
#pragma warning( disable : C4083  )
#pragma warning( disable : C4244  )
#pragma warning( disable : C4334  )

#ifdef RVM_NOEXCEPT
#define EXPECT_REG_EXISTS(reg_n) if ((reg_n) >= RegSize) {\
  return { false, "invalid register at " + std::to_string(registers_[Ip]) };\
}
#else
#define EXPECT_REG_EXISTS(reg_n) if ((reg_n) >= RegSize) {\
  throw std::runtime_error{ "invalid register at " + std::to_string(registers_[Ip]) };\
}
#endif

#define ABORT_IF_DEFAULT default: assert(false);

#if __cplusplus >= 201703L
#define FALLTHROUGH [[fallthrough]]
#define NODISCARD [[nodiscard]]
#else
#define FALLTHROUGH
#define NODISCARD
#endif


constexpr uint64_t operator "" _ull(unsigned long long int x)
{
  return static_cast<uint64_t>(x);
}

constexpr bool logicXor(bool a, bool b)
{
  return (a || b) && !(a && b);
}


class Rvm
{
public:

  struct status_t
  {
    bool ok;
    std::string message;
  };

  explicit Rvm(uint64_t = 10000);


#ifdef RVM_NOEXCEPT
  NODISCARD status_t run(const std::vector<uint8_t>&) noexcept;
#else
  void run(const std::vector<uint8_t>&);
#endif

private:

  enum OpCodes
  {
    Add,
    Sub,

    And,
    Or,
    Xor,
    Not,

    Mov,
    Push,
    Pop,

    Jmp,
    Call,
    Ret,
    Int,

    Cmp,
    Test
  };

  enum Registers
  {
    R0,
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,

    Ir,
    Fg,
    Ip,
    Sp,
    Bp,

    RegSize
  };

  enum MemSize
  {
    Byte,
    Word,
    Dword,
    Qword
  };

  enum Interrupt
  {
    PutC,
    PutS,
    GetC,
    Halt,
    IntSize
  };

  enum Flags
  {
    NegFlag = 1 << 0,
    ZeroFlag = 1 << 1,
    PosFlag = 1 << 2
  };

  void push_(uint64_t, MemSize);
  uint64_t pop_(MemSize);

  void run_interrupt_(Interrupt);
  void update_flags_(uint64_t);

  uint64_t get_num_(MemSize, uint64_t);
  void load_num_(MemSize, uint64_t, uint64_t);
  uint64_t extend_byte_logical_(uint64_t);


  std::array<uint64_t, RegSize> registers_{};
  std::vector<uint8_t> stack_{};
  uint64_t stack_bottom_ = 0;
  bool halted_ = false;
};


__forceinline Rvm::Rvm(uint64_t stackSize) :
  stack_(stackSize)
{
  std::fill(registers_.begin(), registers_.end(), 0);
}

#ifdef RVM_NOEXCEPT
NODISCARD Rvm::status_t Rvm::run(const std::vector<uint8_t>& program) noexcept
#else
void Rvm::run(const std::vector<uint8_t>& program)
#endif
{
  std::copy(program.begin(), program.end(), stack_.begin());
  stack_bottom_ = program.size();
  registers_[Sp] = stack_bottom_;
  registers_[Bp] = stack_bottom_;
  registers_[Ip] = 0;
  while (registers_[Ip] < stack_bottom_ && !halted_) {
    auto opCode = stack_[registers_[Ip]++];
    switch (opCode) {

      //
      //  Add: 8 bit opcode + 4 bit dest reg + 4 bit src reg
      //
      //  for simplicity we cant do something like "add r0, [r1 + 20],
      //  but this con be accomplished with mov, then add
      //

    case Add : FALLTHROUGH

      //
      //  Sub. Similar to add format
      //

    case Sub : FALLTHROUGH

      //
      //  Bitwise And. Simple as it is. Format similar to add
      //

    case And : FALLTHROUGH

      //
      //  Bitwise Or
      //

    case Or : FALLTHROUGH

      //
      //  Bitwise Xor
      //

    case Xor : FALLTHROUGH

      //
      //  Bitwise Not
      //

    case Not : {

      uint8_t dst = stack_[registers_[Ip]] >> 4 & 0xF;
      uint8_t src = stack_[registers_[Ip]] & 0xF;
      EXPECT_REG_EXISTS(dst)
      EXPECT_REG_EXISTS(src)
      switch (opCode) {
      case Add :
        registers_[dst] += registers_[src];
        break;
      case Sub :
        registers_[dst] += (~registers_[src] + 1);
        break;
      case And :
        registers_[dst] &= registers_[src];
        break;
      case Or :
        registers_[dst] |= registers_[src];
        break;
      case Xor :
        registers_[dst] ^= registers_[src];
        break;
      case Not :
        registers_[dst] = ~registers_[src];
        break;
      ABORT_IF_DEFAULT
      }
      update_flags_(registers_[dst]);
      ++registers_[Ip];
      break;
    }

      //
      //  MOVE (COPY)
      //
      //  format: 8 bit opcode | mod + size? + dstReg + | srcReg / num
      //
      //  mode:
      //
      //  00 reg <- num            : opcode | 00 + 00 + (???? - dstReg) | num ... 
      //  01 reg <- reg            : opcode | 01 + 00 + (???? - dstReg) | (???? - srcReg) + 0000
      //  10 reg <- [reg + offset] : opcode | 10 + (?? - movSize) + (???? - dstReg) | (???? - srcReg) +  0000 | num ...
      //  11 [reg + offset] <- reg : opcode | 11 + (?? - movSize) + (???? - dstReg) | (???? - srcReg) + 0000 | num ...
      //
      //  examples:
      //
      //  MOV R0, qword [BP + 10]  ==  00000110 | 10'11'0000 | 1011'00'00 | 00001010
      //  MOV word [BP - 512], R3  ==  00000110 | 11'01'1011 | 0011'01'00 | 11111101 | 11111111
      //

    case Mov : {
      auto fstByte = stack_[registers_[Ip]++];
      auto mode = fstByte >> 6 & 0x3, dstReg = fstByte & 0xF;
      EXPECT_REG_EXISTS(dstReg);
      switch (mode) {
      case 0b00 : {
        registers_[dstReg] = get_num_(Qword, registers_[Ip]);
        registers_[Ip] += 1_ull << 3;
        update_flags_(registers_[dstReg]);
        break;
      }
      case 0b01 : {
        auto srcReg = stack_[registers_[Ip]++] >> 4 & 0xF;
        EXPECT_REG_EXISTS(srcReg);
        registers_[dstReg] = registers_[srcReg];
        update_flags_(registers_[dstReg]);
        break;
      }
      case 0b10 : {
        auto movSize = fstByte >> 4 & 0x3;
        auto srcReg = stack_[registers_[Ip]] >> 4 & 0xF;
        EXPECT_REG_EXISTS(srcReg);
        auto offset = get_num_(Qword, registers_[Ip]);
        registers_[Ip] += 1_ull << 3;
        registers_[dstReg] = get_num_(MemSize(movSize), stack_[registers_[srcReg] + offset]);
        update_flags_(registers_[dstReg]);
        break;
      }
      case 0b11 : {
        auto movSize = fstByte >> 4 & 0x3;
        auto srcReg = stack_[registers_[Ip]++] >> 4 & 0xF;
        EXPECT_REG_EXISTS(srcReg);
        auto offset = get_num_(Qword, registers_[Ip]);
        registers_[Ip] += 1_ull << 3;
        load_num_(MemSize(movSize), registers_[dstReg] + offset, registers_[srcReg]);
        update_flags_(get_num_(MemSize(movSize), registers_[dstReg] + offset));
        break;
      }
      ABORT_IF_DEFAULT
      }
      break;
    }

      //
      //  push value of some size from reg to stack
      //
      //  format: opcode | (????) - srcReg, (??) - size, 00 
      //

    case Push : {
      auto srcReg = stack_[registers_[Ip]] >> 4 & 0xF;
      EXPECT_REG_EXISTS(srcReg);
      auto size = stack_[registers_[Ip]++] >> 2 & 0x3;
      push_(registers_[srcReg], MemSize(size));
      break;
    }

      //
      //  pop value of some size to register
      //
      //  format: opcode | (????) - srcReg, (??) - size, 00
      //

    case Pop : {
      auto dstReg = stack_[registers_[Ip]] >> 4 & 0xF;
      EXPECT_REG_EXISTS(dstReg);
      auto size = stack_[registers_[Ip]++] >> 2 & 0x3;
      registers_[dstReg] = pop_(MemSize(size));
      update_flags_(registers_[dstReg]);
      break;
    }

      //
      //  jump somewhere
      //
      //  format: opcode | (?) - negBit, (??) - mode, 00000 | 64 bit dest
      //
      //  modes: 00 - if true
      //         01 - if neg
      //         10 - if zero
      //         11 - if pos
      //
      //  example: opcode | 10100000 | ... - jump if not neg (jnn)
      //
      //  comment: jump if not true doesn't really exist, so combination of neg bit with 00 mode equivalent to 000
      //

    case Jmp : {
      bool neg = stack_[registers_[Ip]] >> 7 & 0x1;
      auto mode = stack_[registers_[Ip]++] >> 5 & 0x3;
      auto dest = get_num_(Qword, registers_[Ip]);
      registers_[Ip] += 8;

      switch (mode) {
      case 0b00 :
        registers_[Ip] = dest;
        break;
      case 0b01 :
        registers_[Ip] = logicXor(registers_[Fg] & NegFlag, neg) ? dest : registers_[Ip];
        break;
      case 0b10 :
        registers_[Ip] = logicXor(registers_[Fg] & ZeroFlag, neg) ? dest : registers_[Ip];
        break;
      case 0b11 :
        registers_[Ip] = logicXor(registers_[Fg] & PosFlag, neg) ? dest : registers_[Ip];
        break;
      ABORT_IF_DEFAULT
      }
      break;
    }

      //
      //  push IP and jump
      //
      //  format: opcode | 64 bit of dest ip
      //

    case Call : {
      auto dst = get_num_(Qword, registers_[Ip]);
      registers_[Ip] += 8;
      push_(registers_[Ip], Qword);
      registers_[Ip] = dst;
      break;
    }

      //
      //  pop IP and jump
      //
      //  format: opcode
      //

    case Ret : {
      auto dst = pop_(Qword);
      registers_[Ip] = dst;
      break;
    }

      //
      //  run interrupt using Interrupt Register (Ir)
      //
      //  format: opcode | int_num
      //

    case Int : {
      auto intNum = stack_[registers_[Ip]++];
      if (intNum >= IntSize) {
#ifdef RVM_NOEXCEPT
        return { false, "invalid interrupt id at " + std::to_string(registers_[Ip]) };
#else
        throw std::runtime_error{"invalid interrupt id at " + std::to_string(registers_[Ip])};
#endif
      }
      run_interrupt_(Interrupt(intNum));
      break;
    }

      //
      //  sub SndReg from FstReg, update flags, discard result
      //
      //  format: opcode | (????) - FstReg, (????) - SndReg
      //
      //  cmp (5), (5) -> zFlag -> je, jge, jle - true
      //
      //  cmp (5), (4) -> pFlag -> jg, jge, jne - true
      //

    case Cmp : {
      auto fstReg = stack_[registers_[Ip]] >> 4 & 0xF;
      EXPECT_REG_EXISTS(fstReg)
      auto sndReg = stack_[registers_[Ip]++] & 0xF;
      EXPECT_REG_EXISTS(sndReg)
      update_flags_(registers_[fstReg] += ~registers_[sndReg]);
      break;
    }

      //
      //  update flags based on srcReg
      //
      //  format: opcode | (????) - srcReg 0000
      //

    case Test : {
      auto srcReg = stack_[registers_[Ip]++] >> 4 & 0xF;
      EXPECT_REG_EXISTS(srcReg);
      update_flags_(registers_[srcReg]);
      break;
    }

    default :
#ifdef RVM_NOEXCEPT
      return { false, "invalid opcode at " + std::to_string(registers_[Ip]) };
#else
      throw std::runtime_error{"invalid opcode at " + std::to_string(registers_[Ip])};
#endif
    }
  }
#ifdef RVM_NOEXCEPT
  return { true, {} };
#endif
}

__forceinline void Rvm::push_(uint64_t x, MemSize size)
{
  load_num_(size, registers_[Sp], x);
  registers_[Sp] += 1_ull << size;
}

__forceinline uint64_t Rvm::pop_(MemSize size)
{
  registers_[Sp] -= 1_ull << size;
  return get_num_(size, registers_[Sp]);
}

__forceinline void Rvm::update_flags_(uint64_t x)
{
  if (x == 0) {
    registers_[Fg] = ZeroFlag;
  } else if (x >> 63) {
    registers_[Fg] = NegFlag;
  } else {
    registers_[Fg] = PosFlag;
  }
}

uint64_t Rvm::get_num_(MemSize size, uint64_t adr)
{
  uint64_t res = 0;
  switch (size) {
  case Qword :
    res |= extend_byte_logical_(stack_[adr++]) << 56;
    res |= extend_byte_logical_(stack_[adr++]) << 48;
    res |= extend_byte_logical_(stack_[adr++]) << 40;
    res |= extend_byte_logical_(stack_[adr++]) << 32;
    FALLTHROUGH
  case Dword :
    res |= extend_byte_logical_(stack_[adr++]) << 24;
    res |= extend_byte_logical_(stack_[adr++]) << 16;
    FALLTHROUGH
  case Word :
    res |= extend_byte_logical_(stack_[adr++]) << 8;
    FALLTHROUGH
  case Byte :
    res |= extend_byte_logical_(stack_[adr++]);
    break;
  ABORT_IF_DEFAULT
  }
  return res;
}

void Rvm::load_num_(MemSize size, uint64_t adr, uint64_t num)
{
  switch (size) {
  case Qword :
    stack_[adr++] = num >> 56 & 0xFF;
    stack_[adr++] = num >> 48 & 0xFF;
    stack_[adr++] = num >> 40 & 0xFF;
    stack_[adr++] = num >> 32 & 0xFF;
    FALLTHROUGH
  case Dword :
    stack_[adr++] = num >> 24 & 0xFF;
    stack_[adr++] = num >> 16 & 0xFF;
    FALLTHROUGH
  case Word :
    stack_[adr++] = num >> 8 & 0xFF;
    FALLTHROUGH
  case Byte :
    stack_[adr++] = num & 0xFF;
    break;
  ABORT_IF_DEFAULT
  }
}

__forceinline uint64_t Rvm::extend_byte_logical_(uint64_t x)
{
  uint64_t res = 0;
  res |= x;
  return res;
}

void Rvm::run_interrupt_(Interrupt interrupt)
{
  switch (interrupt) {
  case PutC :
    std::cout << static_cast<char>(registers_[Ir]);
    break;
  case PutS : {
    for (auto strAdr = registers_[Ir]; stack_[strAdr] && strAdr < stack_.size(); strAdr++) {
      std::cout << stack_[strAdr];
    }
    break;
  }
  case GetC :
    registers_[Ir] = getchar();
    break;
  case Halt :
    halted_ = true;
    break;
  ABORT_IF_DEFAULT
  }
}

#undef EXPECT_REG_EXISTS
#undef ABORT_IF_DEFAULT
#undef FALLTHROUGH
#undef NODISCARD

#pragma warning( pop )

#endif // RVM_HPP
