#ifndef RASM_LEXER_HPP
#define RASM_LEXER_HPP

#include <variant>
#include <unordered_map>
#include <fstream>
#include <optional>

#include "caseInsensitiveString.hpp"

class RasmLexer
{
public:

  enum class TokenType
  {
    Size,
    BinaryOperator,
    Mov,
    Push,
    Pop,
    Jump,
    Ret,
    Test,
    Call,
    Int,
    Integer,
    Label,
    Register,
    Comma,
    Plus,
    Minus,
    LeftPar,
    RightPar,
    Colon,
    Unknown,
    None,
    Eol,
    Eof
  };

  struct token_t
  {
    [[nodiscard]] uint8_t opcode() const;
    [[nodiscard]] std::pair<uint8_t, uint8_t> opcodeAndMode() const;
    [[nodiscard]] std::string lexeme() const;
    [[nodiscard]] uint8_t registerId() const;
    [[nodiscard]] uint64_t integer() const;
    [[nodiscard]] uint8_t size() const;

    TokenType type = TokenType::None;
    size_t row = 0;
    std::variant<uint8_t, std::pair<uint8_t, uint8_t>, std::string, uint64_t> data{};
  };

  friend std::istream& operator >>(std::istream&, token_t&);

  explicit RasmLexer(std::ifstream&);
  ~RasmLexer();
  token_t getNextToken();

private:

  size_t row_ = 1;
  std::ifstream& fin_;

  const static char comment_mark_ = ';';
  const static std::unordered_map<CaseInsensitiveString, uint8_t> binary_operators_;
  const static std::unordered_map<CaseInsensitiveString, uint8_t> registers_;
  const static std::unordered_map<CaseInsensitiveString, uint8_t> jumps_;
  const static std::unordered_map<CaseInsensitiveString, uint8_t> sizes_;
  const static std::unordered_map<CaseInsensitiveString, std::pair<TokenType, uint8_t>> others_;
};

#endif // RASM_LEXER_HPP
