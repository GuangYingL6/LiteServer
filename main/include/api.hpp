#ifndef __API_HPP__
#define __API_HPP__

#include "../../include/apiroute/Router.hpp"

class IndexServlet : public apiroute::HttpController<IndexServlet>
{
public:
    METHOD_BEG
    METHOD_ADD(getIndex, "/", GET)
    METHOD_END

    IndexServlet() { }

    void getIndex(request &req, std::function<void(const response &)> &&callback)
    {

        response res{};
        res.reason = "OK";
        res.version = "HTTP/1.1";
        res.retcode = "200";
        //res.body =  "Hello, world!";
        //res.setfile("/res/img/index.jpg");
        res.Transfer_Encoding = true;
        callback(res);
        //std::this_thread::sleep_for(std::chrono::seconds(1));
        res.buff = 
R"(<!DOCTYPE html>
<html lang="zh-CN">)";
        callback(res);
        //std::this_thread::sleep_for(std::chrono::seconds(1));
        res.buff =
            R"(<body>
    <div>
        <h1>HELLO</h1>
        <p>world!</p>
    </div>
</body>

</html>)";
        callback(res);
        //std::this_thread::sleep_for(std::chrono::seconds(1));
        res.buff = "";
        callback(res);
    }
};

#endif