// 在线用户管理：
//     管理的是两类用户：进入游戏大厅的& 进入游戏房间的
//     原因：进入游戏大厅的用户和进入游戏房间的用户才会建立wobsocketv长链接
//     管理：将用户id和对应的客户端webscoket长连接关联起来！
//     作用：当一个用户发送了消息（实时聊天消息/下棋消息），我们可以找到房间中的其它用户，在在线用户管理模块中，找到这个用户对应的websocket连接，然后将消息发给指定用户。
//     1. 通过用户id找到用户连接，进而实现向指定用户的客户端推送消息，websocket连接失败时，会自动在在线用户管理模具爱中删除自己的信息！
//     2. 可以判断一个用户是否还在用户管理模块中来确认用户是否在线！

#ifndef __M_ONLINE_H__
#define __M_ONLINE_H__
#include "util.hpp"
#include <mutex>
#include <unordered_map>

class online_manager{
   private:
        std::mutex _mutex;
        //用于建立游戏大厅用户的用户ID与通信连接的关系
        std::unordered_map<uint64_t,  wsserver_t::connection_ptr>  _hall_user;
        //用于建立游戏房间用户的用户ID与通信连接的关系
        std::unordered_map<uint64_t,  wsserver_t::connection_ptr>  _room_user;
   public:
        //websocket连接建立的时候才会加入游戏大厅&游戏房间在线用户管理
        void enter_game_hall(uint64_t uid,   wsserver_t::connection_ptr &conn) {
            std::unique_lock<std::mutex> lock(_mutex);
            _hall_user.insert(std::make_pair(uid, conn));
        }
        void enter_game_room(uint64_t uid,   wsserver_t::connection_ptr &conn) {
            std::unique_lock<std::mutex> lock(_mutex);
            _room_user.insert(std::make_pair(uid, conn));
        }
        //websocket连接断开的时候，才会移除游戏大厅&游戏房间在线用户管理
        void exit_game_hall(uint64_t uid) {
            std::unique_lock<std::mutex> lock(_mutex);
            _hall_user.erase(uid);
        }
        void exit_game_room(uint64_t uid) {
            std::unique_lock<std::mutex> lock(_mutex);
            _room_user.erase(uid);
        }
        //判断当前指定用户是否在游戏大厅/游戏房间
        bool is_in_game_hall(uint64_t uid) {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _hall_user.find(uid);
            if (it == _hall_user.end()) {
                return false;
            }
            return true;
        }
        bool is_in_game_room(uint64_t uid) {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _room_user.find(uid);
            if (it == _room_user.end()) {
                return false;
            }
            return true;
        }
        //通过用户ID在游戏大厅/游戏房间用户管理中获取对应的通信连接
        wsserver_t::connection_ptr get_conn_from_hall(uint64_t uid) {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _hall_user.find(uid);
            if (it == _hall_user.end()) {
                return wsserver_t::connection_ptr();
            }
            return it->second;
        }
        wsserver_t::connection_ptr get_conn_from_room(uint64_t uid) {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _room_user.find(uid);
            if (it == _room_user.end()) {
                return wsserver_t::connection_ptr();
            }
            return it->second;
        }
};

#endif