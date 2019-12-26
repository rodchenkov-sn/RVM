#include "caseInsensitiveString.hpp"

#include <utility>
#include <algorithm>

CaseInsensitiveString::CaseInsensitiveString(std::string data):
  data_(std::move(data))
{
}

CaseInsensitiveString::CaseInsensitiveString(const char* data):
  data_(data)
{ 
}

bool CaseInsensitiveString::operator==(const CaseInsensitiveString& other) const
{
  if (other.data_.size() != data_.size()) {
    return false;
  }
  return std::equal(
    data_.begin(), data_.end(),
    other.data_.begin(), other.data_.end(),
    [](char l, char r) { return tolower(l) == tolower(r); }
  );
}

bool CaseInsensitiveString::operator!=(const CaseInsensitiveString& other) const
{
  return !(*this == other);
}

std::string& CaseInsensitiveString::data()
{
  return data_;
}

std::string CaseInsensitiveString::lowerCase() const
{
  std::string tmp;
  tmp.resize(data_.size());
  std::transform(data_.begin(), data_.end(), tmp.begin(), tolower);
  return tmp;
}

