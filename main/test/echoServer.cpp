
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <string>
#include <sstream>
#include <mutex>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
using namespace boost::beast;
using namespace boost::asio;

#include "../../include/tool.h"

const char *IP = "127.0.0.1";
const int port = 8080;
const int BUFFER_SIZE = 1024;
const int MAX_EVENT_NUMBER = 1024;

std::mutex mtx;
std::unordered_map<int, std::unique_ptr<http::request_parser<http::string_body>>> parsers;



void echoServer(int sockfd, int epollfd)
{
    char buffer[BUFFER_SIZE];
    memset(buffer, '\0', BUFFER_SIZE);
    ssize_t n = 0;
    while (true)
    {
        n = read(sockfd, buffer, BUFFER_SIZE);
        if (n <= 0)
        {
            close(sockfd);
            break;
        }
        if (write(sockfd, buffer, n) < 0)
        {
            break;
        }
        memset(buffer, 0, BUFFER_SIZE);
    }
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
    close(sockfd);
}

std::string request_to_string(http::request<http::string_body> const &req)
{
    std::ostringstream oss;

    // 1. 请求行: METHOD SP TARGET SP VERSION\r\n
    oss << req.method_string() << ' '
        << req.target() << ' '
        << "HTTP/" << req.version() / 10 << '.' << req.version() % 10 << "\r\n";

    // 2. 头部字段: 每个字段一行，格式 "name: value\r\n"
    for (auto const &field : req)
    {
        oss << field.name_string() << ": " << field.value() << "\r\n";
    }

    // 3. 头部结束空行
    oss << "\r\n";

    // 4. 消息体（如果存在）
    oss << req.body();

    return oss.str();
}

std::string response_to_string(http::response<http::string_body> const &res)
{
    std::ostringstream oss;

    // 1. 状态行: HTTP/版本 状态码 状态短语\r\n
    oss << "HTTP/" << res.version() / 10 << '.' << res.version() % 10 << ' '
        << res.result_int() << ' '
        << res.reason() << "\r\n";

    // 2. 头部字段
    for (auto const &field : res)
    {
        oss << field.name_string() << ": " << field.value() << "\r\n";
    }

    // 3. 头部结束空行
    oss << "\r\n";

    // 4. 消息体（如果存在）
    oss << res.body();

    return oss.str();
}

std::string connect_url(http::request<http::string_body> &requ);

void forwardServer(int sockfd, int epollfd)
{
    std::unique_lock<std::mutex> lock(mtx);
    auto it = parsers.find(sockfd);
    if (it == parsers.end())
    {
        lock.unlock();
        return;
    }
    lock.unlock();
    // it可能失效
    auto &parser = it->second;
    char buf[4096];
    ssize_t n = read(sockfd, buf, sizeof(buf));

    if (n > 0)
    {
        boost::system::error_code ec;
        parser->put(boost::asio::buffer(buf, n), ec);
        if (ec)
        { /* 解析错误 */
            epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
            close(sockfd);
            return;
        }

        if (parser->is_done())
        {
            auto const &req = parser->get();
            // 处理请求...

            // http::response<http::string_body> res{http::status::ok, req.version()};
            // res.body() = "Echo: " + req.body();
            // res.prepare_payload();
            //  1. 构造一个 GET 请求，目标路径 "/"，HTTP/1.1
            //http::request<http::string_body> req{http::verb::get, "/", 11};

            // 2. 设置必要的头部
            //req.set(http::field::host, "example.com");
            // 可选：添加其他头部，例如 User-Agent
            //req.set(http::field::user_agent, "Beast minimal client");

            std::string res_str = connect_url(parser->get());
            std::cout << res_str << '\n';
            
            write(sockfd, res_str.c_str(), res_str.size());

            // 清理或重置 parser 以便复用
            // 这里简单关闭连接作为示例
            // close_conn(sockfd, parsers);

            epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
            close(sockfd);
        }
        else
        {
            // 重新设置触发 等待下次数据读取
            reset_oneshot(epollfd, sockfd);
            return;
        }
    }
    else
    {
        // n == 0 or error
        // close_conn(sockfd, parsers);
        epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
        close(sockfd);
    }
}

