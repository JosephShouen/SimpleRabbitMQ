#ifndef RMQ_LIB_V_H
#define RMQ_LIB_V_H
#include <string>
#include <vector>
#include <thread>

struct GottenMessage
{
    std::vector<char> gotten_messages; //текст сообщения
    std::string sender_person; //отправитель
};

class SimpleRabbitMQ
{
public:  
    SimpleRabbitMQ(const std::string queue) : queue(queue) {}
    virtual ~SimpleRabbitMQ() {}
    
    virtual void run();
    virtual void send_message(const std::string &queue_to, const std::string &host, const std::string &message);
    int get_count_message() const;
    std::vector<GottenMessage> get_message(int i) const;
    
protected:
    std::string queue; //очередь сервера
    std::thread listen; //поток для прослушки
    std::vector<GottenMessage> messages;
    std::string queue_name;
    std::string message_text;
    virtual void server();
};

#endif
