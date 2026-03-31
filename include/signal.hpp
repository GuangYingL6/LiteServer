#ifndef __SIGNAL_HPP__
#define __SIGNAL_HPP__

#include <sys/socket.h>
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <mutex>

#include "epoll_conn.hpp"

class signalctl {
    static signalctl *ptr;
    static std::mutex mtx;
    static int pipefd[2];
    signalctl()
    {
        /*使用socketpair创建管道，注册pipefd[0]上的可读事件*/
        if (socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd) == -1)
            throw "sig pipe err";
        setnonblocking(pipefd[1]);
        /*设置一些信号的处理函数*/
        addsig(SIGHUP);
        addsig(SIGPIPE);
        addsig(SIGINT);
    }

public:
    static signalctl* make_signalctl() {
        if (ptr == nullptr) {
            std::unique_lock<std::mutex> lock(mtx);
            if (ptr == nullptr) {
                ptr = new signalctl;
            }
        }
        return ptr;
    }
    static signalctl *get_signalctl() { return ptr; }
    ~signalctl() {}

    int in() { return pipefd[1]; }
    int out() { return pipefd[0]; }

    // 信号处理函数
    static void sig_handler(int sig)
    {
        /*保留原来的errno，在函数最后恢复，以保证函数的可重入性*/
        int save_errno = errno;
        int msg = sig;
        send(pipefd[1], (char *)&msg, 1, 0); /*将信号值写入管道，以通知主循环*/
        errno = save_errno;
    }
    // 设置信号的处理函数
    static void addsig(int sig)
    {
        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sa.sa_handler = sig_handler;
        sa.sa_flags |= SA_RESTART;
        sigfillset(&sa.sa_mask);
        if (sigaction(sig, &sa, NULL) == -1)
            throw;
    }
};

signalctl *signalctl::ptr{nullptr};
std::mutex signalctl::mtx;
int signalctl::pipefd[2]{};

#endif