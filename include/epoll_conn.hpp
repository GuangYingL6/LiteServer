#ifndef __EPOLL_CONN_HPP__
#define __EPOLL_CONN_HPP__

#include <fcntl.h>
#include <sys/epoll.h>

#include "buffer.hpp"
#include "http.hpp"
#include "timer/timer_heap.hpp"

struct conn
{
    conn(int _fd) : fd(_fd) {}
    ~conn() {}
    enum class SENDSTAT
    {
        NULLSTAT,
        HEAD,
        HEAD_AND_BODY_STR,
        BODY_FILE
    };

    int fd;
    char_buffer buf;
    request req;
    SENDSTAT stat{SENDSTAT::NULLSTAT};
    file_buffer fbuf;
    httpsection section{};
    char_buffer_que bufque;
    retimedata tdata{};
};

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, old_option | O_NONBLOCK);
    return old_option;
}

conn* epoll_addconn(int epollfd, int fd, bool use_oneshot = true)
{
    epoll_event event;
    event.data.ptr = static_cast<void *>(new conn{fd});
    event.events = EPOLLIN | EPOLLET;
    if (use_oneshot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
    return static_cast<conn*>(event.data.ptr);
}

void epoll_modconn(int epollfd, conn *con, int EVENT)
{
    epoll_event event;
    event.data.ptr = (void *)con;
    event.events = EVENT | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, con->fd, &event);
}


#endif