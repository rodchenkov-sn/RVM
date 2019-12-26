#ifndef CASE_INSENSITIVE_STRING_HPP
#define CASE_INSENSITIVE_STRING_HPP

#include <string>

class CaseInsensitiveString
{
public:
  /* implicit */ CaseInsensitiveString(std::string);
  /* implicit */ CaseInsensitiveString(const char*);
  bool operator == (const CaseInsensitiveString&) const;
  bool operator != (const CaseInsensitiveString&) const;
  friend struct std::hash<CaseInsensitiveString>;
  std::string& data();
  std::string lowerCase() const;
private:
  std::string data_;
};

namespace std {
  template<>
  struct hash<CaseInsensitiveString>
  {
    size_t operator() (const CaseInsensitiveString& cis) const noexcept
    {
      return std::hash<std::string>{}(cis.lowerCase());
    }
  };
}

#endif // CASE_INSENSITIVE_STRING_HPP
