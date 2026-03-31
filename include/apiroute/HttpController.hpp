#ifndef __HTTPCONTROLLER_HPP__
#define __HTTPCONTROLLER_HPP__

#include <functional>
#include <string>
#include <iostream>

#include "../http.hpp"

namespace apiroute {


#define METHOD_BEG              \
    static void init(Type *obj) \
    {

#define METHOD_END \
    }

#define METHOD_ADD(func, path, method) \
    std::apply([](auto &&...args) { app.addRoute(std::forward<decltype(args)>(args)...); }, create_handler<Type, decltype(&Type::func)>(obj, &Type::func, path, #method));

template <typename T, typename = void>
struct function_traits;

template <typename Ret, typename Req, typename Call, typename... Args>
struct function_traits<Ret(Req, Call, Args...)>
{
    using tuple = std::tuple<Args...>;
    static constexpr size_t size = sizeof...(Args);
};

template <typename Ret, typename... Args>
struct function_traits<std::function<Ret(Args...)>> : function_traits<Ret(Args...)>
{
};

template <typename Ret, typename... Args>
struct function_traits<Ret (*)(Args...)> : function_traits<Ret(Args...)>
{
};

template <typename Class, typename Ret, typename... Args>
struct function_traits<Ret (Class::*)(Args...)> : function_traits<Ret(Args...)>
{
};

template <typename Class, typename Ret, typename... Args>
struct function_traits<Ret (Class::*)(Args...) const> : function_traits<Ret(Args...)>
{
};

template <typename Lambda>
struct function_traits<Lambda, std::void_t<decltype(&Lambda::operator())>>
    : function_traits<decltype(&Lambda::operator())>
{
};

template <typename T>
T string_to_type(std::string &str)
{
    if constexpr (std::is_same_v<T, int>)
    {
        return std::stoi(str);
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        return str;
    }
    else
    {
        static_assert(sizeof(T) == 0, "Unsupported type");
    }
}

template <typename Func, typename Tuple, size_t... Idx>
void call_impi(Func &&f, request &req, std::function<void(const response &)> &&_callback, std::index_sequence<Idx...>)
{
    // 生成参数元组
    auto args_tuple = std::tuple_cat(
        std::forward_as_tuple(
            req, std::move(_callback)),
        std::make_tuple(
            (string_to_type<typename std::tuple_element<Idx, Tuple>::type>(req.args[Idx]))...));
    // 调用函数
    std::apply(std::forward<Func>(f), std::move(args_tuple));
}

template <typename Func>
void call_(Func &&f, request &req, std::function<void(const response &)> &&_callback)
{
    using Functor = std::remove_reference_t<Func>;
    constexpr size_t N = function_traits<Functor>::size;
    call_impi<Func, typename function_traits<Functor>::tuple>(
        std::forward<Func>(f), req, std::move(_callback), std::make_integer_sequence<size_t, N>{});
}

///////////
using RequestHandler = std::function<void(
    request &,
    std::function<void(const response &)> &&)>;

using RequestMsg = std::tuple<RequestHandler, std::string, std::string>;

template <typename Class, typename... BusinessArgs>
RequestHandler make_handler(Class *obj,
                            void (Class::*func)(request &, std::function<void(const response &)> &&, BusinessArgs...))
{
    return [obj, func = std::move(func)](request &req, std::function<void(const response &)> &&cb)
    {
        auto caller = [obj, func](request &r, std::function<void(const response &)> &&c, BusinessArgs... args)
        {
            (obj->*func)(r, std::move(c), std::forward<BusinessArgs>(args)...);
        };
        call_(std::move(caller), req, std::move(cb));
    };
}

template <typename Lambda>
RequestHandler make_handler(Lambda &&func)
{
    return [func = std::move(func)](request &req, std::function<void(const response &)> &&cb)
    { 
        call_(std::move(func), req, std::move(cb));
    };
}

template <typename Class, typename Func>
static RequestMsg create_handler(Class *obj, Func &&func, const std::string path_info, const std::string http_method)
{
    return {make_handler(obj, std::move(func)), path_info, http_method};
}

template <typename Lambda>
static RequestMsg create_handler(Lambda &&func, const std::string path_info, const std::string http_method)
{
    return {make_handler(std::move(func)), path_info, http_method};
}





template <typename Derived>
class HttpController
{
private:
    HttpController()
    {
        Derived::init(static_cast<Derived*>(this));
    }
    friend Derived;
    using Type = Derived;
};

};

#endif