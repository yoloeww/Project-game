// 游戏房间管理模块：
// 1. 对匹配成功的玩家创建房间，建立起一个小范围的玩家之间的关联关系！
//    房间里一个玩家产生的动作将会广播给房间里的其他用户！
// 2. 因为房间里可能会有很多，所以要将这些房间管理起来，以便于进行房间生命周期的控制！
// 实现两个部分：
// 1. 房间的设计
// 2. 房间管理的设置
// 游戏房间的设计：
// 管理的数据,处理房间中产生的动作
// 1. 房间的ID
// 2. 房间的状态（决定了一个玩家退出房间时所作的动作）
// 3. 房间中玩家的数量（决定了玩家什么时候销毁）
// 4. 白棋玩家id
// 5. 黑棋玩家id
// 6. 用户信息表的句柄（当玩家胜利/失败的时候更新用户数据）
// 7. 棋盘信息（二维数组）

// 房间中产生的动作：
// 1. 下棋
// 2. 聊天
// 不管是什么动作，只要是合理的，都要广播给房间里的其他用户！
#ifndef __M_ROOM_H__
#define __M_ROOM_H__
#include "util.hpp"
#include "logger.hpp"
#include "online.hpp"
#include "db.hpp"
#define BOARD_ROW 15
#define BOARD_COL 15
#define CHESS_WHITE 1
#define CHESS_BLACK 2
typedef enum { GAME_START, GAME_OVER }room_statu;
class room {
    private:
    // 1. 房间的ID
    // 2. 房间的状态（决定了一个玩家退出房间时所作的动作）
    // 3. 房间中玩家的数量（决定了玩家什么时候销毁）
    // 4. 白棋玩家id
    // 5. 黑棋玩家id
    // 6. 用户信息表的句柄（当玩家胜利/失败的时候更新用户数据）
    // 7. 棋盘信息（二维数组）
        uint64_t _room_id;
        room_statu _statu;
        int _player_count;
        uint64_t _white_id;
        uint64_t _black_id;
        user_table *_tb_user; 
        online_manager *_online_user;
        std::vector<std::vector<int>> _board;
    private: 
        bool five(int row, int col, int row_off, int col_off, int color) {
            //row和col是下棋位置，  row_off和col_off是偏移量，也是方向
            int count = 1;
            int search_row = row + row_off;
            int search_col = col + col_off;
            while(search_row >= 0 && search_row < BOARD_ROW &&
                  search_col >= 0 && search_col < BOARD_COL &&
                  _board[search_row][search_col] == color) {
                //同色棋子数量++
                count++;
                //检索位置继续向后偏移
                search_row += row_off;
                search_col += col_off;
            }
            search_row = row - row_off;
            search_col = col - col_off;
            while(search_row >= 0 && search_row < BOARD_ROW &&
                  search_col >= 0 && search_col < BOARD_COL &&
                  _board[search_row][search_col] == color) {
                //同色棋子数量++
                count++;
                //检索位置继续向后偏移
                search_row -= row_off;
                search_col -= col_off;
            }
            return (count >= 5);
        }
        uint64_t check_win(int row, int col, int color) {
            // 从下棋位置的四个不同方向上检测是否出现了5个及以上相同颜色的棋子（横行，纵列，正斜，反斜）
            if (five(row, col, 0, 1, color) || 
                five(row, col, 1, 0, color) ||
                five(row, col, -1, 1, color)||
                five(row, col, -1, -1, color)) {
                //任意一个方向上出现了true也就是五星连珠，则设置返回值
                return color == CHESS_WHITE ? _white_id : _black_id;
            }
            return 0;
        }
    public:
        room(uint64_t room_id, user_table *tb_user, online_manager *online_user):
            _room_id(room_id), _statu(GAME_START), _player_count(0),
            _tb_user(tb_user), _online_user(online_user),
            _board(BOARD_ROW, std::vector<int>(BOARD_COL, 0)){
            DLOG("%lu 房间创建成功!!", _room_id);
        }
        ~room() {
            DLOG("%lu 房间销毁成功!!", _room_id);
        }
        uint64_t id() { 
            return _room_id; 
        }
        room_statu statu() { 
            return _statu; 
        }
        int player_count() { 
            return _player_count; 
        }
        void add_white_user(uint64_t uid) { 
            _white_id = uid; _player_count++;
        }
        void add_black_user(uint64_t uid) {
            _black_id = uid; _player_count++; 
        }
        uint64_t get_white_user() { 
            return _white_id; 
        }
        uint64_t get_black_user() { 
            return _black_id; 
        }
        // 处理下棋动作
        Json::Value handle_chess(Json::Value &req) {
            Json::Value json_resp = req;
             // 2. 判断房间中两个玩家是否都在线，任意一个不在线，就是另一方胜利。
            int chess_row = req["row"].asInt();
            int chess_col = req["col"].asInt();
            uint64_t cur_uid = req["uid"].asUInt64();
            if (_online_user -> is_in_game_room(_white_id) == false) {
                json_resp["result"] = true;
                json_resp["reason"] = "运气真好！对方掉线，不战而胜！";
                json_resp["winner"] = (Json::UInt64)_black_id;
                return json_resp;
            }
            if (_online_user->is_in_game_room(_black_id) == false) {
                json_resp["result"] = true;
                json_resp["reason"] = "运气真好！对方掉线，不战而胜！";
                json_resp["winner"] = (Json::UInt64)_white_id;
                return json_resp;
            }
             // 3. 获取走棋位置，判断当前走棋是否合理（位置是否已经被占用）
            if (_board[chess_row][chess_col] != 0) {
                json_resp["result"] = false;
                json_resp["reason"] = "当前位置已经有了其他棋子！";
                return json_resp;
            }
            int cur_color = cur_uid == _white_id ? CHESS_WHITE : CHESS_BLACK;
            _board[chess_row][chess_col] = cur_color;
             // 4. 判断是否有玩家胜利（从当前走棋位置开始判断是否存在五星连珠）
            uint64_t winner_id = check_win(chess_row, chess_col, cur_color);
            if (winner_id != 0) {
                json_resp["reason"] = "五星连珠，您获胜了！";
            }
            json_resp["result"] = true;
            json_resp["winner"] = (Json::UInt64)winner_id;
            return json_resp;
        }
        /*处理聊天动作*/
        Json::Value handle_chat(Json::Value &req) {
            Json::Value json_resp = req;
            // 检查消息中是否包含敏感词
            std::string msg = req["message"].asString();
            size_t pos = msg.find("sb");
            if (pos != std::string::npos) {
                json_resp["result"] = false;
                json_resp["reason"] = "消息中包含敏感词，不能发送！";
                return json_resp;
            }
            //广播消息---返回消息
            json_resp["result"] = true;
            return json_resp;
        }
        /*处理玩家退出房间动作*/
        void handle_exit(uint64_t uid) {
            //如果是下棋中退出，则对方胜利，否则下棋结束了退出，则是正常退出
            Json::Value json_resp;
            if (_statu == GAME_START) {
                uint64_t winner_id = (Json::UInt64)(uid == _white_id ? _black_id : _white_id);
                json_resp["optype"] = "put_chess";
                json_resp["result"] = true;
                json_resp["reason"] = "对方掉线，不战而胜！";
                json_resp["room_id"] = (Json::UInt64)_room_id;
                json_resp["uid"] = (Json::UInt64)uid;
                json_resp["row"] = -1;
                json_resp["col"] = -1;
                json_resp["winner"] = (Json::UInt64)winner_id;
                uint64_t loser_id = winner_id == _white_id ? _black_id : _white_id;
                _tb_user->win(winner_id);
                _tb_user->lose(loser_id);
                _statu = GAME_OVER;
                broadcast(json_resp);
            }
              //房间中玩家数量--
            _player_count--;
            return;
        }
        void handle_request(Json::Value &req) {
            //1. 校验房间号是否匹配
            Json::Value json_resp;
            uint64_t room_id = req["room_id"].asUInt64();
            if (room_id != _room_id) {
                json_resp["optype"] = req["optype"].asString();
                json_resp["result"] = false;
                json_resp["reason"] = "房间号不匹配！";
                return broadcast(json_resp);
            }
            //2. 根据不同的请求类型调用不同的处理函数
             if (req["optype"].asString() == "put_chess") {
                json_resp = handle_chess(req);
                if (json_resp["winner"].asUInt64() != 0) {
                    uint64_t winner_id = json_resp["winner"].asUInt64();
                    uint64_t loser_id = winner_id == _white_id ? _black_id : _white_id;
                    _tb_user->win(winner_id);
                    _tb_user->lose(loser_id);
                    _statu = GAME_OVER;
                }
            }
            else if (req["optype"].asString() == "chat") {
                json_resp = handle_chat(req);
            }
            else {
                json_resp["optype"] = req["optype"].asString();
                json_resp["result"] = false;
                json_resp["reason"] = "未知请求类型";
            }
            std::string body;
            json_util::serialize(json_resp, body);
            DLOG("房间-广播动作: %s", body.c_str());
            return broadcast(json_resp);
        }
        /*将指定的信息广播给房间中所有玩家*/
        void broadcast(Json::Value &rsp) {
            //1. 对要响应的信息进行序列化，将Json::Value中的数据序列化成为json格式字符串
            std::string body;
            json_util::serialize(rsp, body);
            //2. 获取房间中所有用户的通信连接
            //3. 发送响应信息
            wsserver_t::connection_ptr wconn = _online_user->get_conn_from_room(_white_id);
            if (wconn.get() != nullptr) {
                wconn->send(body);
            }
            else {
                DLOG("房间-白棋玩家连接获取失败");
            }
            wsserver_t::connection_ptr bconn = _online_user->get_conn_from_room(_black_id);
            if (bconn.get() != nullptr) {
                bconn->send(body);
            }
            else {
                DLOG("房间-黑棋玩家连接获取失败");
            }
            return;
        }
};