int main()
{

    int sock = socket(PF_INET, SOCK_STREAM, 0);

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        return 0;
    // struct linger tmp = {1, 0};
    // setsockopt(sock, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, IP, &addr.sin_addr);
    addr.sin_port = htons(port);

    bind(sock, (struct sockaddr *)&addr, sizeof(addr));

    listen(sock, 5);
    std::cout << "listen...\n";
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    if (epollfd == -1)
    {
        std::cout << "err: epoll_create();";
        return 0;
    }
    addfd(epollfd, sock, false);

    while (true)
    {
        std::cout << "epoll_wait()...\n";
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        std::cout << "epoll_wait() end\n";
        if (ret < 0)
        {
            std::cout << "err: epoll_wait();\n";
            break;
        }

        for (int i = 0; i < ret; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == sock)
            {
                struct sockaddr_in client;
                socklen_t clientL = sizeof(client);
                std::cout << "accept()...\n";
                int connfd = accept(sock, (struct sockaddr *)&client, &clientL);
                char remote[INET_ADDRSTRLEN];
                std::cout << "accept: "
                          << "ip:" << inet_ntop(AF_INET, &client.sin_addr, remote, INET_ADDRSTRLEN)
                          << "port:" << ntohs(client.sin_port) << '\n';
                addfd(epollfd, connfd, true);
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    parsers[connfd] = std::make_unique<http::request_parser<http::string_body>>();
                    parsers[connfd]->body_limit(1024 * 1024); // 限制body大小
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                std::thread threadone(forwardServer, sockfd, epollfd);
                threadone.detach();
                std::cout << "new thread\n";
            }
            else
            {
                std::cout << "next socket\n";
            }
        }
    }
    close(sock);
    return 0;
}

std::string get_chunked_response_auto(const std::string &host, http::request<http::string_body> &req);

std::string connect_url(http::request<http::string_body> &requ)
{

    const char *hostname = "example.com";
    const char *port = "80"; // HTTP 端口

    // 2. 构建 HTTP GET 请求
    requ.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    // 无需手动设置分块相关头，Beast 会自动识别服务端返回的 Transfer-Encoding

    requ.set(http::field::host, hostname);
    //requ.set(http::field::connection, "close"); // 强制关闭连接
    requ.erase(http::field::accept_encoding);
    requ.erase(http::field::if_modified_since);
    requ.set(http::field::te, "close"); // 关键：告诉服务器不要用分块编码

    std::string requ_str = request_to_string(requ);
    std::cout << requ_str << '\n';

    std::string res_str = get_chunked_response_auto(hostname, requ);
    std::cout << res_str << std::endl;
    return res_str;
}

