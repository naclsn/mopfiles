#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace yson {

  struct value;

  using boolean = bool;
  using number = double;
  using string = std::string;
  using list = std::vector<value>;
  using object = std::unordered_map<string, value>;

  using base = std::variant<boolean, number, string, list, object>;
  struct value : base {
    using base::variant;

    template <typename T>
    inline std::optional<std::reference_wrapper<T>> as() {
      auto* r = std::get_if<T>(this);
      return r ? *r : std::nullopt;
    }

    template <typename T>
    inline T& as_unchecked() {
      return std::get<T>(*this);
    }
  };

  std::istream& operator>>(std::istream&, value&);
  std::ostream& operator<<(std::ostream&, value const&);

#ifdef YSON_IMPLEMENTATION
# ifdef YSON_NOTHROW
#  define YSON_EXTRACT_NOTHROW
#  define YSON_INSERT_NOTHROW
# endif

  inline void _expect(std::istream& in, char c) {
    char a = (in >> std::ws).get();
    if (c != a) {
      in.setstate(std::ios::failbit);
# ifndef YSON_EXTRACT_NOTHROW
      throw std::runtime_error(string("expected '") + c + "' but got '" + a + '\'');
# endif
    }
  }

  std::istream& operator>>(std::istream& in, string& st) {
    _expect(in, '"');
    while (true) switch (in.peek()) {
      case std::char_traits<char>::eof():
        _expect(in, '"');
        return in;
      case '"':
        in.ignore();
        return in;
      case '\\':
        st.push_back(in.get());
        [[fallthrough]];
      default:
        st.push_back(in.get());
    }
  }

  std::istream& operator>>(std::istream& in, list& ls) {
    _expect(in, '[');
    while (']' != (in >> std::ws).peek() && in >> ls.emplace_back() && ',' == (in >> std::ws).peek())
      in.ignore();
    _expect(in, ']');
    return in;
  }

  std::istream& operator>>(std::istream& in, object& ob) {
    _expect(in, '{');
    string k;
    while ('}' != (in >> std::ws).peek() && in >> k && (_expect(in, ':'), in >> std::get<1>(*std::get<0>(ob.emplace(move(k), false)))) && ',' == (in >> std::ws).peek())
      in.ignore();
    _expect(in, '}');
    return in;
  }

  std::istream& operator>>(std::istream& in, value& val) {
    switch ((in >> std::ws).peek()) {
      case 't':
      case 'f': return in >> std::boolalpha >> val.emplace<boolean>();
      case '"': return in >> val.emplace<string>();
      case '[': return in >> val.emplace<list>();
      case '{': return in >> val.emplace<object>();
    }
    if ('0' <= in.peek() && in.peek() <= '9') return in >> val.emplace<number>();

    in.setstate(std::ios::failbit);
# ifdef YSON_EXTRACT_NOTHROW
    return in;
# else
    throw std::runtime_error("invalid input");
# endif
  }

  std::ostream& operator<<(std::ostream& out, string const& st) {
    return std::operator<<(out << '"', st) << '"';
  }

  std::ostream& operator<<(std::ostream& out, list const& ls) {
    bool first = true;
    out << '[';
    for (auto const& e : ls) {
      if (first) first = false; else out << ',';
      out << e;
    }
    return out << ']';
  }

  std::ostream& operator<<(std::ostream& out, object const& ob) {
    bool first = true;
    out << '{';
    for (auto const& pair : ob) {
      if (first) first = false; else out << ',';
      out << pair.first << ':' << pair.second;
    }
    return out << '}';
  }

  std::ostream& operator<<(std::ostream& out, value const& val) {
    return std::visit([&out](auto&& it) -> std::ostream& {
      using ti = std::decay_t<decltype(it)>;

      if constexpr (std::is_same_v<boolean, ti>) return out << std::boolalpha << it;
      if constexpr (std::is_same_v<number, ti>) return out << it;
      if constexpr (std::is_same_v<string, ti>) return out << it;
      if constexpr (std::is_same_v<list, ti>) return out << it;
      if constexpr (std::is_same_v<object, ti>) return out << it;

# ifdef YSON_INSERT_NOTHROW
      return out;
# else
      throw std::runtime_error("unrechable: corrupted value");
# endif
    }, val);
  }

#endif // YSON_IMPLEMENTATION

} // namespace yson
