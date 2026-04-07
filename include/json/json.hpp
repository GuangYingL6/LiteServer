#ifndef __JSON_HPP__
#define __JSON_HPP__

#include <string>
#include <utility>
#include <tuple>
#include <sstream>
#include <array>
#include <vector>
#include <deque>
#include <map>
#include <iostream>

namespace json {

    template <typename Class, typename Type, const char* Str, Type Class::*Ptr>
    struct JsonType
    {
        using type = Type;
        static constexpr std::string_view name{Str};
        static constexpr Type Class::*ptr{Ptr};
    };

#define JSONTYPE(type, name) \
    json::JsonType<DerType, type, STR(name), &DerType::name>

#define GETSTRNAME(name) __struct_string_##name

#define DEF_STRING(name)                      \
    struct __struct_string_##name             \
    {                                         \
        static constexpr char _str[] = #name;  \
    };

#define STR(str) GETSTRNAME(str)::_str

    template <typename T>
    struct member_type;

    template <typename ClassType, typename MemberType>
    struct member_type<MemberType ClassType::*>
    {
        using type = MemberType;
    };

    template <typename T>
    using member_type_t = typename member_type<T>::type;

#define MAKE_JSONTYPE(mem) \
    JSONTYPE(json::member_type_t<decltype(&DerType::mem)>, mem)

#define DEF_JSONLIST(...) \
    using json_member_list = std::tuple<__VA_ARGS__>

    template <typename T>
    std::string to_str(T &&obj) {
        return obj.list_tojson();
    }

    template <>
    std::string to_str(std::string &obj) {
        return R"(")" + obj + R"(")";
    }
    template <typename T, size_t N>
    std::string to_str(std::array<T, N> &obj) {
        if constexpr (N == 0)
            return R"([])";
        std::string strarr[N];
        size_t total_size = 1;
        for (size_t i{0}; i < N; ++ i) {
            strarr[i] = std::move(to_str(obj[i]));
            total_size += strarr[i].size() + 1;//逗号
        }
        std::string result;
        result.reserve(total_size);
        result += '[';
        result.append(strarr[0]);
        for (size_t i{1}; i < N; ++ i) {
            result += ',';
            result.append(std::move(strarr[i]));
        }
        result += ']';
        return result;
    }
    template <typename T>
    std::string to_str(std::vector<T> &obj){
        if (obj.size() == 0)
            return R"([])";
        std::string strarr[obj.size()];
        size_t total_size = 1;
        for (size_t i{0}; i < obj.size(); ++i) {
            strarr[i] = std::move(to_str(obj[i]));
            total_size += strarr[i].size() + 1; // 逗号
        }
        std::string result;
        result.reserve(total_size);
        result += '[';
        result.append(strarr[0]);
        for (size_t i{1}; i < obj.size(); ++i)
        {
            result += ',';
            result.append(std::move(strarr[i]));
        }
        result += ']';
        return result;
    }

    template <>
    std::string to_str(int &obj) { return std::to_string(obj); }
    template <>
    std::string to_str(long &obj) { return std::to_string(obj); }
    template <>
    std::string to_str(double &obj) { return std::to_string(obj); }
    template <>
    std::string to_str(bool &obj) { return std::to_string(obj); }

    template <typename JsonType,typename Class, typename Type>
    std::string jsontype_to_json(Class* obj, Type Class::*mem) {
        return R"(")" + std::string(JsonType::name) + R"(":)" + to_str(obj->*mem);
    }

    template <typename Derived>
    class JsonBase
    {
    public:
        using DerType = Derived;
        template <typename Tuple, size_t... Is>
        std::string _list_tojson(std::index_sequence<Is...>) {
            std::string parts[] = {
                jsontype_to_json<std::tuple_element_t<Is, Tuple>>(static_cast<Derived *>(this), std::tuple_element_t<Is, typename Derived::json_member_list>::ptr)...};
            std::ostringstream oss;
            oss << "{";
            for (size_t i = 0; i < sizeof...(Is); ++i) {
                if (i != 0)
                    oss << ",";
                oss << parts[i];
            }
            oss << "}";
            return oss.str();
        }