// Restful 风格的网络通信接口设计：
// 房间管理：
// 1. 创建房间 (两个玩家对战匹配完成了，为他们创造一个房间，需要传入两个玩家的用户id)
// 2. 查找房间 (通过房间id查找房间信息，通过用户id查找房间所在信息)
// 3. 销毁房间 (根据房间id销毁房间，房间中所有用户退出了，销毁房间)
// 需要管理的数据：
// 1. 数据管理模块句柄
// 2. 在线用户管理模块句柄
// 3. 房间id分配计数器
// 4. 互斥锁
// using room_ptr = std::shared_ptr<room>; 房间信息的空间使用shared_ptr进行管理，释放了我们还操作，访问错误！
// 5. unordered_map<room_id,room_ptr> 房间信息管理(建立起房间id与房间信息的映射关系)
// 6. unordered_map<room_id,user_id> 房间id与用户id的关联关系管理！ 通过用户id找到所在房间id，再去查找房间信息！
// 7. 房间中所有用户退出了，销毁房间。
using room_ptr = std::shared_ptr<room>;

class room_manager {
private:
    uint64_t _next_rid;
        std::mutex _mutex;
        user_table *_tb_user;
        online_manager *_online_user;
        std::unordered_map<uint64_t, room_ptr> _rooms;
        std::unordered_map<uint64_t, uint64_t> _users;
public:
     /*初始化房间ID计数器*/
        room_manager(user_table *ut, online_manager *om):
                _next_rid(1), _tb_user(ut), _online_user(om) {
                DLOG("房间管理模块初始化完毕！");
    }
        ~room_manager() { 
            DLOG("房间管理模块即将销毁！"); 
    }
    //为两个用户创建房间，并返回房间的智能指针管理对象
        room_ptr create_room(uint64_t uid1, uint64_t uid2) {
        // 两个用户在游戏大厅中进行对战匹配，匹配成功后创建房间
        // 1. 校验两个用户是否都还在游戏大厅中，只有都在才需要创建房间。
            if (_online_user->is_in_game_hall(uid1) == false) {
                DLOG("用户：%lu 不在大厅中，创建房间失败!", uid1);
                return room_ptr();
            }
            if (_online_user->is_in_game_hall(uid2) == false) {
                DLOG("用户：%lu 不在大厅中，创建房间失败!", uid2);
                return room_ptr();
            }
        // 2. 创建房间，将用户信息添加到房间中
            std::unique_lock<std::mutex> lock(_mutex);
            room_ptr rp(new room(_next_rid,_tb_user,_online_user));
            rp->add_white_user(uid1);
            rp->add_black_user(uid2);
        //3. 将房间信息管理起来
            _rooms.insert(std::make_pair(_next_rid, rp));
            _users.insert(std::make_pair(uid1, _next_rid));
            _users.insert(std::make_pair(uid2, _next_rid));
            _next_rid++;
        //4. 返回房间信息
            return rp;
    }
    /*通过房间ID获取房间信息*/
        room_ptr get_room_by_rid(uint64_t rid) {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _rooms.find(rid);
            if (it == _rooms.end()) {
                return room_ptr();
            }
            return it->second;
        }
        /*通过用户ID获取房间信息*/
        room_ptr get_room_by_uid(uint64_t uid) {
            std::unique_lock<std::mutex> lock(_mutex);
            //1. 通过用户ID获取房间ID
            auto uit = _users.find(uid);
            if (uit == _users.end()) {
                return room_ptr();
            }
            uint64_t rid = uit->second;
            //2. 通过房间ID获取房间信息
            auto rit = _rooms.find(rid);
            if (rit == _rooms.end()) {
                return room_ptr();
            }
            return rit->second;
        }
  
     /*通过房间ID销毁房间*/
    void remove_room(uint64_t rid) {
        //因为房间信息，是通过shared_ptr在_rooms中进行管理，因此只要将shared_ptr从_rooms中移除
        //则shared_ptr计数器==0，外界没有对房间信息进行操作保存的情况下就会释放
        //1. 通过房间ID，获取房间信息
        room_ptr rp = get_room_by_rid(rid);
        if (rp.get() == nullptr)
            return ;
        //2. 通过房间信息，获取房间中所有用户的ID
            uint64_t uid1 = rp->get_white_user();
            uint64_t uid2 = rp->get_black_user();
        //3. 移除房间管理中的用户信息
            std::unique_lock<std::mutex> lock(_mutex);
            _users.erase(uid1);
            _users.erase(uid2);
            //4. 移除房间管理信息
            _rooms.erase(rid);
    }
    /*删除房间中指定用户，如果房间中没有用户了，则销毁房间，用户连接断开时被调用*/
    void remove_room_user(uint64_t uid) {
        room_ptr rp = get_room_by_rid(uid);
        if (rp.get() == nullptr)
            return ;
        rp->handle_exit(uid);
        if (rp->player_count() == 0) {
            remove_room(rp->id());
        }
        return ;
    }
};

#endif