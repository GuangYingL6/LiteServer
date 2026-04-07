
#include "../../include/apiroute/Router.hpp"
USE_ROUTER_APP
#include "../../include/LeaderPool.hpp"
#include "../include/api.hpp"

int main()
{
    setwebroot("../../../Web");
    ThreadSet ts;
    ts.setip("127.0.0.1")
        .setport(8080)
        .seteventnum(1)
        .setepolltab(5)
        .setthreadnum(1);
    app.addRoute_lambda("GET", "/api/lambda",
                        [](request &req, std::function<void(const response &)> &&callback)
                        {
                            std::cout << req.body << std::endl;
                            response res;
                            res.reason = "OK";
                            res.version = "HTTP/1.1";
                            res.retcode = "200";
                            res.body = req.body;
                            // res.setfile("/res/img/index0.jpg");
                            std::cout << "lambda" << std::endl;
                            callback(res);
                        });

    IndexServlet idxs;
    
    ts.run();

    return 0;
}