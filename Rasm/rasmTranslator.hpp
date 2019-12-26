#ifndef RASM_TRANSLATOR_HPP
#define RASM_TRANSLATOR_HPP

#include <deque>
#include <vector>
#include <unordered_map>
#include <functional>

#include "rasmLexer.hpp"

class RasmTranslator
{
public:

  using Token = RasmLexer::token_t;
  using TokenType = RasmLexer::TokenType;

  class Status
  {
  public:
    Status() = delete;
    explicit operator bool() const noexcept;
    friend std::ostream& operator << (std::ostream&, const Status&);
  private:
    friend class RasmTranslator;

    Status(bool, std::vector<std::string>);

    bool ok_ = true;
    std::vector<std::string> errors_;
  };

  Status translate(std::ifstream&, std::ofstream&);

private:

  void recover_();
  void log_error_(const std::string&);
  bool check_head_type_(const std::deque<Token>&, TokenType, const std::string&);
  bool check_end_of_line_(const std::deque<Token>&, const std::string&);

  void write_(std::ofstream&);
  void handle_new_labels_(std::deque<Token>&);
  void try_resolve_label_(const std::string&, uint64_t);

  void handle_arithmetic_(std::deque<Token>&);
  void handle_jumps_(std::deque<Token>&);
  void handle_mov_(std::deque<Token>&);
  void handle_others_(std::deque<Token>&);

  std::optional<std::pair<uint8_t, int64_t>> get_reg_and_offset_(std::deque<Token>&);

  std::deque<uint8_t> byte_code_buffer_;
  std::unordered_map<std::string, uint64_t> labels_;
  std::unordered_multimap<std::string, uint64_t> unresolved_labels_;

  std::unique_ptr<RasmLexer> lexer_;

  std::vector<std::string> errors_;
  bool has_errors_ = false;
  uint64_t curr_ip_    = 0;
};

#endif // RASM_TRANSLATOR_HPP
