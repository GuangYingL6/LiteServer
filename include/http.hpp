#ifndef __HTTP_H__
#define __HTTP_H__

#include <string>
#include <list>
#include <iostream>
#include <sstream>
#include <map>
#include <unordered_map>
#include <vector>

#include <sys/uio.h>

#include "buffer.hpp"

static const std::string WEBROOT{"../../../Web"}; // /main/src/outout/main.o

struct httpsection
{
    httpsection() {};
    struct Host
    {
        std::string hostname;
        int port;
    } host;
    bool is_Connection{true};
    unsigned long Content_Length{0};
    bool is_Transfer_Encoding{false};
    std::string Content_Type;
};

static const std::map<const std::string, const std::string> MEMI{
    {".txt", "text/plain"},
    {".html", "text/html"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"}
};

class request {
public:
    enum class METHOD
    {
        UNDFINE, 
        GET,
        OTHER
    };

    std::string method = "";
    std::string url;
    std::string version;
    std::string buffer;


    httpsection section;

    enum parser_stat
    {
        READ_LINE,
        PARSER_LINE,
        REQUEST_END
    };
    parser_stat stat = READ_LINE;

    request() {}
    ~request() {}

    
    int request_parser(char_buffer &_buffer)
    {
        request& req = *this;
        char_buffer buffer;
        buffer.swap(_buffer);
        char *h = buffer.ptr();
        while (true)
        {
            switch (stat)
            {
            case READ_LINE:
            {
                char *p = h;
                for (; p - h < buffer.cont && *p != '\n'; ++p)
                {
                }
                if (p - h < buffer.cont)
                {
                    if (h == buffer.ptr())
                    {
                        req.buffer += std::string(h, p);
                    }
                    else
                    {
                        req.buffer = std::string(h, p);
                    }
                    h = p + 1;
                    stat = PARSER_LINE;
                }
                else
                {
                    req.buffer = std::string(h, p);
                    return 0;
                }
                break;
            }
            case PARSER_LINE:
            {
                std::string buf = "";
                std::swap(buf, req.buffer);
                std::cout << "req.buffer:" << buf << std::endl;
                if (req.method == "")
                {
                    int idx = 0;
                    // idx != std::string::npos;
                    int i = buf.find(' ', idx);
                    req.method = buf.substr(idx, i - idx);
                    
                    idx = i + 1;
                    i = buf.find(' ', idx);
                    req.url = buf.substr(idx, i - idx);
                    idx = i + 1;
                    i = buf.find(' ', idx);
                    req.version = buf.substr(idx, i - idx);

                    std::cout << " url:" << req.url << std::endl;
                }
                else
                {
                    // 字段
                    //std::cout << buf.size() << std::endl;
                    if (buf == "\r")
                    {
                        stat = REQUEST_END;
                        break;
                    }
                    int idx = buf.find(':');
                    std::string_view key(buf.c_str(), idx);
                    std::string_view value(buf.c_str() + idx + 2, buf.size() - idx - 3);
                    if (key == "Host") {
                        int i = value.find(':');
                        if (i != -1) {
                            section.host.port = stoi(std::string(value.substr(i + 1, value.size() - i - 1)));
                        } else
                            i = value.size();
                        section.host.hostname = value.substr(0, i);
                    }
                    else if (key == "Connection") {
                        if (value == "close")
                            section.is_Connection = false;
                    }
                    else if (key == "Content-Length") {
                        section.Content_Length = stol(std::string(value));
                    }
                    else if (key == "Transfer-Encoding") {
                        section.is_Transfer_Encoding = true;
                    }
                    else if (key == "Content_Type") {
                        section.Content_Type = value;
                    }
                }
                stat = READ_LINE;
                break;
            }
            case REQUEST_END:
            {
                return 1;
                break;
            }
            default:
                return -1;
                break;
            }
        }
    }

public:
    std::vector<std::string> args; // 路径参数
    std::unordered_map<std::string, std::string> query_params; // 查询参数
};

class response {
public:
    std::string version;
    std::string retcode;
    std::string reason;

    std::string body;

    bool is_file{false};
    std::string file;
    std::string type{"text/html"};
    std::string head_str(httpsection &section) const {
        std::ostringstream oss;
        section.Content_Type = type;
        oss << version << ' ' << retcode << ' ' << reason << "\r\n";
        if (section.is_Transfer_Encoding) 
            oss << "Transfer-Encoding: chunked" << "\r\n";
        else
            oss << "Content-Length: " << (is_file ? section.Content_Length : (unsigned long)body.size()) << "\r\n";

        oss << "Content-Type: " << section.Content_Type << "\r\n"
            << "\r\n";
        if (!is_file) {
            oss << body;
        }
        return oss.str();
    }

    std::string head_str() const {
        httpsection sec{};
        sec.Content_Length = body.size();
        return head_str(sec);
    }


    void setfile(const std::string &file_path)
    {
        is_file = true;
        int pos = file_path.find_last_of('/');
        if (int i = file_path.find_last_of('.'); i != -1) {
            auto it = MEMI.find(file_path.substr(pos, i - pos));
            type = (it != MEMI.end() ? it->second : "text/plain");
        }
        file = std::move(WEBROOT + file_path); // /main/src/output/a.o
    }

    
public:
    bool Transfer_Encoding = false;
    std::string buff;
};

#endif