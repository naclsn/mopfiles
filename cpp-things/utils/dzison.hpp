#ifndef _DZISON_
#define _DZISON_
#include <type_traits>
#include <ios>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

/// Defines `to_json`/`from_json` templates for:
///   - arithmetic types
///   - char const*
///   - string
///   - vector<..>
///   - map<string, ..>
///   - varargs
///   - initializer_list of keys and matching varargs
///   - any type with a `cvt_to_from_json` member type
///   - type with matching `to_` (resp. `from_`)
///
/// These `to_json`/`from_json` functions take an out/in
/// stream and a reference to the object as argument.
///
/// There is also a generic `any_json` type.
///
/// Example:
/// ```cpp
/// #include "dzison.hpp"
/// using namespace dzison; // (see also: ADL)
///
/// to_json(cout, some_int_vector);
/// from_json(cin, some_int_vector);
///
/// struct bidoof {
///     string name;
///     unsigned age;
///
///     struct cvt_to_from_json {
///         typedef bidoof type;
///         static constexpr tuple pairs{
///             pair{"name", &type::name},
///             pair{"age", &type::age}};
///     };
/// };
///
/// to_json(cout, some_bidoof);
/// from_json(cin, some_bidoof);
/// ```

namespace dzison {

    using namespace std;

    struct any_json;

    template <typename T> enable_if_t<is_arithmetic_v<T>, ostream&> to_json(ostream& res, T const& val) { return res << val; }
    template <typename T> enable_if_t<is_arithmetic_v<T>, istream&> from_json(istream& ser, T& val) { return ser >> val; }

    ostream& to_json(ostream& res, bool const& val) { return res << boolalpha << val; }
    istream& from_json(istream& ser, bool& val) { return ser >> boolalpha >> val; }

    template <typename T>
    ostream& to_json(ostream& res, optional<T> const& val) { return val ? to_json(res, *val) : res << "null"; }
    template <typename T>
    istream& from_json(istream& ser, optional<T>& val) {
        if ('n' == (ser >> ws).peek()) {
            char null[4];
            if (ser.read(null, 4) && 'u' == null[1] && 'l' == null[2] && 'l' == null[3]) val.reset();
            else ser.setstate(ios::failbit);
        } else from_json(ser, val.emplace());
        return ser;
    }

    ostream& to_json(ostream& res, char const* val) {
        res << '"';
        char it;
        while ((it = *val++)) {
            switch (it) {
                case '\b': res << "\\b"; continue;
                case '\t': res << "\\t"; continue;
                case '\n': res << "\\n"; continue;
                case '\f': res << "\\f"; continue;
                case '\r': res << "\\r"; continue;
                case '"': res << "\\\""; continue;
                case '\\': res << "\\\\"; continue;
            }
            if (it < 0x20) res << (it < 0x10 ? "\\u000" : "\\u00") << hex << (int)it;
            else res << it;
        }
        return res << '"';
    }
    ostream& to_json(ostream& res, string const& val) { return to_json(res, val.c_str()); }
    istream& from_json(istream& ser, string& val) {
        char c = (ser >> ws).get();
        if ('"' == c) {
            val.clear();
            while (ser && '"' != (c = ser.get())) {
                if ('\\' == c) switch ((c = ser.get())) {
                    case 'b': val.push_back('\b'); continue;
                    case 't': val.push_back('\t'); continue;
                    case 'n': val.push_back('\n'); continue;
                    case 'f': val.push_back('\f'); continue;
                    case 'r': val.push_back('\r'); continue;
                    case 'u':
                        // XXX: doesn't handle above 0xff
                        for (int k = c = 0, o; k < 4; k++) {
                            o = ser.get() & 0b1011111;
                            if (!ser && !('0' <= o && o <= '9' || 'A' <= o && o <= 'F'))
                                return ser.setstate(ios::failbit), ser;
                            c = c << 8 | ((o&0b1111) + ('9'<o)*9);
                        }
                }
                val.push_back(c);
            }
        }
        if ('"' != c) ser.setstate(ios::failbit);
        return ser;
    }
    // XXX: no wstring, although it would just be c/p of above and better handle 16 bits

    template <typename T>
    ostream& to_json(ostream& res, vector<T> const& val) {
        res << '[';
        bool sep = false;
        for (auto const& it : val) to_json(sep ? res << ',' : (sep = true, res), it);
        return res << ']';
    }
    template <typename T>
    istream& from_json(istream& ser, vector<T>& val) {
        char c = (ser >> ws).get();
        if ('[' != c) return ser.setstate(ios::failbit), ser;
        while (ser) if (',' != (c = (from_json(ser, val.emplace_back()) >> ws).get())) break;
        if (']' != c) return ser.setstate(ios::failbit), ser;
        return ser;
    }
    // XXX: no list/deque.., although it would just be c/p of above -> template

