#include <vector>
#include <string>
#include <map>
#include <functional>

#include "../../include/APImap/HttpController.hpp"




std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = s.find(delim);
    while (end != std::string::npos)
    {
        tokens.push_back(s.substr(start, end - start));
        start = end + 1;
        end = s.find(delim, start);
    }
    tokens.push_back(s.substr(start)); // 最后一段
    return tokens;
}

void callback(const response &res)
{
    std::cout << "callback" << std::endl;
}




struct Route
{
    std::string method;
    std::string pattern;                   // 原始模式，如 "/user/:id"
    std::vector<std::string> pathSegments; // 按 '/' 分割后的片段
    RequestHandler handler;
    std::vector<std::string> paramNames; // 提取出的参数名（如 ["id"]）
};


class Router
{
    std::vector<Route> routes_;

public:
    void addRoute(const RequestHandler &&handler, const std::string &&pattern, const std::string &&method)
    {
        Route r;
        r.method = method;
        r.pattern = pattern;
        r.handler = handler;
        // 分割路径，提取参数名
        r.pathSegments = split(pattern, '/');
        for (const auto &seg : r.pathSegments)
        {
            if (!seg.empty() && seg[0] == ':')
            {
                r.paramNames.push_back(seg.substr(1));
            }
        }
        routes_.push_back(r);
        std::cout << pattern << std::endl;
    }

    bool route(request &req, response &res)
    {
        std::vector<std::string> reqSegments = split(req.url, '/');
        for (auto &route : routes_)
        {
            std::cout << "route.method: " << route.method << " req.method: " << req.method << std::endl;
            if (route.method != req.method)
                continue;
            //if (route.pathSegments.size() != reqSegments.size())
            //    continue;

            std::vector<std::string> params;
            bool match = true;
            for (size_t i = 0; i < route.pathSegments.size(); ++i)
            {
                const auto &seg = route.pathSegments[i];
                const auto &reqSeg = reqSegments[i];
                if (!seg.empty() && seg[0] == ':')
                {
                    // 参数匹配，存储值
                    params.push_back(reqSeg);
                }
                else if (seg != reqSeg)
                {
                    match = false;
                    break;
                }
            }
            std::cout << "match: " << match << std::endl;
            if (match)
            {
                req.args = std::move(params);
                route.handler(req, callback);
                //call_(route.handler, req, res, route.paramNames);
                return true;
            }
        }
        return false; // 404
    }
};

Router app;

class User : public HttpController<User>
{
public:
    std::string name;
    METHOD_BEG
    METHOD_ADD(getUserById, "/api/user/:id", GET)
    METHOD_END

    User()
    { }

    void getUserById(request &req, std::function<void(const response &)> &&callback, int id)
    {
        std::cout << id;
        response res{};
        callback(res);
    }
};

void fun1(int num)
{
    std::cout << num;
};


int main()
{
    User user;
    request req;
    response res;
    req.args = {"1", "helloworld"};
    std::function fun = [](request &req, std::function<decltype(callback)> &&cb, int num1, std::string str)
    {
        std::cout << num1 << str;
        response res{};
        cb(res);
    };


    //Regist("GET", "/api/user/{id}", func);
    call_(fun, req, std::move(callback));
    request requser;
    requser.url = "/api/user/3";
    requser.method = "GET";
    requser.args = {"3"};
    //make_handler(new User, &User::getUserById)(requser, std::move(callback));

    std::cout << app.route(requser, res) << std::endl;
}