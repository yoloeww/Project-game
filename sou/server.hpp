// 服务器的整合实现：
// 1. 网络通信接口的设计
//     收到一个什么格式的数据，代表什么样的请求，应该给予什么样的业务处理及响应。
// 2. 开始搭建服务器
//     1. 搭建websocket服务器，实现网络通信。
//     2. 针对各种不同的请求进行不同的业务处理。
// 客户端存在的请求：
// 1. 客户端从服务器获取一个注册页面 - 静态页面请求
// 2. 客户端给服务器发送一个注册请求（提交用户名以及密码）—— 动态功能请求
// 3. 客户端从服务器获取一个登陆页面 - 静态页面请求
// 4. 客户端给服务器发送一个登录请求（提交用户名以及密码） —— 动态功能请求
// 5. 客户端从服务器获取一个游戏大厅页面 - 静态页面请求
// 6. 客户端给服务器发送了一个获取个人信息的请求（展现个人信息）—— 动态功能请求

// websocket
// 7. 客户端给服务器发送了一个切换websocket协议通信的请求（建立游戏大厅长连接）
// 8. 客户端给服务器发送对战匹配请求
// 9. 客户端给服务器发送停止匹配请求 
// 10. 对战匹配成功，客户端从服务器获取一个游戏房间页面 - 静态页面请求
// 11. 客户端给服务器发送了一个切换websocket协议通信的请求（建立游戏房间长连接）
// 12. 客户端给服务器发送下棋请求
// 13. 客户端给服务器发送聊天请求
// 14. 游戏结束，返回游戏大厅，客户端给服务器发送一个获取游戏大厅页面的请求 -> 5


// 1. 静态资源页面，在后台服务器上是个html/css/js 文件
// 静态资源请求的处理，其实就是将文件中的内容发给客户端
// 1. 注册页面请求
// 请求：GET /register.htm HTTP/1.1
// 响应：
// HTTP/1.1 200 OK 
// Content - Length : xxx
// Content - Type : text/html
// register.html 文件中的内容数据

// 2. 登陆页面请求
// 请求：GET /login.htm HTTP/1.1

// 3. 大厅页面请求

// 请求：GET /game_hall.htm HTTP/1.1

// 4. 房间页面请求
// 请求：GET /game_room.htm HTTP/1.1

// 动态功能请求：
// post /reg HTTP/1.1
   
#ifndef __M_SRV_H__
#define __M_SRV_H__
#include "db.hpp"
#include "matcher.hpp"
#include "online.hpp"
#include "room.hpp"
#include "session.hpp"
#include "util.hpp"

