#ifndef __LEADERPOLL_HPP__
#define __LEADERPOLL_HPP__

#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <queue>
#include <functional>
#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>

#include "../include/http.hpp"
#include "../include/buffer.hpp"
#include "../include/task_queue.hpp"
#include "../include/epoll_conn.hpp"
#include "../include/apiroute/Router.hpp"
#include "../include/signal.hpp"

using namespace apiroute;
///

class ThreadSet
{
    std::string ip = "127.0.0.1"; // INADDR_ANY
    int port = 8080;

    int MAX_EVENT_NUMBER = 1;
    int MAX_EPOLL_TABLE = 5;
    int epollfd;
    int listenfd;

    std::condition_variable cond;
    std::mutex mtx;
    bool hav_leader = false;
    std::vector<std::thread> threads;
    int THREAD_NUM = 1;

    //EventHand eventhands;
    std::unordered_map<int, conn *> conptrs;
    bool stop = false;

    task_queue TaskQueue;

public:
    ThreadSet() { }
    ThreadSet& setip(const std::string &_ip = "") { ip = _ip; return *this; }
    ThreadSet& setport(const int _port) { port = _port; return *this; }
    ThreadSet& seteventnum(const int _num = 1) { MAX_EVENT_NUMBER = _num; return *this; }//EPOLL监听事件个数
    ThreadSet& setepolltab(const int _num = 5) { MAX_EPOLL_TABLE = _num; return *this; }//EPOLL内核表大小
    ThreadSet& setthreadnum(const int _num = 1) { THREAD_NUM = _num; return *this; }// 线程数

    void init() {
        epollfd = epoll_create(MAX_EPOLL_TABLE);
        if (epollfd == -1)
        {
            throw "err: epoll_create();";
        }

        listenfd = socket(PF_INET, SOCK_STREAM, 0);
        int opt = 1;
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
            throw "err: setsockopt();";

        struct sockaddr_in addr;
        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        if (ip.empty())
            addr.sin_addr.s_addr = INADDR_ANY;
        else
            inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        addr.sin_port = htons(port);

        bind(listenfd, (struct sockaddr *)&addr, sizeof(addr));

        listen(listenfd, 5);
        conptrs.insert({listenfd, epoll_addconn(epollfd, listenfd, false)});

        signalctl::make_signalctl();
        {
            int out = signalctl::get_signalctl()->out();
            conptrs.insert({out, epoll_addconn(epollfd, out)});
        }
        for (int i = 0; i < THREAD_NUM; ++i)
        {
            threads.emplace_back(&ThreadSet::mainThread, this);
        }
    }
    ~ThreadSet() { }

    void deinit() {
        std::cout << "conptrs.size: " << conptrs.size() << std::endl;
        for (auto &[fd, ptr] : conptrs)
        {
            //std::cout << "ptr->fd:" << ptr->fd << std::endl;
            epoll_ctl(epollfd, EPOLL_CTL_DEL, ptr->fd, nullptr);
            if (ptr->fd != -1) { close(ptr->fd); ptr->fd = -1; }
            if (ptr) { delete ptr; ptr = nullptr; }
        }
        close(signalctl::get_signalctl()->in());
        close(epollfd);
    }
    void run() {
        init();
        for (auto &t : threads) {
            t.join();
        }
        deinit();
    }

