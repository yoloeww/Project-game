// 匹配对战：
// 1. 将所有玩家，根据得分，分为三个档次！
// score < 2000;
// score >= 2000 && score < 3000;
// score >= 3000;
// 2. 为三个不同档次创建三个不同的匹配队列！
// 3. 如果有玩家想要进行对战匹配，根据玩家分数，将玩家的id，加到指定的队列中！
// 4. 当一个队列中元素数量 >= 2,则表示有两个玩家要进行匹配，匹配成功
// 5. 出对队列中的前两个元素，为这两个玩家创建房间！
// 6. 给匹配成功的玩家，发送匹配响应，对战匹配成功！

// 设计：
// 1. 设计一个匹配队列 —— 阻塞队列
// 2. 匹配管理
//    因为匹配队列有三个，因此创建三个线程，阻塞等待指定队列中的玩家数量>=2;


// 设计一个阻塞队列：（目的是为了实现玩家匹配队列）
// 功能：
// 1. 入队数据
// 2. 出队数据
// 3. 移除指定的数据
// 4. 线程安全
// 5. 获取队列元素个数
// 6. 阻塞接
// 7. 判断队列是否为空
#include "util.hpp"
#include "online.hpp"
#include "db.hpp"
#include "room.hpp"
#include <list>
#include <mutex>
#include <condition_variable>

#ifndef __M_MATCHER_H__
#define __M_MATCHER_H__
template <class T>
class match_queue {
private:
    /*用链表而不直接使用queue是因为我们有中间删除数据的需要*/
    std::list<T> _list;
    /*实现线程安全*/
    std::mutex _mutex;
    /*这个条件变量主要为了阻塞消费者，后边使用的时候：队列中元素个数<2则阻塞*/
    std::condition_variable _cond;
public:
   /*获取元素个数*/
        int size() {  
            std::unique_lock<std::mutex> lock(_mutex);
            return _list.size(); 
        }
        /*判断是否为空*/
        bool empty() {
            std::unique_lock<std::mutex> lock(_mutex);
            return _list.empty();
        }
        /*阻塞线程*/
        void wait() {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock);
        }
        /*入队数据，并唤醒线程*/
        void push(const T &data) {
            std::unique_lock<std::mutex> lock(_mutex);
            _list.push_back(data);
            _cond.notify_all();
        }
        /*出队数据*/
        bool pop(T &data) {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_list.empty() == true) {
                return false;
            }
            data = _list.front();
            _list.pop_front();
            return true;
        }
        /*移除指定的数据*/
        void remove(T &data) {
            std::unique_lock<std::mutex> lock(_mutex);
            _list.remove(data);
        }
};

// 匹配管理：
// 1. 三个不同档次的队列
// 2. 三个线程分别对三个队列中的玩家进行匹配
// 3. 房间管理模块的句柄
// 4. 在线用户管理模块的句柄
// 5. 数据管理模块——用户表的句柄
// 功能：
// 1. 添加用户到匹配队列
// 2. 线程入口函数
//     判断指定队列是否热人数大于二
//     出对队列中的前两个元素
//     创建房间，将两个用户信息添加到房间中
//     向两个玩家发送对战匹配成功的消息！
// 3. 从匹配队列移除用户

class matcher {
    private:
        /*普通选手匹配队列*/
        match_queue<uint64_t> _q_normal;
        /*高手匹配队列*/
        match_queue<uint64_t> _q_high;
        /*大神匹配队列*/
        match_queue<uint64_t> _q_super;
        /*对应三个匹配队列的处理线程*/
        std::thread _th_normal;
        std::thread _th_high;
        std::thread _th_super;

        room_manager *_rm;
        user_table *_ut;
        online_manager *_om;
    private:
        void handle_match(match_queue<uint64_t> &mq) {
            while(1) {
                //1. 判断队列人数是否大于2，<2则阻塞等待
                while (mq.size() < 2) {
                    mq.wait();
                }    
                //2. 走下来代表人数够了，出队两个玩家
                uint64_t uid1, uid2;
                bool ret = mq.pop(uid1);
                if (ret == false) { 
                    continue; 
                }
                ret = mq.pop(uid2);
                if (ret == false) { 
                    this->add(uid1); 
                    continue; 
                }
                //3. 校验两个玩家是否在线，如果有人掉线，则要吧另一个人重新添加入队列
                wsserver_t::connection_ptr conn1 = _om->get_conn_from_hall(uid1);
                if (conn1.get() == nullptr) {
                    this->add(uid2); 
                    continue;
                }
                wsserver_t::connection_ptr conn2 = _om->get_conn_from_hall(uid2);
                if (conn2.get() == nullptr) {
                    this->add(uid1); 
                    continue;
                }
                //4. 为两个玩家创建房间，并将玩家加入房间中
                room_ptr rp = _rm->create_room(uid1, uid2);
                if (rp.get() == nullptr) {
                    this->add(uid1);
                    this->add(uid2);
                    continue;
                }
                //5. 对两个玩家进行响应
                Json::Value resp;
                resp["optype"] = "match_success";
                resp["result"] = true;
                std::string body;
                json_util::serialize(resp, body);
                conn1->send(body);
                conn2->send(body);
            }
        }
         void th_normal_entry() { 
            return handle_match(_q_normal); 
        }
        void th_high_entry() { 
            return handle_match(_q_high); 
        }
        void th_super_entry() { 
            return handle_match(_q_super); 
        }
    public:
       matcher(room_manager *rm, user_table *ut, online_manager *om): 
            _rm(rm), _ut(ut), _om(om),
            _th_normal(std::thread(&matcher::th_normal_entry, this)),
            _th_high(std::thread(&matcher::th_high_entry, this)),
            _th_super(std::thread(&matcher::th_super_entry, this)){
            DLOG("游戏匹配模块初始化完毕....");
        }
        bool add(uint64_t uid) {
            //根据玩家的天梯分数，来判定玩家档次，添加到不同的匹配队列
            // 1. 根据用户ID，获取玩家信息
            Json::Value user;
            bool ret = _ut->select_by_id(uid, user);
            if (ret == false) {
                DLOG("获取玩家:%d 信息失败！！", uid);
                return false;
            }
            int score = user["score"].asInt();
            // 2. 添加到指定的队列中
            if (score < 2000) {
                _q_normal.push(uid);
            }else if (score >= 2000 && score < 3000) {
                _q_high.push(uid);
            }else {
                _q_super.push(uid);
            }
            return true;
        }
        bool del(uint64_t uid) {
            Json::Value user;
            bool ret = _ut->select_by_id(uid, user);
            if (ret == false) {
                DLOG("获取玩家:%d 信息失败！！", uid);
                return false;
            }
            int score = user["score"].asInt();
            // 2. 添加到指定的队列中
            if (score < 2000) {
                _q_normal.remove(uid);
            }else if (score >= 2000 && score < 3000) {
                _q_high.remove(uid);
            }else {
                _q_super.remove(uid);
            }
            return true;
        }
};
#endif