#include "rasmTranslator.hpp"

#include <utility>
#include <iomanip>
#include <algorithm>

RasmTranslator::Status::Status(bool ok, std::vector<std::string> errors) :
  ok_(ok),
  errors_(std::move(errors))
{
}

RasmTranslator::Status::operator bool() const noexcept
{
  return ok_;
}

std::ostream& operator<<(std::ostream& stream, const RasmTranslator::Status& status)
{
  std::ostream::sentry sentry{ stream };
  if (!sentry) {
    return stream;
  }
  stream << "Errors: " << status.errors_.size() << "\n";
  for (size_t i = 0; i < status.errors_.size(); i++) {
    stream << "[" << std::setw(6) << std::right << i + 1 << "] " << status.errors_.at(i) << "\n";
  }
  return stream;
}

RasmTranslator::Status RasmTranslator::translate(std::ifstream& fin, std::ofstream& fout)
{
  if (!fin.is_open()) {
    return { false, { "input error occured." } };
  }
  if (!fout.is_open()) {
    return { false, { "output error occured."} };
  }
  lexer_.reset(new RasmLexer{ fin });
  if (!lexer_) {
    return { false, { "error occured while creating new lexer." } };
  }
  while (true) {
    std::deque<Token> line;
    Token current;
    while (current.type != TokenType::Eol && current.type != TokenType::Eof) {
      current = lexer_->getNextToken();
      if (current.type == TokenType::Unknown) {
        log_error_("at row " + std::to_string(current.row) + " unexpected token \'" + current.lexeme() + "\'");
        continue;
      }
      line.push_back(current);
    }
    handle_new_labels_(line);
    if (line.empty() || line.size() == 1 && line.front().type == TokenType::Eol) {
      continue;
    }
    switch (line.front().type) {
    case TokenType::BinaryOperator:
      handle_arithmetic_(line);
      break;
    case TokenType::Jump: [[falthrough]]
    case TokenType::Call:
      handle_jumps_(line);
      break;
    case TokenType::Mov:
      handle_mov_(line);
      break;
    default:
      handle_others_(line);
    }
    if (unresolved_labels_.empty()) {
      write_(fout);
    }
    if (line.back().type == TokenType::Eof) {
      break;
    }
  }
  return { !has_errors_, errors_ };
}

void RasmTranslator::try_resolve_label_(const std::string& label, uint64_t ip)
{
  if (unresolved_labels_.find(label) != unresolved_labels_.end()) {
    auto [l, h] = unresolved_labels_.equal_range(label);
    std::for_each(l, h, [&](const std::pair<std::string, uint64_t>& kv) {
      for (auto j = 0; j < 8; j++) {
        byte_code_buffer_[kv.second + j] = ip >> (64 - 8 * (j + 1)) & 0xFF;
      }
    });
    unresolved_labels_.erase(label);
  }
}

void RasmTranslator::recover_()
{
  auto t = lexer_->getNextToken().type;
  while (t != TokenType::Eol && t != TokenType::Eof) {
    t = lexer_->getNextToken().type;
  }
}

void RasmTranslator::log_error_(const std::string& message)
{
  errors_.push_back(message);
  has_errors_ = true;
  recover_();
}

bool RasmTranslator::check_head_type_(const std::deque<Token>& line, TokenType expected, const std::string& error)
{
  if (line.empty() || line.front().type != expected) {
    log_error_(error);
    return false;
  }
  return true;
}

bool RasmTranslator::check_end_of_line_(const std::deque<Token>& line, const std::string& error)
{
  if (!line.empty() && line.front().type != TokenType::Eol && line.front().type != TokenType::Eof) {
    log_error_(error);
    return false;
  }
  return true;
}

void RasmTranslator::write_(std::ofstream& fout)
{
  if (!has_errors_) {
    std::for_each(byte_code_buffer_.begin(), byte_code_buffer_.end(), [&](uint8_t b) {
      fout.write(reinterpret_cast<const char*>(&b), 1);
    });
  }
  byte_code_buffer_.clear();
}

void RasmTranslator::handle_new_labels_(std::deque<Token>& line)
{
  while (!line.empty() && line.front().type == TokenType::Label) {
    auto label = line.front().lexeme();
    auto row = line.front().row;
    line.pop_front();
    if (!check_head_type_(line, TokenType::Colon, ("at row " + std::to_string(row) + " unexpected token \'" + label + "\'"))) {
      return;
    }
    line.pop_front();
    if (labels_.find(label) != labels_.end()) {
      log_error_("at row " + std::to_string(row) + " label \'" + label + "\' was redefined");
    }
    labels_.insert({ label, curr_ip_ });
    try_resolve_label_(label, curr_ip_);
  }
}