    void mainThread() {
        while (true) {
            { // Follower
                std::unique_lock<std::mutex> lock(mtx);
                cond.wait(lock, [this]()
                          { return !hav_leader || stop; });
                if (stop) {
                    lock.unlock();
                    //cond.notify_one();
                    return;
                }
                hav_leader = true;
                std::cout << "new leader" << std::endl;
            }
            // Leader
            epoll_event events[MAX_EVENT_NUMBER];
            std::cout << "epoll_wait() beg..." << std::endl;
            int nums = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
            std::cout << "epoll_wait() end..." << std::endl;
            {
                std::unique_lock<std::mutex> lock(mtx);
                hav_leader = false;
            }
            cond.notify_one();
            // Processing
            for (int i = 0; i < nums; ++i)
            {
                conn *con = (conn *)events[i].data.ptr;
                int sockfd = con->fd;
                if (sockfd != listenfd && sockfd != signalctl::get_signalctl()->out())
                {
                    if (con->tdata.check_update())
                    { //未超时
                        std::cout << "[un time out]" << std::endl;
                    }
                    else
                    { // 超时
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
                        if (auto it = conptrs.find(sockfd); it != conptrs.end())
                            conptrs.erase(it);
                        if (sockfd != -1)
                        {
                            shutdown(sockfd, SHUT_RDWR);
                            close(sockfd);
                            sockfd = -1;
                        }
                        // shutdown(sockfd, SHUT_RD);
                        std::cout << "[time out]" << std::endl;
                        if (con)
                        {
                            delete con;
                            con = nullptr;
                        }
                        continue;
                    }
                }
                if ((sockfd == signalctl::get_signalctl()->out()) && (events[i].events & EPOLLIN))
                {
                    int ret = read_handle(con);
                    if (ret == -1)
                    {
                        std::cout << "OUT: sig err" << std::endl;
                    }
                    else if (ret == 0)
                    {
                        epoll_modconn(this->epollfd, con, EPOLLIN);
                        std::cout << "IN: sig modconn IN" << std::endl;
                    }
                    else
                    {
                        /*每个信号值占1字节，按字节逐个接收信号。安全地终止服务器主循环*/
                        for (int i = 0; i < con->buf.cont; ++i)
                        {
                            switch (con->buf.ptr()[i])
                            {
                            case SIGHUP:
                            {
                                //
                                break;
                            }
                            case SIGINT:
                            { // 终止线程池
                                if (!stop)
                                {
                                    stop = true;
                                    kill(getpid(), SIGINT);
                                    cond.notify_all();
                                }
                                return;
                                break;
                            }
                            case SIGALRM: {// 超时处理
                                auto &&tvc = TimerHeap::make_timerheap()->getoutvec();
                                for (const auto& td : tvc) {
                                    if (conptrs[td.fd]->tdata.is_timeout) { // 超时
                                        epoll_modconn(this->epollfd, con, EPOLLOUT);
                                        std::cout << "TimeOut: [fd:" << td.fd << "]" << std::endl;
                                    } else {
                                        conptrs[td.fd]->tdata.is_timeout = true;
                                        TimerHeap::make_timerheap()->add(timedata{td.fd, conptrs[td.fd]->tdata.gettimer()}); // 重新注册 延时
                                    }
                                }
                                break;
                            }
                            default:
                                break;
                            }
                        }
                        con->buf.cont = 0;
                        epoll_modconn(this->epollfd, con, EPOLLIN);
                        std::cout << "IN: sig modconn IN" << std::endl;
                    }
                }
                if (events[i].events & EPOLLIN)
                {
                    if (sockfd == listenfd) {
                        while (true)
                        {
                            struct sockaddr_in client;
                            socklen_t clientL = sizeof(client);
                            int connfd = accept(sockfd, (struct sockaddr *)&client, &clientL);
                            if (connfd == -1)
                            {
                                if (errno == EAGAIN || errno == EWOULDBLOCK)
                                {
                                    break; // 无更多连接，退出循环
                                }
                                else
                                {
                                    std::cout << "err: accept()" << std::endl;
                                    continue;
                                }
                            }
                            conn *conptr = nullptr;
                            conptrs.insert({connfd, conptr = epoll_addconn(epollfd, connfd)});
                            TimerHeap::make_timerheap()->add(timedata{connfd, conptr->tdata.gettimer()});
                            std::cout << "new accept" << std::endl;
                        }
                        epoll_modconn(this->epollfd, con, EPOLLIN);
                    } else
                    {
                        int ret = read_handle(con);
                        if (ret == 0) {
                            //reset_oneshot(epollfd, sockfd);
                            epoll_modconn(this->epollfd, con, EPOLLIN);
                            std::cout << "IN: modconn IN" << std::endl;
                        }
                        else if (ret == 1)
                        {
                            int rpret = con->req.request_parser(con->buf);
                            if (rpret == 1)
                            {
                                std::cout << con->req.url << std::endl;

                                //epoll_modconn(this->epollfd, con, EPOLLOUT);
                                TaskQueue.push([this, con]() {
                                    std::cout << con->req.method << " " << con->req.url << std::endl;
                                    request req;
                                    std::swap(req, con->req);
                                    auto result = app.route(req.method, req.url);
                                    if (result.handler == nullptr) {
                                        std::cout << "err: route ret nullptr";
                                        response badres;
                                        badres.version = "HTTP/1.1";
                                        badres.retcode = "404";
                                        badres.retcode = "Not Found";
                                        badres.body = "404 Not Found!";

                                        con->stat = conn::SENDSTAT::HEAD_AND_BODY_STR;
                                        std::string resstr = std::move(badres.head_str());

                                        std::string_view src(resstr);
                                        size_t copy_len = std::min((int)src.size(), con->buf.size);
                                        std::copy_n(src.data(), copy_len, con->buf.ptr());
                                        con->buf.cont = copy_len;
                                        epoll_modconn(this->epollfd, con, EPOLLOUT);
                                        std::cout << "add: EPOLLOUT" << std::endl;
                                        return;
                                    }
                                    req.args = result.params;
                                    req.query_params = result.query_params;

                                    result.handler(req, [this, con](const response &res) {
                                        if (con->section.is_Transfer_Encoding) {
                                            std::ostringstream ostr;
                                            ostr << std::hex << res.buff.size() << std::dec << "\r\n";
                                            ostr << res.buff << "\r\n";
                                            const std::string str = std::move(ostr.str());
                                            std::cout << "str:" << str << std::endl;

                                            char_buffer buf;
                                            size_t copy_len = std::min((int)str.size(), buf.size);
                                            std::copy_n(str.c_str(), copy_len, buf.ptr());
                                            buf.cont = copy_len;
                                            con->bufque.push(buf);
                                            epoll_modconn(this->epollfd, con, EPOLLOUT);
                                            std::cout << "add: EPOLLOUT" << std::endl;
                                            return;
                                        }
                                        httpsection newsection;
                                        con->section = std::move(newsection);
                                            if (res.is_file) {
                                                file_buffer newfilebuf;
                                                // open 文件
                                                newfilebuf.filefd = open(res.file.c_str(), O_RDONLY | O_NONBLOCK);
                                                int &fd = newfilebuf.filefd;
                                                if (fd != -1) {
                                                    struct stat st;
                                                    if (fstat(fd, &st) == -1) {
                                                        close(fd);
                                                        fd = -1;
                                                    } else {
                                                        newfilebuf.filesize = st.st_size;
                                                    }
                                                    con->section.Content_Length = newfilebuf.filesize;
                                                    
                                                } else {
                                                    std::cout << "dont find file:" << res.file << std::endl;
                                                }
                                                con->fbuf = std::move(newfilebuf);
                                                con->stat = conn::SENDSTAT::HEAD;
                                            } else {
                                                if (res.Transfer_Encoding) {
                                                    con->section.is_Transfer_Encoding = true;
                                                    con->stat = conn::SENDSTAT::HEAD;
                                                } else
                                                    con->stat = conn::SENDSTAT::HEAD_AND_BODY_STR;
                                            }
                                            std::string resstr = (con->stat == conn::SENDSTAT::HEAD_AND_BODY_STR ?
                                            std::move(res.head_str()) : std::move(res.head_str(con->section)));
                                            std::string_view src(resstr);
                                            size_t copy_len = std::min((int)src.size(), con->buf.size);
                                            std::copy_n(src.data(), copy_len, con->buf.ptr());
                                            con->buf.cont = copy_len;
                                            epoll_modconn(this->epollfd, con, EPOLLOUT);
                                            std::cout << "add: EPOLLOUT" << std::endl; 
                                        });
                                    });
                                std::cout << "IN: parser 1 ->" << ret << std::endl;
                            } else {
                                //std::cout << "ret 0 : req.buffer:" << con->req.buffer << std::endl;
                                epoll_modconn(this->epollfd, con, EPOLLIN);
                                std::cout << "IN: parser " << rpret << " ->IN" << std::endl;
                            }
                        } else {
                            epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
                            if (auto it = conptrs.find(sockfd); it != conptrs.end())
                                conptrs.erase(it);
                            if (sockfd != -1) {
                                close(sockfd);
                                sockfd = -1;
                            }
                            //shutdown(sockfd, SHUT_RD);
                            std::cout << "shutdown(RD);" << std::endl;
                            if (con) {
                                delete con;
                                con = nullptr;
                            }
                            
                        }
                    }
                }
                if (events[i].events & EPOLLOUT) {
                    std::cout << "outbuffer:" << std::string_view(con->buf.ptr(), con->buf.cont) << std::endl;
                    int ret = -1;
                    auto &stat = con->stat;
                    switch(stat) {
                    case conn::SENDSTAT::HEAD: {
                        //send
                        ret = write_handle(con);
                        if (ret == 1) {
                            stat = conn::SENDSTAT::BODY_FILE;
                            ret = 0;
                        }
                        break;
                    }
                    case conn::SENDSTAT::HEAD_AND_BODY_STR: {
                        //send
                        ret = write_handle(con);
                        break;
                    }
                    case conn::SENDSTAT::BODY_FILE: {
                        if (!con->section.is_Transfer_Encoding) {
                            // sendfile
                            ret = writefile_handle(con);
                            if (ret == 1 || ret == -1) {
                                close(con->fbuf.filefd);
                                con->fbuf.filefd = -1;
                            }
                            break;
                        } else {
                            //
                            while (!con->bufque.is_empty()) {
                                char_buffer &buf = con->bufque.pop();
                                ret = writeblock_handle(con->fd, buf);
                                if (ret != 1) {
                                    break;
                                }
                                else {
                                    if (*buf.ptr() == '0') {
                                        con->section.is_Transfer_Encoding = false;
                                    } else {
                                        ret = -2;
                                    }
                                }
                            }
                            break;
                        }
                    }
                    default:
                        break;
                    }
                    
                    if (ret == 0) {
                        // 重新注册读事件
                        epoll_modconn(this->epollfd, con, EPOLLOUT);
                        std::cout << "OUT: modconn OUT" << std::endl;
                    } else if (ret == 1) {
                        epoll_modconn(this->epollfd, con, EPOLLIN);
                        std::cout << "OUT: modconn IN ret:" << ret << std::endl;
                    } else {
                        //epoll_ctl(this->epollfd, EPOLL_CTL_DEL, con->fd, nullptr);
                        //shutdown(con->fd, SHUT_WR);
                        std::cout << "OUT: err" << std::endl;
                        // delete con;
                    }
                }
                if (sockfd != listenfd && sockfd != signalctl::get_signalctl()->out())
                {
                    if (con->tdata.check_update())
                    { // 未超时
                        std::cout << "[un time out]" << std::endl;
                    }
                    else
                    { // 超时
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
                        if (auto it = conptrs.find(sockfd); it != conptrs.end())
                            conptrs.erase(it);
                        if (sockfd != -1)
                        {
                            shutdown(sockfd, SHUT_RDWR);
                            close(sockfd);
                            sockfd = -1;
                        }
                        // shutdown(sockfd, SHUT_RD);
                        std::cout << "[time out]" << std::endl;
                        if (con)
                        {
                            delete con;
                            con = nullptr;
                        }
                        continue;
                    }
                }
            }
                //eventhands[events[i].events](sockfd);
        }
    }


