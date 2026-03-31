#ifndef __ROUTER_HPP__
#define __ROUTER_HPP__

#include <string>
#include <sstream>
#include "HttpController.hpp"

namespace apiroute {

    struct TrieNode
    {
        std::unordered_map<std::string, std::unique_ptr<TrieNode>> children;
        std::string param_name;                                   // 如果是参数段，记录参数名
        std::unordered_map<std::string, RequestHandler> handlers; // 方法到处理器的映射
    };

    class Router
    {
        TrieNode root_;

    public:
        void addRoute_lambda(const std::string &&method, const std::string &&path, const RequestHandler &lambda)
        {
            std::apply([this](auto &&...args)
                       { this->addRoute(std::forward<decltype(args)>(args)...); }, create_handler(lambda, path, method));
        }

        void addRoute(const RequestHandler &&handler, const std::string &&path, const std::string &&method)
        {
            TrieNode *node = &root_;
            std::stringstream ss(path);
            std::string segment;
            while (std::getline(ss, segment, '/'))
            {
                if (segment.empty())
                    continue; // 忽略开头的空段

                // 判断是否为参数段（例如以 { 开头且以 } 结尾）
                if (segment.front() == '{' && segment.back() == '}')
                {
                    std::string param_name = segment.substr(1, segment.size() - 2);
                    if (!node->param_name.empty() && node->param_name != param_name)
                    {
                        // 参数名冲突，可报错或忽略
                    }
                    // 如果还没有参数子节点，创建一个
                    if (!node->children["*"])
                    { // 用 "*" 作为参数段的特殊键
                        node->children["*"] = std::make_unique<TrieNode>();
                        node->children["*"]->param_name = param_name;
                    }
                    node = node->children["*"].get();
                }
                else
                {
                    // 静态段
                    if (!node->children[segment])
                    {
                        node->children[segment] = std::make_unique<TrieNode>();
                    }
                    node = node->children[segment].get();
                }
            }
            // 在最终节点上设置处理器
            node->handlers[method] = std::move(handler);
        }

        struct MatchResult
        {
            RequestHandler handler;
            std::vector<std::string> params; // 按参数顺序存储提取的值
            std::unordered_map<std::string, std::string> query_params;
        };
        MatchResult route(const std::string &method_str, const std::string &full_url)
        {
            // 分离路径和查询字符串
            std::string path = full_url;
            std::string query_str;
            size_t qpos = full_url.find('?');
            if (qpos != std::string::npos)
            {
                path = full_url.substr(0, qpos);
                query_str = full_url.substr(qpos + 1);
            }

            std::string method = method_str; // 自行实现转换
            TrieNode *node = &root_;
            std::vector<std::string> params;
            std::stringstream ss(path);
            std::string segment;
            while (std::getline(ss, segment, '/'))
            {
                if (segment.empty())
                    continue;

                // 先尝试静态匹配
                if (node->children.count(segment))
                {
                    node = node->children[segment].get();
                }
                // 再尝试参数匹配
                else if (node->children.count("*"))
                {
                    node = node->children["*"].get();
                    params.push_back(segment); // 将实际段的值作为参数收集
                }
                else
                {
                    return {nullptr, {}, {}}; // 匹配失败
                }
            }

            auto it = node->handlers.find(method);
            if (it != node->handlers.end())
            {
                // 解析查询字符串
                auto query_map = parse_query(query_str);
                return {it->second, std::move(params), std::move(query_map)};
            }
            return {nullptr, {}, {}};
        }

        std::unordered_map<std::string, std::string> parse_query(const std::string &query_str)
        {
            std::unordered_map<std::string, std::string> result;
            std::stringstream ss(query_str);
            std::string pair;
            while (std::getline(ss, pair, '&'))
            {
                auto eq = pair.find('=');
                if (eq != std::string::npos)
                {
                    std::string key = pair.substr(0, eq);
                    std::string value = pair.substr(eq + 1);
                    // 简单 URL 解码（如果需要）
                    // value = url_decode(value);
                    result[key] = value;
                }
                else
                {
                    // 无值的参数，如 ?debug
                    result[pair] = "null";
                }
            }
            return result;
        }
    };

#define USE_ROUTER_APP \
    apiroute::Router app;

};

extern apiroute::Router app;
#endif