void RasmTranslator::handle_arithmetic_(std::deque<Token>& line)
{
  auto fstByte = line.front().opcode();
  auto row = std::to_string(line.front().row);
  line.pop_front();
  if (!check_head_type_(line, TokenType::Register, "at row " + row + " expected register after binary operator")) {
    return;
  }
  uint8_t sndByte = line.front().registerId() << 4;
  line.pop_front();
  if (!check_head_type_(line, TokenType::Comma, "at row " + row + " expected comma between registers")) {
    return;
  }
  line.pop_front();
  if (!check_head_type_(line, TokenType::Register, "at row " + row + " expected two registers after binary operator")) {
    return;
  }
  sndByte |= line.front().registerId();
  line.pop_front();
  if (!check_end_of_line_(line, "at row " + row + " unexpected token after binary operation")) {
    return;
  }
  byte_code_buffer_.push_back(fstByte);
  byte_code_buffer_.push_back(sndByte);
  curr_ip_ += 2;
}

void RasmTranslator::handle_jumps_(std::deque<Token>& line)
{
  if (line.front().type == TokenType::Jump) {
    auto [fstByte, sndByte] = line.front().opcodeAndMode();
    sndByte <<= 5;
    byte_code_buffer_.push_back(fstByte);
    byte_code_buffer_.push_back(sndByte);
    curr_ip_ += 10;
  } else {
    auto opcode = line.front().opcode();
    byte_code_buffer_.push_back(opcode);
    curr_ip_ += 9;
  }
  auto row = std::to_string(line.front().row);
  line.pop_front();
  if (!check_head_type_(line, TokenType::Label, "at row " + row + " expected label after jump")) {
    return;
  }
  auto label = line.front().lexeme();
  line.pop_front();
  if (!check_end_of_line_(line, "at row " + row + " unexpected token after jump statement")) {
    return;
  }
  if (labels_.find(label) == labels_.end()) {
    unresolved_labels_.insert({ label, byte_code_buffer_.size() });
    for (auto i = 0; i < 8; i++) {
      byte_code_buffer_.push_back(0);
    }
  } else {
    auto dst = labels_.at(label);
    for (auto i = 1; i <= 8; i++) {
      byte_code_buffer_.push_back(dst >> (64 - 8 * i) & 0xFF);
    }
  }
}

void RasmTranslator::handle_mov_(std::deque<Token>& line)
{
  auto opcode = line.front().opcode();
  auto row = std::to_string(line.front().row);
  line.pop_front();
  if (!line.empty() && line.front().type == TokenType::Register) {
    auto dstReg = line.front().registerId();
    line.pop_front();
    if (!check_head_type_(line, TokenType::Comma, "at row " + row + " expected move source after comma")) {
      return;
    }
    line.pop_front();
    bool neg = false;
    if (!line.empty() && line.front().type == TokenType::Minus) {
      line.pop_front();
      neg = true;
      if (!check_head_type_(line, TokenType::Integer, "at row " + row + " expected integer to move")) {
        return;
      }
    }
    if (!line.empty() && line.front().type == TokenType::Integer) {
      auto num = line.front().integer();
      if (neg) {
        num |= uint64_t(1) << 63;
      }
      line.pop_front();
      if (!check_end_of_line_(line, "at row " + row + " unexpected token after move statement")) {
        return;
      }
      byte_code_buffer_.push_back(opcode);
      byte_code_buffer_.push_back(0b00000000 | dstReg);
      for (auto i = 1; i <= 8; i++) {
        byte_code_buffer_.push_back(num >> (64 - 8 * i) & 0xFF);
      }
      curr_ip_ += 10;
      return;
    }
    if (!line.empty() && line.front().type == TokenType::Register) {
      auto srcReg = line.front().registerId();
      line.pop_front();
      if (!check_end_of_line_(line, "at row " + row + " unexpected token after move statement")) {
        return;
      }
      byte_code_buffer_.push_back(opcode);
      byte_code_buffer_.push_back(0b01000000 | dstReg);
      byte_code_buffer_.push_back(srcReg << 4);
      curr_ip_ += 3;
      return;
    }
    if (!line.empty() && line.front().type == TokenType::Size) {
      auto size = line.front().size();
      line.pop_front();
      if (!check_head_type_(line, TokenType::LeftPar, "at row " + row + " expected move source address")) {
        return;
      }
      auto optSrcOff = get_reg_and_offset_(line);
      if (!optSrcOff) {
        return;
      }
      if (!check_end_of_line_(line, "at row " + row + " unexpected token after move statement")) {
        return;
      }
      auto [srcReg, offset] = optSrcOff.value();
      byte_code_buffer_.push_back(opcode);
      byte_code_buffer_.push_back(0b10000000 | dstReg | size << 4);
      byte_code_buffer_.push_back(srcReg << 4);
      for (auto i = 1; i <= 8; i++) {
        byte_code_buffer_.push_back(offset >> (64 - 8 * i) & 0xFF);
      }
      curr_ip_ += 11;
      return;
    }
    log_error_("at row " + row + " unexpected opcode and operands combination");
  } else if (!line.empty() && line.front().type == TokenType::Size) {
    auto size = line.front().size();
    line.pop_front();
    if (!check_head_type_(line, TokenType::LeftPar, "at row " + row + " expected destination move address")) {
      return;
    }
    auto optDstOff = get_reg_and_offset_(line);
    if (!optDstOff) {
      return;
    }
    auto [dstReg, offset] = optDstOff.value();
    if (!check_head_type_(line, TokenType::Comma, "at row" + row + " expected move source after comma")) {
      return;
    }
    line.pop_front();
    if (!check_head_type_(line, TokenType::Register, "at row " + row + " expected move source register")) {
      return;
    }
    auto srcReg = line.front().registerId();
    line.pop_front();
    if (!check_end_of_line_(line, "at row " + row + " unexpected token after move statement")) {
      return;
    }
    byte_code_buffer_.push_back(opcode);
    byte_code_buffer_.push_back(0b11000000 | dstReg | size << 4);
    byte_code_buffer_.push_back(srcReg << 4);
    for (auto i = 1; i <= 8; i++) {
      byte_code_buffer_.push_back(offset >> (64 - 8 * i) & 0xFF);
    }
    curr_ip_ += 11;
  } else {
    log_error_("at row " + row + " expected move destination");
  }
}