        std::string list_tojson() {
            using Tuple = typename Derived::json_member_list;
            constexpr size_t N = std::tuple_size<Tuple>::value;
            return _list_tojson<Tuple>(std::make_index_sequence<N>{});
        }

        std::string serialization() {
            return list_tojson();
        }
    };

    auto getdom(const std::string_view& json) -> decltype(auto) {
        std::map<std::string_view, std::string_view> dom;
        int cont = 0;
        bool is_key = true;
        int idx = -1, cnt = 0;
        std::string_view sk, sv;
        for (size_t pos{0}; pos < json.size(); ++pos)
        {
            const char &c = json[pos];
            if (c == '{' || c == '[')
                ++cont;
            else if (c == '}' || c == ']')
                --cont;
            if (cont == 1) {
                if (is_key) {
                    if (c == '"') {
                        if (idx == -1) {
                            idx = pos + 1;
                        } else {
                            sk = std::string_view(json.data() + idx, cnt - 1);
                            idx = -1; cnt = 0;
                            is_key = false;
                        }
                    }
                } else {
                    if (c == ':') {
                        idx = pos + 1;
                    } else if (idx != -1 && c == ',') {
                        sv = std::string_view(json.data() + idx, cnt - 1);
                        idx = -1; cnt = 0;
                        is_key = true;
                        dom.insert({sk, sv});
                        sk = {}; sv = {};
                    }
                }
            } else if (cont == 0) {
                if (idx != -1) {
                    sv = std::string_view(json.data() + idx, cnt - 1);
                    idx = -1; cnt = 0;
                    is_key = true;
                    dom.insert({sk, sv});
                    sk = {}; sv = {};
                }
            }
            if (idx != -1)
                ++cnt;
        }
        for (auto& [k, v] : dom) {
            std::cout << k << ":" << v << std::endl;
        }
        return dom;
    }

    template <typename T>
    auto fromjson(const std::string_view &json) -> T;

    template <typename T>
    struct TypeTag {
        using type = T;
    };

    template <template <typename, size_t> typename T, typename Type, size_t N>
    struct TypeTag<T<Type, N>> {
        using type = T<Type, N>;
    };

    template <template <typename> typename T, typename Type>
    struct TypeTag<T<Type>> {
        using type = T<Type>;
    };

    template <typename T>
    auto from_str(const std::string_view &str, TypeTag<T>) -> T {
        return fromjson<T>(str);
    }

    template <typename>
    auto from_str(const std::string_view &str, TypeTag<std::string>) -> std::string {
        return std::string(str.data() + 1, str.size() - 2);
    }

    template <typename>
    auto from_str(const std::string_view &str, TypeTag<int>) -> int {
        return std::stoi(static_cast<const std::string>(str));
    }

    template <typename>
    auto from_str(const std::string_view &str, TypeTag<long>) -> long {
        return std::stol(static_cast<const std::string>(str));
    }

    template <typename>
    auto from_str(const std::string_view &str, TypeTag<double>) -> double {
        return std::stod(static_cast<const std::string>(str));
    }

    template <typename>
    auto from_str(const std::string_view &str, TypeTag<bool>) -> bool {
        if (str == "true")
            return true;
        else
            return false;
    }