// 同步请求并自动解析分块响应
std::string get_chunked_response_auto(const std::string &host, http::request<http::string_body> &req)
{
    try
    {
        // 1. 初始化 ASIO 上下文和 TCP 连接
        io_context ioc;
        ip::tcp::resolver resolver(ioc);
        ip::tcp::socket socket(ioc);

        // 解析域名并连接
        auto const results = resolver.resolve(host, "80");
        net::connect(socket, results.begin(), results.end());

        

        // 3. 发送请求
        http::write(socket, req);

        // 4. 接收响应（核心：Beast 自动解析分块）
        flat_buffer buffer; // 用于存储临时数据
        http::response<http::string_body> res;

        // read 函数会自动检测 Transfer-Encoding: chunked，解析所有块并拼接成完整 body
        http::read(socket, buffer, res);

        // 5. 输出结果
        //std::cout << "=== 响应头 ===" << std::endl;
        //std::cout << res.base() << std::endl;

        //std::cout << "\n=== 自动解析后的完整响应体 ===" << std::endl;
        //std::cout << res.body() << std::endl;

        return response_to_string(res);

        // 6. 关闭连接
        error_code ec;
        socket.shutdown(ip::tcp::socket::shutdown_both, ec);
        if (ec && ec != errc::not_connected)
            throw system_error{ec};
    }
    catch (std::exception const &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

std::string connect_url_1(http::request<http::string_body> &requ)
{

    const char *hostname = "example.com";
    const char *port = "80"; // HTTP 端口

    // 设置 hints 结构，告诉 getaddrinfo 我们想要什么类型的连接
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; // TCP 流式socket
    hints.ai_flags = AI_CANONNAME;   // 获取规范主机名（可选）

    if (getaddrinfo(hostname, port, &hints, &res) != 0)
    {
        std::cout << "err: getaddrinfo();" << std::endl;
        return "err";
    }

    int sockfd = -1;
    // 遍历返回的地址链表，尝试创建socket并连接
    for (rp = res; rp != nullptr; rp = rp->ai_next)
    {
        // 创建一个socket
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1)
        {
            continue; // 创建失败，尝试下一个地址
        }

        // 打印当前尝试连接的IP地址
        char ipstr[INET6_ADDRSTRLEN];
        void *addr;
        if (rp->ai_family == AF_INET)
        { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)rp->ai_addr;
            addr = &(ipv4->sin_addr);
        }
        inet_ntop(rp->ai_family, addr, ipstr, sizeof ipstr);
        std::cout << "尝试连接 " << ipstr << " ..." << std::endl;

        // 连接到服务器
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
        {
            break; // 连接成功，跳出循环
        }

        // 连接失败，关闭socket，继续尝试下一个地址
        close(sockfd);
        sockfd = -1;
    }

    if (sockfd == -1)
    {
        std::cerr << "无法连接到任何地址" << std::endl;
        freeaddrinfo(res); // 释放地址链表
        return "err";
    }
    std::cout << "连接成功！" << std::endl;

    requ.set(http::field::host, hostname);
    requ.set(http::field::connection, "close"); // 强制关闭连接
    requ.erase(http::field::accept_encoding);
    requ.erase(http::field::if_modified_since);
    requ.set(http::field::te, "close"); // 关键：告诉服务器不要用分块编码

    std::string requ_str = request_to_string(requ);
    std::cout << requ_str << '\n';
    // write(sockfd, requ_str.c_str(), requ_str.size());

    // 发送请求（循环确保完整）
    const char *data = requ_str.data();
    size_t remaining = requ_str.size();
    while (remaining > 0)
    {
        ssize_t sent = write(sockfd, data, remaining);
        if (sent <= 0)
        {
            if (errno == EINTR)
                continue;
            std::cout << "write\n";
            close(sockfd);
            return "err";
        }
        data += sent;
        remaining -= sent;
    }

    // 2. 准备响应解析器
    http::response_parser<http::string_body> parser;
    parser.body_limit(1024 * 1024); // 可选：设置 body 大小限制

    char buf[4096];
    bool keep_reading = true;

    while (true)
    {
        std::cout << "read start " << std::flush;
        ssize_t n = read(sockfd, buf, sizeof(buf));
        std::cout << "read: n = " << n << std::endl;
        if (n > 0)
        {
            boost::system::error_code ec;
            parser.put(boost::asio::buffer(buf, n), ec);
            if (ec)
            {
                std::cout << "Parse error: " << ec.message() << std::endl;
                break; // 解析出错，退出循环
            }

            if (parser.is_done())
            {
                // 响应解析完成
                auto const &res = parser.get();
                // std::cout << "=== Response received ===" << std::endl;
                // std::cout << response_to_string(res) << std::endl;

                std::cout << "Body size: " << res.body().size() << std::endl;
                // 可选：输出 body 前 100 字节
                std::cout << "Body preview: " << res.body().substr(0, 100) << std::endl;

                // 如果希望支持持久连接，可以重置 parser 并继续发送下一个请求
                // 这里简单处理，退出循环
                close(sockfd);
                return response_to_string(res);
                // return res.body();
                //  keep_reading = false;
            }
            else
            {
                std::cout << "!is_done()" << std::endl;
            }
            // 否则继续读取更多数据
        }
        else if (n == 0)
        {
            // 对端关闭连接
            std::cout << "Server closed connection." << std::endl;
            if (parser.is_done())
            {
                auto const &response = parser.get();
                std::string res_str = response_to_string(response);
                std::cout << res_str << std::endl;
                close(sockfd);
                return res_str;
            }
            break;
        }
        else
        {
            // read 出错
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 非阻塞模式下暂时无数据，应等待下一次事件
                // 这里假设使用阻塞 socket，所以不会发生
                std::cout << "err: read" << std::endl;
                continue;
            }
            else
            {
                std::cout << "read error" << std::endl;
                break;
            }
        }
    }

    close(sockfd);
    return "err";
}