    template <typename T, typename... TT>
    ostream& to_json(ostream& res, T const& val, TT const&... vval) {
        res << '[';
        (to_json(res, val) &&...&& to_json(res << ',', vval));
        return res << ']';
    }
    template <typename T, typename... TT>
    istream& from_json(istream& ser, T& val, TT&... vval) {
        char c = (ser >> ws).get();
        if ('[' != c) return ser.setstate(ios::failbit), ser;
        bool ok = ((',' == (c = (from_json(ser, val) >> ws).get())) &&...&& (ser && ',' == (c = (from_json(ser, vval) >> ws).get())));
        if (!ok || ']' != c) return ser.setstate(ios::failbit), ser;
        return ser;
    }

    template <typename T>
    ostream& to_json(ostream& res, map<string, T> const& val) {
        res << '{';
        bool sep = false;
        for (auto const& it : val) to_json(to_json(sep ? res << ',' : (sep = true, res), it.first) << ':', it.second);
        return res << '}';
    }
    template <typename T>
    istream& from_json(istream& ser, map<string, T>& val) {
        char c = (ser >> ws).get();
        if ('{' != c) return ser.setstate(ios::failbit), ser;
        string key;
        while (ser) {
            if (':' != (c = (from_json(ser, key) >> ws).get()) || !ser) return ser.setstate(ios::failbit), ser;
            if (',' != (c = (from_json(ser, val.emplace(move(key), T()).first->second) >> ws).get())) break;
        }
        if ('}' != c) return ser.setstate(ios::failbit), ser;
        return ser;
    }
    // XXX: no unsorted_map, although it would just be c/p of above -> template

    template <typename T, typename... TT>
    ostream& to_json(ostream& res, initializer_list<char const*> const keys, T const& val, TT const&... vval) {
        res << '{';
        auto cur = keys.begin();
        (to_json(to_json(res, *cur) << ':', val), ..., to_json(to_json(res << ',', *++cur) << ':', vval));
        return res << '}';
    }
    template <typename T, typename... TT>
    istream& from_json(istream& ser, initializer_list<char const*> const keys, T& val, TT&... vval) {
        char c = (ser >> ws).get();
        if ('{' != c) return ser.setstate(ios::failbit), ser;
        string key;
        while (ser) {
            if (':' != (c = (from_json(ser, key) >> ws).get()) || !ser) return ser.setstate(ios::failbit), ser;
            auto cur = keys.begin();
            if (*cur == key) from_json(ser, val);
            else if (([&] { return *++cur == key ? from_json(ser, vval), false : true; }() &&...)) {
                any_json dum;
                from_json(ser, dum);
            }
            if (!ser || ',' != (c = (ser >> ws).get())) break;
        }
        if ('}' != c) return ser.setstate(ios::failbit), ser;
        return ser;
    }

    template <typename T>
    bool from_json(char const* c, T& val) {
        istringstream s(c);
        return !!from_json(s, val);
    }

    template <typename T, typename desc = typename T::cvt_to_from_json>
    ostream& to_json(ostream& res, T const& val) {
        res << '{';
        apply([&](auto const&... it) {
            bool sep = false;
            (to_json(to_json(sep ? res << ',' : (sep = true, res), it.first) << ':', val.*(it.second)), ...);
        }, desc::pairs);
        return res << '}';
    }
    template <typename T, typename desc = typename T::cvt_to_from_json>
    istream& from_json(istream& ser, T& val) {
        char c = (ser >> ws).get();
        if ('{' != c) return ser.setstate(ios::failbit), ser;
        string key;
        while (ser) {
            if (':' != (c = (from_json(ser, key) >> ws).get()) || !ser) return ser.setstate(ios::failbit), ser;
            apply([&](auto const&... it) {
                if (([&] { return it.first == key ? from_json(ser, val.*(it.second)), false : true; }() &&...)) {
                    any_json dum;
                    from_json(ser, dum);
                }
            }, desc::pairs);
            if (!ser || ',' != (c = (ser >> ws).get())) break;
        }
        if ('}' != c) return ser.setstate(ios::failbit), ser;
        return ser;
    }

    struct any_json : optional<variant<bool, double, string, vector<any_json>, map<string, any_json>>> {
        using optional::optional;
        friend ostream& to_json(ostream& res, any_json const& val) {
            if (!val) return res << "null";
            std::visit([&](auto const& it) { to_json(res, it); }, *val);
            return res;
        }
        friend istream& from_json(istream& ser, any_json& val) {
            char c = (ser >> ws).peek();
            if ('n' == c) {
                char null[4];
                if (ser.read(null, 4) && 'u' == null[1] && 'l' == null[2] && 'l' == null[3]) val.reset();
                else ser.setstate(ios::failbit);
                return ser;
            }
            val.emplace();
            switch (c) {
                case 't':
                case 'f': return from_json(ser, val->emplace<bool>());
                case '"': return from_json(ser, val->emplace<string>());
                case '[': return from_json(ser, val->emplace<vector<any_json>>());
                case '{': return from_json(ser, val->emplace<map<string, any_json>>());
            }
            if ('0' <= c && c <= '9') return from_json(ser, val->emplace<double>());
            return ser.setstate(ios::failbit), ser;
        }
    };

} // namespace dzison
#endif // _DZISON_