void RasmTranslator::handle_others_(std::deque<Token>& line)
{
  auto row = std::to_string(line.front().row);
  switch (line.front().type) {
  case TokenType::Ret: {
    byte_code_buffer_.push_back(line.front().opcode());
    line.pop_front();
    ++curr_ip_;
    break;
  }
  case TokenType::Int: {
    byte_code_buffer_.push_back(line.front().opcode());
    curr_ip_ += 2;
    line.pop_front();
    if (!check_head_type_(line, TokenType::Integer, "at row " + row + " expected interrupt id")) {
      return;
    }
    byte_code_buffer_.push_back(line.front().integer());
    line.pop_front();
    break;
  }
  case TokenType::Test: {
    byte_code_buffer_.push_back(line.front().opcode());
    curr_ip_ += 2;
    line.pop_front();
    if (!check_head_type_(line, TokenType::Register, "at row " + row + " expected register to test")) {
      return;
    }
    byte_code_buffer_.push_back(line.front().registerId() << 4);
    line.pop_front();
    break;
  }
  case TokenType::Push: [[falthrough]]
  case TokenType::Pop: {
    byte_code_buffer_.push_back(line.front().opcode());
    curr_ip_ += 2;
    line.pop_front();
    if (!check_head_type_(line, TokenType::Size, "at row " + row + " expected size after push/pop")) {
      return;
    }
    uint8_t sndByte = line.front().size() << 2;
    line.pop_front();
    if (!check_head_type_(line, TokenType::Register, "at row " + row + " expected register to push/pop")) {
      return;
    }
    sndByte |= line.front().registerId() << 4;
    line.pop_front();
    byte_code_buffer_.push_back(sndByte);
    break;
  }
  case TokenType::Eof: return;
  default: {
    log_error_("at row " + row + " unexpected token found");
    return;
  }
  }
  check_end_of_line_(line, "at row " + row + " unexpected token found");
}

std::optional<std::pair<uint8_t, int64_t>> RasmTranslator::get_reg_and_offset_(std::deque<Token>& line)
{
  auto row = std::to_string(line.front().row);
  line.pop_front();
  if (!check_head_type_(line, TokenType::Register, "at row " + row + " expected register as base for memory access")) {
    return {};
  }
  uint8_t reg = line.front().registerId();
  line.pop_front();
  if (!line.empty() && line.front().type == TokenType::RightPar) {
    line.pop_front();
    return { { reg, 0 } };
  }
  if (!line.empty() && (line.front().type == TokenType::Plus || line.front().type == TokenType::Minus)) {
    bool neg = line.front().type == TokenType::Minus;
    line.pop_front();
    if (!check_head_type_(line, TokenType::Integer, "at row " + row + " offset expected")) {
      return {};
    }
    uint64_t offset = neg ? line.front().integer() | uint64_t(1) << 63 : line.front().integer();
    line.pop_front();
    if (!check_head_type_(line, TokenType::RightPar, "at row " + row + " expected closing memory access bracket")) {
      return {};
    }
    line.pop_front();
    return { { reg, offset } };
  }
  log_error_("at row " + row + " unexpected memory access format");
  return {};
}
