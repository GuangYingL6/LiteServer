#include "../json.hpp"

class User : public json::JsonBase<User>
{
public:
    std::string name;
    int year;
    std::array<std::string, 3> vecname;

    DEF_STRING(name);
    DEF_STRING(year);
    DEF_STRING(vecname);
    DEF_JSONLIST(MAKE_JSONTYPE(name), MAKE_JSONTYPE(year), MAKE_JSONTYPE(vecname));
};

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