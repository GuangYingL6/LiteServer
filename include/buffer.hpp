#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <memory>
#include <deque>
#include <fcntl.h>

const int BUFFER_SIZE = 1024;
class char_buffer
{
    std::unique_ptr<char[]> buffer;

public:
    int size{0};
    int cont{0};

    char_buffer(const int _size = BUFFER_SIZE) : buffer(new char[_size]), size(_size), cont(0)
    {
        std::fill(buffer.get(), buffer.get() + size, '\0');
    }
    char_buffer(char_buffer &&obj) : buffer(obj.buffer.release()), size(obj.size), cont(obj.cont) {}
    ~char_buffer() {}

    void swap(char_buffer &obj)
    {
        std::swap(buffer, obj.buffer);
        std::swap(size, obj.size);
        std::swap(cont, obj.cont);
    }
    char *ptr()
    {
        return buffer.get();
    }
};

class char_buffer_que {
    std::deque<char_buffer> que{};
public:
    char_buffer_que() {}
    ~char_buffer_que() { que.clear(); }
    void push(char_buffer& buf) {
        que.push_back(std::move(buf));
    }

    char_buffer& pop() {
        return que.front();
    }
    bool is_empty() {
        while (!que.empty() && que.front().cont <= 0)
            que.pop_front();
        return que.empty();
    }
};

struct file_buffer
{
    int filefd{-1};
    off_t fileoff{0};
    size_t filesize{0};
};

#endif