    template <typename, typename Type, size_t N>
    auto from_str(const std::string_view &str, TypeTag<std::array<Type, N>>) -> std::array<Type, N> {
        std::array<Type, N> arr;
        int idx = -1, cnt = 0;
        int count = 0;
        for (size_t i{0}, n{0}; i < str.size(); ++i) {
            const char &c = str[i];
            if (c == '[' || c == '{')
                count++;
            else if (c == ']' || c == '}')
                count--;
            if (count == 1) {
                if (idx == -1) {
                    idx = i + 1;
                } else if (c == ',') {
                    std::string_view s(str.data() + idx, cnt - 1);
                    arr[n++] = std::move(from_str<Type>(s, TypeTag<Type>{}));
                    idx = i + 1;
                    cnt = 0;
                }
            } else if (count == 0) {
                if (idx != -1) {
                    std::string_view s(str.data() + idx, cnt - 1);
                    arr[n++] = std::move(from_str<Type>(s, TypeTag<Type>{}));
                    idx = i + 1;
                    cnt = 0;
                }
            }
            if (idx != -1) {
                cnt++;
            }
        }
        return arr;
    }

    template <typename, typename Type>
    auto from_str(const std::string_view &str, TypeTag<std::vector<Type>>) -> std::vector<Type>
    {
        std::vector<Type> vec;
        int idx = -1, cnt = 0;
        int count = 0;
        for (size_t i{0}; i < str.size(); ++i) {
            const char &c = str[i];
            if (c == '[' || c == '{')
                count++;
            else if (c == ']' || c == '}')
                count--;
            if (count == 1) {
                if (idx == -1) {
                    idx = i + 1;
                } else if (c == ',') {
                    std::string_view s(str.data() + idx, cnt - 1);
                    vec.push_back(std::move(from_str<Type>(s, TypeTag<Type>{})));
                    idx = i + 1;
                    cnt = 0;
                }
            } else if (count == 0) {
                if (idx != -1) {
                    std::string_view s(str.data() + idx, cnt - 1);
                    vec.push_back(std::move(from_str<Type>(s, TypeTag<Type>{})));
                    idx = i + 1;
                    cnt = 0;
                }
            }
            if (idx != -1) {
                cnt++;
            }
        }
        return vec;
    }

    template <typename Class, size_t I>
    auto for_each_tuple(Class *obj, const std::string_view& name, const std::string_view& value) -> void
    {
        using Tuple = typename Class::json_member_list;
        using JsonType = std::tuple_element_t<I, Tuple>;
        using Type = typename JsonType::type;
        if (JsonType::name == name) {
            obj->*JsonType::ptr = from_str<Type>(value, TypeTag<Type>{});
        }
        if constexpr (I >= 1)
            for_each_tuple<Class, I - 1>(obj, name, value);
    }

    template <typename T>
    auto fromjson(const std::string_view &json) -> T {
        auto &&dom = getdom(json);
        T obj;
        constexpr size_t N = std::tuple_size_v<typename T::json_member_list>;
        for (auto &[name, value] : dom) {
            if constexpr (N >= 1)
                for_each_tuple<T, N - 1>(&obj, name, value);
        }
        return obj;
    }

    template <typename T, typename Str>
    inline auto deserialization(Str &&str) -> T {
        const std::string_view sv(str);
        return from_str<T>(sv, TypeTag<T>{});
    }

    template <typename T>
    inline std::string serialization(T &&obj) {
        return to_str(std::forward<T>(obj));
    }

    /*
    class User : public JsonBase<User>
    {
    public:
        std::string name;
        int year;
        std::array<std::string, 3> vecname;

        DEF_STRING(name);
        DEF_STRING(year);
        DEF_STRING(vecname);
        DEF_JSONLIST(MAKE_JSONTYPE(name), MAKE_JSONTYPE(year), MAKE_JSONTYPE(vecname));
    };
    */
};

/*
int main() {
    using namespace json;
    User u;
    u.name = "n1";
    u.year = 10;
    u.vecname = {"abc", "123", "嗨嗨嗨"};
    std::string s;
    std::cout << (s = serialization(u)) << std::endl;
    auto u1 = deserialization<User>(s);
    std::cout << (u1.serialization()) << std::endl;
    return 0;
}
*/

#endif