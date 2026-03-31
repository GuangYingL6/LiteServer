#include "../Router.hpp"


class User : public HttpController<User>
{
public:
    std::string name;
    METHOD_BEG
    METHOD_ADD(getUserById, "/api/user/{id}", GET)
    METHOD_ADD(postUserById, "/api/user/{id}", POST)
    METHOD_END

    User()
    {
    }

    void getUserById(request &req, std::function<void(const response &)> &&callback, int id)
    {
        std::cout << "id: " << id << " name: " << name << std::endl;
        response res{};
        callback(res);
    }

    void postUserById(request &req, std::function<void(const response &)> &&callback, int id)
    {
        auto newname = req.query_params["name"];
        std::cout << "post oldname: " << name << " = newname: " << newname << std::endl;
        name = newname;
        response res{};
        callback(res);
    }
};

int main()
{
    User user;
    request req1;
    req1.method = "POST";
    req1.url = "/api/user/10?name=newname";
    {
        auto result = app.route(req1.method, req1.url);
        if (result.handler == nullptr)
        {
            std::cout << "err: route ret nullptr";
            return 0;
        }
        req1.args = result.params;
        req1.query_params = result.query_params;
        result.handler(req1, [](const response &) {});
    }
    request req;
    req.method = "GET";
    req.url = "/api/user/10";
    auto msg = app.route(req.method, req.url);
    req.args = msg.params;

    msg.handler(req, [](const response &) {});
}

