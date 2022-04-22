#include <unordered_map>
#include <string>

using namespace std;

//需要的功能
//1. 访问后将该节点移至头部 ---- 登录成功访问
//2. 插入某节点 ---------------- 已有用户并未存在LRU中
//3. 删除尾节点 ---------------- 达到最大存储范围
//4. 删除某个特定节点 ---------- 修改密码 原则：先更新数据库，再删除缓存

template<typename T>
class DlinkNode {
public:
    string _key;
    T _value;
    DlinkNode *prev;
    DlinkNode *next;
    // T() 表示模板类型的默认值
    DlinkNode(): _key(""), _value(T()), prev(nullptr), next(nullptr) {}
    DlinkNode(string key, T value): _key(key), _value(value), prev(nullptr), next(nullptr) {}
};

template<typename T>
class LRUCache {
public:
    LRUCache(int capacity): _capacity(capacity), _size(0) {
        head = new DlinkNode<T>();
        tail = new DlinkNode<T>();
        head->next = tail;
        tail->prev = head;
    }

    int GetSize() const {   return _size;   }
    int GetCapacity() const {   return _capacity;   }

    //获取节点值
    bool GetValue(string key, T &value) {
        if (cache.find(key) == cache.end())
            return false;

        DlinkNode<T> *node = cache[key];
        removeNode(node);
        addtoHead(node);
        value = node->_value;
        return true;
    }

    //添加节点
    bool PutValue(string key, T &value) {
        //与LRU不同，找到已存在值直接返回false
        if (cache.find(key) != cache.end())
            return false;

        DlinkNode<T> *node = new DlinkNode<T>(key, value);
        cache[key] = node;
        addtoHead(node);
        _size++;

        if (_size > _capacity) {
            DlinkNode<T> *removed = removeTail();
            cache.erase(removed->_key);
            delete removed;
            _size--;
        }

        return true;
    }

    //删除节点
    bool DelValue(string key) {
        if (cache.find(key) == cache.end())
            return false;
        
        DlinkNode<T> *removed = cache[key];
        removeNode(removed);
        cache.erase(removed->_key);
        delete removed;
        _size--;

        return true;
    }

private:
    unordered_map<string, DlinkNode<T>* > cache;
    DlinkNode<T> *head;
    DlinkNode<T> *tail;
    int _size;
    int _capacity;

    void removeNode(DlinkNode<T> *node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
        node->prev = nullptr;
        node->next = nullptr;
    }

    void addtoHead(DlinkNode<T> *node) {
        node->prev = head;
        node->next = head->next;
        head->next->prev = node;
        head->next = node;
    }

    DlinkNode<T> *removeTail() {
        DlinkNode<T> *node = tail->prev;
        removeNode(node);
        return node;
    }
};
