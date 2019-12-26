#include "rasmLexer.hpp"

#include <functional>

namespace {
  std::string readWhile(std::istream& stream, std::function<bool(char)> pred)
  {
    std::istream::sentry sentry{ stream };
    if (!sentry) {
      return {};
    }
    std::string res;
    while (pred(stream.peek())) {
      char c;
      stream.get(c);
      res += c;
    }
    return res;
  }

  void dropWhile(std::istream& stream, std::function<bool(char)> pred)
  {
    std::istream::sentry sentry{ stream };
    if (!sentry) {
      return;
    }
    while (pred(stream.peek())) {
      stream.get();
    }
  }
}


const std::unordered_map<CaseInsensitiveString, uint8_t> RasmLexer::registers_ = {
  { "r0", 0  }, { "r1", 1 }, { "r2", 2  }, { "r3", 3  },
  { "r4", 4  }, { "r5", 5 }, { "r6", 6  }, { "r7", 7  },
  { "ir", 8  }, { "fg", 9 }, { "ip", 10 }, { "sp", 11 },
  { "bp", 12 }
};

const std::unordered_map<CaseInsensitiveString, uint8_t> RasmLexer::binary_operators_ = {
  { "add", 0  }, { "sub", 1  }, { "and", 2  },
  { "or" , 3  }, { "xor", 4  }, { "not", 5  },
  { "cmp", 13 }
};

const std::unordered_map<CaseInsensitiveString, uint8_t> RasmLexer::jumps_ = {
  { "jmp", 0b000 }, { "jz", 0b010 },
  { "jnz", 0b110 }, { "jp", 0b011 },
  { "jnp", 0b111 }, { "jn", 0b001 },
  { "jnn", 0b101 }, { "je", 0b010 },
  { "jne", 0b110 }, { "jg", 0b011 },
  { "jle", 0b111 }, { "jl", 0b001 },
  { "jge", 0b101 }
};

const std::unordered_map<CaseInsensitiveString, std::pair<RasmLexer::TokenType, uint8_t>> RasmLexer::others_ = {
  { "mov" , { TokenType::Mov, 6  }}, { "push", { TokenType::Push, 7  }},
  { "pop" , { TokenType::Pop, 8  }}, { "call", { TokenType::Call, 10 }},
  { "ret" , { TokenType::Ret, 11 }}, { "int",  { TokenType::Int , 12 }},
  { "test", {TokenType::Test, 14 }}
};

const std::unordered_map<CaseInsensitiveString, uint8_t> RasmLexer::sizes_ = {
  { "byte" , 0 }, { "word" , 1 },
  { "dword", 2 }, { "qword", 3 }
};

uint8_t RasmLexer::token_t::opcode() const
{
  return std::get<uint8_t>(data);
}

std::pair<uint8_t, uint8_t> RasmLexer::token_t::opcodeAndMode() const
{
  return std::get<std::pair<uint8_t, uint8_t>>(data);
}

std::string RasmLexer::token_t::lexeme() const
{
  return std::get<std::string>(data);
}

uint8_t RasmLexer::token_t::registerId() const
{
  return std::get<std::uint8_t>(data);
}

uint64_t RasmLexer::token_t::integer() const
{
  return std::get<uint64_t>(data);
}

uint8_t RasmLexer::token_t::size() const
{
  return std::get<uint8_t>(data);
}

RasmLexer::RasmLexer(std::ifstream& fin):
  fin_(fin)
{
  if (!fin_) {
    throw std::ios_base::failure{ "file input error." };
  }
  fin_ >> std::noskipws;
}

RasmLexer::~RasmLexer()
{
  fin_ >> std::skipws;
}

RasmLexer::token_t RasmLexer::getNextToken()
{
  dropWhile(fin_, [](char c) { return isblank(c); });
  if (fin_.peek() == comment_mark_) {
    dropWhile(fin_, [](char c) { return c != '\n'; });
  }
  token_t current;
  fin_ >> current;
  if (current.type == TokenType::Eol) {
    ++row_;
  }
  current.row = row_;
  return current;
}

std::istream& operator>>(std::istream& stream, RasmLexer::token_t& token)
{
  std::istream::sentry sentry{ stream };
  if (!sentry) {
    token.type = RasmLexer::TokenType::Eof;
    return stream;
  }
  if (isalpha(stream.peek())) {
    auto lex = readWhile(stream, [](char c) { return isalpha(c) || isdigit(c) || c == '_'; });
    if (RasmLexer::registers_.find(lex) != RasmLexer::registers_.end()) {
      token.type = RasmLexer::TokenType::Register;
      token.data = RasmLexer::registers_.at(lex);
    } else if (RasmLexer::binary_operators_.find(lex) != RasmLexer::binary_operators_.end()) {
      token.type = RasmLexer::TokenType::BinaryOperator;
      token.data = RasmLexer::binary_operators_.at(lex);
    } else if (RasmLexer::jumps_.find(lex) != RasmLexer::jumps_.end()) {
      token.type = RasmLexer::TokenType::Jump;
      token.data = std::pair{ 9, RasmLexer::jumps_.at(lex) };
    } else if (RasmLexer::others_.find(lex) != RasmLexer::others_.end()) {
      auto [t, c] = RasmLexer::others_.at(lex);
      token.type = t;
      token.data = c;
    } else if (RasmLexer::sizes_.find(lex) != RasmLexer::sizes_.end()) {
      token.type = RasmLexer::TokenType::Size;
      token.data = RasmLexer::sizes_.at(lex);
    } else {
      token.type = RasmLexer::TokenType::Label;
      token.data = lex;
    }
  } else if (isdigit(stream.peek())) {
    auto num = readWhile(stream, [](char c) { return isdigit(c); });
    token.type = RasmLexer::TokenType::Integer;
    token.data = std::stoull(num);
  } else if (stream.peek() == std::char_traits<char>::eof()) {
    token.type = RasmLexer::TokenType::Eof;
  } else {
    switch (stream.peek()) {
    case '-':
      token.type = RasmLexer::TokenType::Minus;
      stream.get();
      break;
    case '+':
      token.type = RasmLexer::TokenType::Plus;
      stream.get();
      break;
    case ':':
      token.type = RasmLexer::TokenType::Colon;
      stream.get();
      break;
    case '[':
      token.type = RasmLexer::TokenType::LeftPar;
      stream.get();
      break;
    case ']':
      token.type = RasmLexer::TokenType::RightPar;
      stream.get();
      break;
    case '\n':
      token.type = RasmLexer::TokenType::Eol;
      stream.get();
      break;
    case ',':
      token.type = RasmLexer::TokenType::Comma;
      stream.get();
      break;
    default:
      token.type = RasmLexer::TokenType::Unknown;
      token.data = readWhile(stream, [](char c) { return !isspace(c); });
    }
  }
  return stream;
}