#define WWWROOT "./wwwroot/"
class gobang_server{
    private:
        std::string _web_root;//静态资源根目录 ./wwwroot/      /register.html ->  ./wwwroot/register.html
        wsserver_t _wssrv;
        user_table _ut;
        online_manager _om;
        room_manager _rm;
        matcher _mm;
        session_manager _sm;
    private:
        void file_handler(wsserver_t::connection_ptr &conn) {
            //静态资源请求的处理
            //1. 获取到请求uri-资源路径，了解客户端请求的页面文件名称
            websocketpp::http::parser::request req = conn->get_request();
            std::string uri = req.get_uri();
            //2. 组合出文件的实际路径   相对根目录 + uri
            std::string realpath = _web_root + uri;
            //3. 如果请求的是个目录，增加一个后缀  login.html,    /  ->  /login.html
            if (realpath.back() == '/') {
                realpath += "login.html";
            }
            //4. 读取文件内容
            Json::Value resp_json;
            std::string body;
            bool ret = file_util::read(realpath, body);
            //  1. 文件不存在，读取文件内容失败，返回404
            if (ret == false) {
                body += "<html>";
                body += "<head>";
                body += "<meta charset='UTF-8'/>";
                body += "</head>";
                body += "<body>";
                body += "<h1> Not Found </h1>";
                body += "</body>";
                conn->set_status(websocketpp::http::status_code::not_found);
                conn->set_body(body);
                return;
            }
            //5. 设置响应正文
            conn->set_body(body);
            conn->set_status(websocketpp::http::status_code::ok);
        }
        void http_resp(wsserver_t::connection_ptr &conn, bool result, 
            websocketpp::http::status_code::value code, const std::string &reason) {
            Json::Value resp_json;
            resp_json["result"] = result;
            resp_json["reason"] = reason;
            std::string resp_body;
            json_util::serialize(resp_json, resp_body);
            conn->set_status(code);
            conn->set_body(resp_body);
            conn->append_header("Content-Type", "application/json");
            return;
        }
        void reg(wsserver_t::connection_ptr &conn) {
            //用户注册功能请求的处理
            websocketpp::http::parser::request req = conn->get_request();
            //1. 获取到请求正文
            std::string req_body = conn->get_request_body();
            //2. 对正文进行json反序列化，得到用户名和密码
            Json::Value login_info;
            bool ret = json_util::unserialize(req_body, login_info);
            if (ret == false) {
                DLOG("反序列化注册信息失败");
                return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请求的正文格式错误");
            }
            //3. 进行数据库的用户新增操作
            if (login_info["username"].isNull() || login_info["password"].isNull()) {
                DLOG("用户名密码不完整");
                return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请输入用户名/密码");
            }
            ret = _ut.insert(login_info);
            if (ret == false) {
                DLOG("向数据库插入数据失败");
                return http_resp(conn, false, websocketpp::http::status_code::bad_request, "用户名已经被占用!");
            }
            //  如果成功了，则返回200
            return http_resp(conn, true, websocketpp::http::status_code::ok, "注册用户成功");
        }
        void login(wsserver_t::connection_ptr &conn) {
            //用户登录功能请求的处理
            //1. 获取请求正文，并进行json反序列化，得到用户名和密码
            std::string req_body = conn->get_request_body();
            Json::Value login_info;
            bool ret = json_util::unserialize(req_body, login_info);
            if (ret == false) {
                DLOG("反序列化登录信息失败");
                return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请求的正文格式错误");
            }
            //2. 校验正文完整性，进行数据库的用户信息验证
            if (login_info["username"].isNull() || login_info["password"].isNull()) {
                DLOG("用户名密码不完整");
                return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请输入用户名/密码");
            }
            ret = _ut.login(login_info);
            if (ret == false) {
                //  1. 如果验证失败，则返回400
                DLOG("用户名密码错误");
                return http_resp(conn, false, websocketpp::http::status_code::bad_request, "用户名密码错误");
            }
            //3. 如果验证成功，给客户端创建session
            uint64_t uid = login_info["id"].asUInt64();
            session_ptr ssp = _sm.create_session(uid, LOGIN);
            if (ssp.get() == nullptr) {
                DLOG("创建会话失败");
                return http_resp(conn, false, websocketpp::http::status_code::internal_server_error , "创建会话失败");
            }
            _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
            //4. 设置响应头部：Set-Cookie,将sessionid通过cookie返回
            std::string cookie_ssid = "SSID=" + std::to_string(ssp->ssid());
            conn->append_header("Set-Cookie", cookie_ssid);
            return http_resp(conn, true, websocketpp::http::status_code::ok , "登录成功");
        }
        bool get_cookie_val(const std::string &cookie_str, const std::string &key,  std::string &val) {
            // Cookie: SSID=XXX; path=/; 
            //1. 以 ; 作为间隔，对字符串进行分割，得到各个单个的cookie信息
            std::string sep = "; ";
            std::vector<std::string> cookie_arr;
            string_util::split(cookie_str, sep, cookie_arr);
            for (auto str : cookie_arr) {
                //2. 对单个cookie字符串，以 = 为间隔进行分割，得到key和val
                std::vector<std::string> tmp_arr;
                string_util::split(str, "=", tmp_arr);
                if (tmp_arr.size() != 2) { continue; }
                if (tmp_arr[0] == key) {
                    val = tmp_arr[1];
                    return true;
                }
            }
            return false;
        }
        void info(wsserver_t::connection_ptr &conn) {
            //用户信息获取功能请求的处理
            Json::Value err_resp;
            // 1. 获取请求信息中的Cookie，从Cookie中获取ssid
            std::string cookie_str = conn->get_request_header("Cookie");
            if (cookie_str.empty()) {
                //如果没有cookie，返回错误：没有cookie信息，让客户端重新登录
                return http_resp(conn, true, websocketpp::http::status_code::bad_request, "找不到cookie信息，请重新登录");
            }
            // 1.5. 从cookie中取出ssid
            std::string ssid_str;
            bool ret = get_cookie_val(cookie_str, "SSID", ssid_str);
            if (ret == false) {
                //cookie中没有ssid，返回错误：没有ssid信息，让客户端重新登录
                return http_resp(conn, true, websocketpp::http::status_code::bad_request, "找不到ssid信息，请重新登录");
            }
            // 2. 在session管理中查找对应的会话信息
            session_ptr ssp = _sm.get_session_by_ssid(std::stol(ssid_str));
            if (ssp.get() == nullptr) {
                //没有找到session，则认为登录已经过期，需要重新登录
                return http_resp(conn, true, websocketpp::http::status_code::bad_request, "登录过期，请重新登录");
            }
            // 3. 从数据库中取出用户信息，进行序列化发送给客户端
            uint64_t uid = ssp->get_user();
            Json::Value user_info;
            ret = _ut.select_by_id(uid, user_info);
            if (ret == false) {
                //获取用户信息失败，返回错误：找不到用户信息
                return http_resp(conn, true, websocketpp::http::status_code::bad_request, "找不到用户信息，请重新登录");
            }
            std::string body;
            json_util::serialize(user_info, body);
            conn->set_body(body);
            conn->append_header("Content-Type", "application/json");
            conn->set_status(websocketpp::http::status_code::ok);
            // 4. 刷新session的过期时间
            _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
        }
        void http_callback(websocketpp::connection_hdl hdl) {
            wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
            websocketpp::http::parser::request req = conn->get_request();
            std::string method = req.get_method();
            std::string uri = req.get_uri();
            if (method == "POST" && uri == "/reg") {
                return reg(conn);
            }else if (method == "POST" && uri == "/login") {
                return login(conn);
            }else if (method == "GET" && uri == "/info") {
                return info(conn);
            }else {
                return file_handler(conn);
            }
        }
        void ws_resp(wsserver_t::connection_ptr conn, Json::Value &resp) {
            std::string body;
            json_util::serialize(resp, body);
            conn->send(body);
        }
        session_ptr get_session_by_cookie(wsserver_t::connection_ptr conn) {
            Json::Value err_resp;
            // 1. 获取请求信息中的Cookie，从Cookie中获取ssid
            std::string cookie_str = conn->get_request_header("Cookie");
            if (cookie_str.empty()) {
                //如果没有cookie，返回错误：没有cookie信息，让客户端重新登录
                err_resp["optype"] = "hall_ready";
                err_resp["reason"] = "没有找到cookie信息，需要重新登录";
                err_resp["result"] = false;
                ws_resp(conn, err_resp);
                return session_ptr();
            }
            // 1.5. 从cookie中取出ssid
            std::string ssid_str;
            bool ret = get_cookie_val(cookie_str, "SSID", ssid_str);
            if (ret == false) {
                //cookie中没有ssid，返回错误：没有ssid信息，让客户端重新登录
                err_resp["optype"] = "hall_ready";
                err_resp["reason"] = "没有找到SSID信息，需要重新登录";
                err_resp["result"] = false;
                ws_resp(conn, err_resp);
                return session_ptr();
            }
            // 2. 在session管理中查找对应的会话信息
            session_ptr ssp = _sm.get_session_by_ssid(std::stol(ssid_str));
            if (ssp.get() == nullptr) {
                //没有找到session，则认为登录已经过期，需要重新登录
                err_resp["optype"] = "hall_ready";
                err_resp["reason"] = "没有找到session信息，需要重新登录";
                err_resp["result"] = false;
                ws_resp(conn, err_resp);
                return session_ptr();
            }
            return ssp;
        }
        void wsopen_game_hall(wsserver_t::connection_ptr conn) {
            //游戏大厅长连接建立成功
            Json::Value resp_json;
            //1. 登录验证--判断当前客户端是否已经成功登录
            session_ptr ssp = get_session_by_cookie(conn);
            if (ssp.get() == nullptr) {
                return;
            }
            //2. 判断当前客户端是否是重复登录
            if (_om.is_in_game_hall(ssp->get_user()) || _om.is_in_game_room(ssp->get_user())) {
                resp_json["optype"] = "hall_ready";
                resp_json["reason"] = "玩家重复登录！";
                resp_json["result"] = false;
                return ws_resp(conn, resp_json);
            }
            //3. 将当前客户端以及连接加入到游戏大厅
            _om.enter_game_hall(ssp->get_user(), conn);
            //4. 给客户端响应游戏大厅连接建立成功
            resp_json["optype"] = "hall_ready";
            resp_json["result"] = true;
            ws_resp(conn, resp_json);
            //5. 记得将session设置为永久存在
            _sm.set_session_expire_time(ssp->ssid(), SESSION_FOREVER);
        }
        void wsopen_game_room(wsserver_t::connection_ptr conn) {
            Json::Value resp_json;
            //1. 获取当前客户端的session
            session_ptr ssp = get_session_by_cookie(conn);
            if (ssp.get() == nullptr) {
                return;
            }
            //2. 当前用户是否已经在在线用户管理的游戏房间或者游戏大厅中---在线用户管理
            if (_om.is_in_game_hall(ssp->get_user()) || _om.is_in_game_room(ssp->get_user())) {
                resp_json["optype"] = "room_ready";
                resp_json["reason"] = "玩家重复登录！";
                resp_json["result"] = false;
                return ws_resp(conn, resp_json);
            }
            //3. 判断当前用户是否已经创建好了房间 --- 房间管理
            room_ptr rp = _rm.get_room_by_uid(ssp->get_user());
            if (rp.get() == nullptr) {
                resp_json["optype"] = "room_ready";
                resp_json["reason"] = "没有找到玩家的房间信息";
                resp_json["result"] = false;
                return ws_resp(conn, resp_json);
            }
            //4. 将当前用户添加到在线用户管理的游戏房间中
            _om.enter_game_room(ssp->get_user(), conn);
            //5. 将session重新设置为永久存在
            _sm.set_session_expire_time(ssp->ssid(), SESSION_FOREVER);
            //6. 回复房间准备完毕
            resp_json["optype"] = "room_ready";
            resp_json["result"] = true;
            resp_json["room_id"] = (Json::UInt64)rp->id();
            resp_json["uid"] = (Json::UInt64)ssp->get_user();
            resp_json["white_id"] = (Json::UInt64)rp->get_white_user();
            resp_json["black_id"] = (Json::UInt64)rp->get_black_user();
            return ws_resp(conn, resp_json);
        }
        void wsopen_callback(websocketpp::connection_hdl hdl) {
            //websocket长连接建立成功之后的处理函数
            wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
            websocketpp::http::parser::request req = conn->get_request();
            std::string uri = req.get_uri();
            if (uri == "/hall") {
                //建立了游戏大厅的长连接
                return wsopen_game_hall(conn);
            }else if (uri == "/room") {
                //建立了游戏房间的长连接
                return wsopen_game_room(conn);
            }
        }
        void wsclose_game_hall(wsserver_t::connection_ptr conn) {
            //游戏大厅长连接断开的处理
            //1. 登录验证--判断当前客户端是否已经成功登录
            session_ptr ssp = get_session_by_cookie(conn);
            if (ssp.get() == nullptr) {
                return;
            }
            //1. 将玩家从游戏大厅中移除
            _om.exit_game_hall(ssp->get_user());
            //2. 将session恢复生命周期的管理，设置定时销毁
            _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
        }
        void wsclose_game_room(wsserver_t::connection_ptr conn) {
            //获取会话信息，识别客户端
            session_ptr ssp = get_session_by_cookie(conn);
            if (ssp.get() == nullptr) {
                return;
            }
            //1. 将玩家从在线用户管理中移除
            _om.exit_game_room(ssp->get_user());
            //2. 将session回复生命周期的管理，设置定时销毁
            _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
            //3. 将玩家从游戏房间中移除，房间中所有用户退出了就会销毁房间
            _rm.remove_room_user(ssp->get_user());
        }
        void wsclose_callback(websocketpp::connection_hdl hdl) {
            //websocket连接断开前的处理
            wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
            websocketpp::http::parser::request req = conn->get_request();
            std::string uri = req.get_uri();
            if (uri == "/hall") {
                //建立了游戏大厅的长连接
                return wsclose_game_hall(conn);
            }else if (uri == "/room") {
                //建立了游戏房间的长连接
                return wsclose_game_room(conn);
            }
        }
        void wsmsg_game_hall(wsserver_t::connection_ptr conn, wsserver_t::message_ptr msg) {
            Json::Value resp_json;
            std::string resp_body;
            //1. 身份验证，当前客户端到底是哪个玩家
            session_ptr ssp = get_session_by_cookie(conn);
            if (ssp.get() == nullptr) {
                return;
            }
            //2. 获取请求信息
            std::string req_body = msg->get_payload();
            Json::Value req_json;
            bool ret = json_util::unserialize(req_body, req_json);
            if (ret == false) {
                resp_json["result"] = false;
                resp_json["reason"] = "请求信息解析失败";
                return ws_resp(conn, resp_json);
            }
            //3. 对于请求进行处理：
            if (!req_json["optype"].isNull() && req_json["optype"].asString() == "match_start"){
                //  开始对战匹配：通过匹配模块，将用户添加到匹配队列中
                _mm.add(ssp->get_user());
                resp_json["optype"] = "match_start";
                resp_json["result"] = true;
                return ws_resp(conn, resp_json);
            }else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "match_stop") {
                //  停止对战匹配：通过匹配模块，将用户从匹配队列中移除
                _mm.del(ssp->get_user());
                resp_json["optype"] = "match_stop";
                resp_json["result"] = true;
                return ws_resp(conn, resp_json);
            }
            resp_json["optype"] = "unknow";
            resp_json["reason"] = "请求类型未知";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }
        void wsmsg_game_room(wsserver_t::connection_ptr conn, wsserver_t::message_ptr msg) {
            Json::Value resp_json;
            //1. 获取客户端session，识别客户端身份
            session_ptr ssp = get_session_by_cookie(conn);
            if (ssp.get() == nullptr) {
                DLOG("房间-没有找到会话信息");
                return;
            }
            //2. 获取客户端房间信息
            room_ptr rp = _rm.get_room_by_uid(ssp->get_user());
            if (rp.get() == nullptr) {
                resp_json["optype"] = "unknow";
                resp_json["reason"] = "没有找到玩家的房间信息";
                resp_json["result"] = false;
                DLOG("房间-没有找到玩家房间信息");
                return ws_resp(conn, resp_json);
            }
            //3. 对消息进行反序列化
            Json::Value req_json;
            std::string req_body = msg->get_payload();
            bool ret = json_util::unserialize(req_body, req_json);
            if (ret == false) {
                resp_json["optype"] = "unknow";
                resp_json["reason"] = "请求解析失败";
                resp_json["result"] = false;
                DLOG("房间-反序列化请求失败");
                return ws_resp(conn, resp_json);
            }
            DLOG("房间：收到房间请求，开始处理....");
            //4. 通过房间模块进行消息请求的处理
            return rp->handle_request(req_json);
        }
        void wsmsg_callback(websocketpp::connection_hdl hdl, wsserver_t::message_ptr msg) {
            //websocket长连接通信处理
            wsserver_t::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
            websocketpp::http::parser::request req = conn->get_request();
            std::string uri = req.get_uri();
            if (uri == "/hall") {
                //建立了游戏大厅的长连接
                return wsmsg_game_hall(conn, msg);
            }else if (uri == "/room") {
                //建立了游戏房间的长连接
                return wsmsg_game_room(conn, msg);
            }
        }
    public:
        /*进行成员初始化，以及服务器回调函数的设置*/
        gobang_server(const std::string &host,
               const std::string &user,
               const std::string &pass,
               const std::string &dbname,
               uint16_t port = 3306,
               const std::string &wwwroot = WWWROOT):
               _web_root(wwwroot), _ut(host, user, pass, dbname, port),
               _rm(&_ut, &_om), _sm(&_wssrv), _mm(&_rm, &_ut, &_om) {
            _wssrv.set_access_channels(websocketpp::log::alevel::none);
            _wssrv.init_asio();
            _wssrv.set_reuse_addr(true);
            _wssrv.set_http_handler(std::bind(&gobang_server::http_callback, this, std::placeholders::_1));
            _wssrv.set_open_handler(std::bind(&gobang_server::wsopen_callback, this, std::placeholders::_1));
            _wssrv.set_close_handler(std::bind(&gobang_server::wsclose_callback, this, std::placeholders::_1));
            _wssrv.set_message_handler(std::bind(&gobang_server::wsmsg_callback, this, std::placeholders::_1, std::placeholders::_2));
        }
        /*启动服务器*/
        void start(int port) {
            _wssrv.listen(port);
            _wssrv.start_accept();
            _wssrv.run();
        }
};
#endif