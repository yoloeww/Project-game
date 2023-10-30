#include "server.hpp"
#include "util.hpp"
#define HOST "127.0.0.1"
#define PORT 3306
#define USER "root"
#define PASS "qwer@wu.888"
#define DBNAME "gobang"
#include "online.hpp"
#include "db.hpp"
#include "room.hpp"
#include "session.hpp"

// int main() {
//     MYSQL *mysql = mysql_util::mysql_create(HOST, USER, PASS, DBNAME, PORT);
//     const char *sql = "insert stu values(null, '小黑', 18, 53, 68, 87);";
//     bool ret = mysql_util::mysql_exec(mysql, sql);
//     if (ret == false) {
//         return -1;
//     }
//     mysql_util::mysql_destroy(mysql);
//     return 0;
// }
using namespace std;
void db_test() {
    user_table ut(HOST, USER, PASS, DBNAME, PORT);
    Json::Value user;
    user["username"] = "xiaoming";
    user["password"] = "213123";
    //bool ret = ut.insert(user);
    //bool ret = ut.login(user);
    bool ret = ut.select_by_name("xiaoming",user);
    if (ret == false) {
       std::cout << "LOGIN FAILED!" << std::endl;
    }
    std::string body;
    json_util::serialize(user, body);
    std::cout << body << std::endl;
}
void online_test() {
    online_manager om;
    wsserver_t::connection_ptr conn;
    uint64_t uid = 2;
    om.enter_game_room(uid, conn);
    if (om.is_in_game_room(uid)) {
        DLOG("IN GAME HALL");
    }else {
        DLOG("NOT IN GAME HALL");
    }
    om.exit_game_room(uid);
    if (om.is_in_game_room(uid)) {
        DLOG("IN GAME HALL");
    }else {
        DLOG("NOT IN GAME HALL");
    }

}
int main()
{
    //db_test();
    gobang_server _server(HOST, USER, PASS, DBNAME, PORT);
    _server.start(8085);
    return 0;
}