#ifndef BB_H

#define BB_H

#include <thread>
#include <mutex>
#include <chrono>
#include <iostream>
#include <condition_variable>
#include "ModelValue.h"
#include <vector>

class BoundedBuffer {
private:
    std::vector<ModelValue> buffer;
    int capacity;

    int front;
    int rear;
    int count;

    std::mutex lock;

    std::condition_variable not_full;
    std::condition_variable not_empty;
public:

    BoundedBuffer(int capacity) : capacity(capacity), front(0), rear(0), count(0) {        
        buffer.resize(capacity);
    }

    ~BoundedBuffer(){        
    }

    void put(const ModelValue &data){
        std::unique_lock<std::mutex> l(lock);

        not_full.wait(l, [this](){return count != capacity; });

        buffer[rear] = data;
        rear = (rear + 1) % capacity;
        ++count;

        l.unlock();
        not_empty.notify_one();
    }

    ModelValue take(){
        std::unique_lock<std::mutex> l(lock);

        not_empty.wait(l, [this](){return count != 0; });

        ModelValue result = buffer[front];
        front = (front + 1) % capacity;
        --count;

        l.unlock();
        not_full.notify_one();

        return result;
    }
};






#endif