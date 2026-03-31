# LiteServer
一款基于C++的轻量级HTTP应用框架，可以方便的构建C++Web应用服务端

## 特点
+ 基于epoll的非阻塞IO框架
+ 使用领导者追随者的并发模式解析HTTP请求
+ 使用线程池执行注册的api服务
+ 通过模板元和宏实现REST ful风格api注册与路由，编译期提取信息 运行时注册路由
+ 通过模板元实现json解析与对象序列化，轻松适配自定义类型
+ 通过sendfile零拷贝传输本地静态文件
+ 统一处理信号，安全的资源释放与结束进程

## 目录结构
```
/include      # 所有功能实现为hpp文件
     /apiroute/      #实现api路由
     /json/          #实现JSON支持
    ./buffer.hpp     #缓冲区管理
    ./epoll_conn.hpp #epoll注册的连接信息
    ./http.hpp       #请求和响应类
    ./signal.hpp     #统一信号源
    ./task_queue.hpp #任务队列线程池(互斥锁&条件变量)
    ./tool.h         #用于epoll注册的工具函数
    ./LeaderPool.hpp #核心框架文件
```

## 使用
### 注册路由
```cpp
#include "../../include/apiroute/Router.hpp"
USE_ROUTER_APP// 定义路由对象app
#include "../../include/LeaderPool.hpp"
#include "../include/api.hpp"//注册api需要使用app对象

int main() {...}
```
+ 使用LeaderPoll、注册api前，使用USE_ROUTER_APP宏定义路由对象仅一次(app)

```cpp
#include "../../include/apiroute/Router.hpp"

// 继承HttpController类
class IndexServlet : public apiroute::HttpController<IndexServlet>
{
public:
    // 在此宏之间注册路由
    METHOD_BEG
    METHOD_ADD(getIndex, "/", GET)
    METHOD_END

    IndexServlet() { }

    // 必须支持req和callback参数
    void getIndex(request &req, std::function<void(const response &)> &&callback)
    {

        response res{};
        res.reason = "OK";
        res.version = "HTTP/1.1";
        res.retcode = "200";
        res.body =  "Hello, world!";
        callback(res);// 结束时要回调callback
        // 框架会生成响应并返回
    }
};
```
+ 成员函数的前两个参数必须为
    + `request &`
    + `std::function<void(const response &)> &&`
    + 后面可以添加其他参数(如：`/api/user/{id}`)
    + REST ful api的`?name=abc`后的参数 可用`req.query_params["name"]`获取，类型为`std::string`
+ 可以用body对象编写响应文本
```cpp
res.setfile("/res/img/index.jpg");
```
+ 可以发送文件
+ 设置Web资源文件路径`setwebroot("../../../Web")`
    + `/Web`相对于可执行文件的路径
```cpp
//
        res.Transfer_Encoding = true;
        callback(res);
        res.buff = 
R"(<!DOCTYPE html>
<html lang="zh-CN">)";
        callback(res);
        res.buff =
            R"(<body>
    <div>
        <h1>HELLO</h1>
        <p>world!</p>
    </div>
</body>

</html>)";
        callback(res);
        // 结束分块传输：发送空的buff
        res.buff = "";
        callback(res);
```
+ 也可以启用分块传输功能：Transfer_Encoding:`true`
+ 每次分块传输都要`callback(res)`
+ 结束需要发送空的buff

```cpp
int main() {
    setwebroot("../../../Web");
    IndexServlet idxs;
    ...
```
+ 运行服务器前，先设置Web路径，构造api对象(`IndexServlet`)

### 简易注册
如果想更轻松的注册api，也可以使用lambda
```cpp
int main() {
    ...
    app.addRoute_lambda("GET", "/api/lambda",
        [](request &req, std::function<void(const response &)> &&callback)
        {
            response res;
            res.reason = "OK";
            res.version = "HTTP/1.1";
            res.retcode = "200";
            res.setfile("/res/img/index0.jpg");
            std::cout << "lambda" << std::endl;
            callback(res);
        });
```
+ 使用路由对象`app`，传递lambda函数直接注册api

### 启动服务
```cpp
#include "../../include/LeaderPool.hpp"
int main() {
    ThreadSet ts;
    ts.setip("127.0.0.1")
        .setport(8080)
        .seteventnum(1)
        .setepolltab(5)
        .setthreadnum(1);

    ts.run();// 循环等待事件
    return 0;
}
```
+ 创建线程集`ThreadSet`，设置参数
+ 调用`run()`开始服务
    + 初始化描述符
    + epoll_wait等待事件
    + 处理事件
    + 收到SIGINT信号
    + 回收资源

### JSON
可以较为方便的为自定义对象提供JSON支持
```cpp
#include "../include/json/json.hpp"

// 继承JsonBase对象
class User : public json::JsonBase<User>
{
public:
    std::string name;
    int year;
    std::array<std::string, 3> vecname;

    // 注册
    DEF_STRING(name);
    DEF_STRING(year);
    DEF_STRING(vecname);
    DEF_JSONLIST(MAKE_JSONTYPE(name),
                 MAKE_JSONTYPE(year), 
                 MAKE_JSONTYPE(vecname));
};
```
+ 继承`public json::JsonBase<T>`对象
+ 对于想要通过json序列化的成员
    + 使用`DEF_STRING()`宏预先定义
    + 最后使用`DEF_JSONLIST()`宏定义列表

对数组的支持：
+ 支持`std::vector<T>`、`std::array<T, N>`作为数组
+ 不支持不同元素类型的数组
+ 不支持原生的数组`T []`

如果成员也是自定义类，会递归序列化成员(必须提供JSON支持)

> 注：必须传递正确的JSON 且不能包含占位符如`{"key": value}`，否则会出现无法预料的错误

### 使用JSON
```cpp
int main()
{
    using namespace json;
    User u;
    u.name = "n1";
    u.year = 10;
    u.vecname = {"abc", "123", "嗨嗨嗨"};
    std::string s;
    std::cout << (s = serialization(u)) << std::endl;
    auto u1 = deserialization<User>(s);
    std::cout << (u1.serialization()) << std::endl;
    return 0;
}
```
+ `deserialization<T>(str)`：解析JSON字符串`str`为`T`类型
+ `serialization(T)`：将T类型的对象序列化为JSON字符串

## 后记
+ 此项目还存在许多不足之处，目前该框架可以便捷的开发简易的Web应用