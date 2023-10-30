// session模块：保存客户端的用户状态信息
// 1. 有自己的表示：ssid
// 2. 用户的状态信息

// session管理：
// 1. 创建session
// 2. 为session设置过期时间
// 3. 获取session
// 4. 销毁session(过期被自动销毁)

// websocket库中定时器的使用！
// 定时任务取消会导致定时任务立刻被执行！

#ifndef __M_SS_H__
#define __M_SS_H__
#include "util.hpp"
#include <unordered_map>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef enum {UNLOGIN, LOGIN} ss_statu;
class session {
    private:
        uint64_t _ssid; //标识符
        uint64_t _uid; //session对应的用户ID
        ss_statu _statu; //用户状态：未登录，已登录
        wsserver_t::timer_ptr _tp; //session关联的定时器
    public:
        session(uint64_t ssid): _ssid(ssid) { 
            DLOG("SESSION %p 被创建！！", this); 
        }
        ~session() { 
            DLOG("SESSION %p 被释放！！", this); 
        }
        uint64_t ssid() { 
            return _ssid; 
        }
        void set_statu(ss_statu statu) { 
            _statu = statu; 
        }
        void set_user(uint64_t uid) { 
            _uid = uid; 
        }
        uint64_t get_user() { 
            return _uid;
        }
        bool is_login() { 
            return (_statu == LOGIN); 
        }
        void set_timer(const wsserver_t::timer_ptr &tp) {
             _tp = tp;
        }
        wsserver_t::timer_ptr& get_timer() { 
            return _tp; 
        }
};

#define SESSION_TIMEOUT 30000
#define SESSION_FOREVER -1
using session_ptr = std::shared_ptr<session>;
// 管理的数据
// 1.ssid的计数器
// 2.互斥锁
// 3.管理器 unordered_map<ssid,session_ptr>
// 4.websocket 的 server 对象 —— 用于添加定时任务
class session_manager {
    private:
        uint64_t _next_ssid;
        std::mutex _mutex;
        std::unordered_map<uint64_t, session_ptr> _session;
        wsserver_t *_server;
    public:
        session_manager(wsserver_t *srv): _next_ssid(1), _server(srv){
            DLOG("session管理器初始化完毕！");
        }
        ~session_manager() { DLOG("session管理器即将销毁！"); }
        // 创建一个session
        session_ptr create_session(uint64_t uid, ss_statu statu) {
            std::unique_lock<std::mutex> lock(_mutex);
            session_ptr ssp(new session(_next_ssid));
            ssp->set_statu(statu);
            ssp->set_user(uid);
            _session.insert(std::make_pair(_next_ssid, ssp));
            _next_ssid++;
            return ssp;
        }

        // 添加一个已有的session
        void append_session(const session_ptr &ssp) {
            std::unique_lock<std::mutex> lock(_mutex);
            _session.insert(std::make_pair(ssp->ssid(), ssp));
        }
        // 获取一个session
        session_ptr get_session_by_ssid(uint64_t ssid) {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _session.find(ssid);
            if (it == _session.end()) {
                return session_ptr();
            }
            return it->second;
        }
        void remove_session(uint64_t ssid) {
            std::unique_lock<std::mutex> lock(_mutex);
             _session.erase(ssid);
        }
        // 定时删除任务
        void set_session_expire_time(uint64_t ssid, int ms) {
            // 依赖于websocketpp的定时器来完成session生命周期的管理。
            // 登录之后，创建session，session需要在指定时间无通信后删除
            // 但是进入游戏大厅，或者游戏房间，这个session就应该永久存在
            // 等到退出游戏大厅，或者游戏房间，这个session应该被重新设置为临时，在长时间无通信后被删除

            // 在http通信的时候（登录，注册） session 应该具备生命周期，指定时间无通信后删除！
            // 在客户端建立websocket长链结构，session应该是永久存在的！
            // 登录之后，创建session session需要在指定时间后无通信就删除
            // 但是在进入游戏大厅，或者游戏房间，这个session就应该永久存在
            // 但是在推出游戏大厅或者游戏房间，这个session就应该被重新设置为临时的，在长时间无通信后删除！
            session_ptr ssp = get_session_by_ssid(ssid);
            if (ssp.get() == nullptr) {
                return;
            }
            wsserver_t::timer_ptr tp = ssp->get_timer();
    
             // 1. 在session永久存在的情况下，设置永久存在
            if (tp.get() == nullptr && ms == SESSION_FOREVER) {
                // 1. 在session永久存在的情况下，设置永久存在
                return ;
             // 2. 在session永久存在的情况下，设置指定时间之后被删除的定时任务
            }
            else if (tp.get() == nullptr && ms != SESSION_FOREVER) {
                 wsserver_t::timer_ptr tmp_tp = _server->set_timer(ms, 
                    std::bind(&session_manager::remove_session, this, ssid));
                ssp->set_timer(tmp_tp);
            }
             // 3. 在session设置了定时删除的情况下，将session设置为永久存在
            else if (tp.get() != nullptr && ms == SESSION_FOREVER) {
                // 删除定时任务--- stready_timer删除定时任务会导致任务直接被执行
                tp->cancel();//因为这个取消定时任务并不是立即取消的
                //因此重新给session管理器中，添加一个session信息, 且添加的时候需要使用定时器，而不是立即添加
                ssp->set_timer(wsserver_t::timer_ptr());//将session关联的定时器设置为空
                // 可能在添加之后又被删除
                _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp));
            }
             // 4. 在session设置了定时删除的情况下，将session重置删除时间
            else {
                tp->cancel();//因为这个取消定时任务并不是立即取消的
                ssp->set_timer(wsserver_t::timer_ptr());
                _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp));
                
                 //重新给session添加定时销毁任务
                wsserver_t::timer_ptr tmp_tp  = _server->set_timer(ms,std::bind(&session_manager::remove_session, this, ssp->ssid()));
                //重新设置session关联的定时器
                ssp->set_timer(tmp_tp);
            }
        }
};

#endif