    // 0:注册IN 1:注册OUT -1:关闭连接
    static int read_handle(conn* con)
    {
        ssize_t n = recv(con->fd, con->buf.ptr(), con->buf.size, 0);
        if (n > 0) {
            con->buf.cont += n;
        }
        else if (n == 0) {
            // 对端关闭连接
            return -1;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                // 当前无数据可读，退出循环，等待下次 EPOLLIN
                return 0;
            }
            else {
                // 错误
                return -1;
            }
        }
        std::cout << std::string_view(con->buf.ptr(), con->buf.cont) << std::endl;
        return 1;
    }

    // 0:注册OUT 1:注册IN -1:关闭连接
    static int write_handle(conn* con) {
        ssize_t n = send(con->fd, con->buf.ptr(), con->buf.cont, 0);
        
        if (n > 0) {
            con->buf.cont -= n;
            if (con->buf.cont <= 0) {
                //std::fill(con->buf.ptr(), con->buf.ptr() + con->buf.size, '\0');
                return 1;
            }
        } else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return 0;
            else 
                return -1;
        }

        return 0;
    }

    static int writefile_handle(conn *con)
    {
        if (con->fbuf.filefd == -1)
            return -1;
        ssize_t n = sendfile(con->fd, con->fbuf.filefd, &con->fbuf.fileoff, con->fbuf.filesize);
        if (n > 0)
        {
            con->fbuf.fileoff += n;
            con->fbuf.filesize -= n;
            if (con->fbuf.filesize <= 0) {
                // std::fill(con->buf.ptr(), con->buf.ptr() + con->buf.size, '\0');
                return 1;
            }
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return 0;
            else
                return -1;
        }

        return 0;
    }

    static int writeblock_handle(int fd, char_buffer& buf)
    {
        ssize_t n = send(fd, buf.ptr(), buf.cont, 0);

        if (n > 0)
        {
            buf.cont -= n;
            if (buf.cont <= 0)
            {
                // std::fill(con->buf.ptr(), con->buf.ptr() + con->buf.size, '\0');
                return 1;
            }
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return 0;
            else
                return -1;
        }

        return 0;
    }
};

#endif