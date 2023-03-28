#include <iostream>
#include <thread>
#include <amqpcpp.h>
#include "conn_handler.h"
#include <sstream>
#include <unistd.h>
#include <string>
#include <fstream>
#include <cstdio>
#include "rmq_ve.h"

struct GottenMessages
{
    std::string sender_person;
    std::string os_name;
    int os_version;
    int kernel_version;
};

class LittleRabbit: public SimpleRabbitMQ
{
public:
        LittleRabbit(const std::string queue,
                     void (callback_two)(const std::string &queue_name_cb, const std::string &os_name_cb, const std::string &os_version_cb, const std::string &kernel_version_cb)) :SimpleRabbitMQ(queue)
        {
            queueT = queue; 
            callbackT = callback_two;
        }
        
        int get_count_messages() const
        {
            return messages.size();
        }
        
        std::vector<GottenMessages> get_messages(int i) const
        {
            if ( i<messages.size()) 
            {
                return messages;
            }
        }
        
        void send_message(const std::string &queue_to, const std::string &host, const std::string &message) override
        {
            auto evbase = event_base_new();
            LibEventHandlerMyError hndl(evbase);

            AMQP::TcpConnection connection(&hndl, AMQP::Address(host, 5672, AMQP::Login("admin", "admin"), "/"));
            AMQP::TcpChannel channel(&connection);
    
            channel.onError([&evbase](const char* message)
            {
                std::cout << "Channel error: " << message << std::endl;
                event_base_loopbreak(evbase);
            });
    
            channel.declareQueue(queue_to, AMQP::passive)
                .onSuccess([&connection](const std::string &name, uint32_t messagecount, uint32_t consumercount){})
                .onFinalize([&connection](){connection.close();});
        
            channel.publish("", queue_to, queue + " " + message);
            event_base_dispatch(evbase);
            event_base_free(evbase);
            return;
        }
        
protected:
        void (*callbackT)(const std::string &queue_name_cb, const std::string &os_name_cb, const std::string &os_version_cb, const std::string &kernel_version_cb);
        std::string queueT;
        std::thread listen;
        std::string queue_name;
        std::string os_name;
        std::string os_version;
        std::string kernel_version;
        std::vector<GottenMessages> messages;
        
        void server() override
        {
            ConnHandler handler;
            AMQP::TcpConnection connection(handler, AMQP::Address("localhost", 5672, AMQP::Login("admin", "admin"), "/"));
            AMQP::TcpChannel channel(&connection);

            channel.onError([&handler](const char* message)
            {
                std::cout << "Channel error: " << message << std::endl;
                handler.Stop();
            });    
    
            channel.declareQueue(queue, AMQP::autodelete)
                .onSuccess([&connection](const std::string &name, uint32_t messagecount, uint32_t consumercount) {}); 
    
            channel.consume(queue, AMQP::noack).onReceived([&](const AMQP::Message &msg, uint64_t tag, bool redelivered) mutable
            {   
                GottenMessages NewMessage;
                messages.push_back(NewMessage);
                std::cout << messages.size() << std::endl;
                queue_name = "";
                os_name = "";
                os_version = "";
                kernel_version = "";
                int check = 0;
                for (int i = 0; i < msg.message().length(); i++)
                {
                    if ((msg.message()[i] == ' '))
                    {
                        check++;
                        continue;
                    } 
                    else if (check == 0)
                    {
                        queue_name = queue_name + msg.message()[i];
                    } 
                    else if (check == 1)
                    {
                        os_name = os_name + msg.message()[i];
                    }
                    else if (check == 2)
                    {
                        os_version = os_version + msg.message()[i];
                    }
                    else
                    {
                        kernel_version = kernel_version + msg.message()[i];
                    }
                }
                messages.at(messages.size()-1).sender_person = queue_name;
                messages.at(messages.size()-1).os_name = os_name;
                messages.at(messages.size()-1).os_version = std::stoi(os_version);
                messages.at(messages.size()-1).kernel_version = std::stoi(kernel_version);
                callbackT(queue_name, os_name, os_version, kernel_version);
            });    
            handler.Start();
            connection.close();
            return;
        }
        
};


int main(int argc, char* argv[])
{       
    std::string queue_from;
    std::cout << "Your queue name: ";
    std::cin >> queue_from; 
    std::string os_name;
    std::string os_version;
    std::string kernel_version;
    LittleRabbit littlerabbit(queue_from, 
                                 [](const std::string &queue_name_cb, const std::string &os_name_cb, const std::string &os_version_cb, const std::string &kernel_version_cb)
                                 {
                                    std::cout << "You have a message from " << queue_name_cb << ", OS name: " << os_name_cb 
                                    << ", OS Version: " << os_version_cb << ", Kernel Version: " << kernel_version_cb << std::endl;
                                 }
    );
    littlerabbit.run();
    std::string queue_to;
    std::string host = "localhost";
    int size;
    char choice_c;
    int choice_i;
    
    for(;;) 
    {
        std::cout << "r - read message, s - send message, q - exit" << std::endl;
        std::cin >> choice_c;        
        if (choice_c == 's') 
        {
            std::cout << "To whom do you want to send this message?" << std::endl;
            std::cout << "Name: ";
            std::getline(std::cin >> std::ws, queue_to);
            std::cout << "OS: " << std::endl;
            std::getline(std::cin >> std::ws, os_name);
            std::cout << "OS version: " << std::endl;
            std::getline(std::cin >> std::ws, os_version);
            std::cout << "Kernel version: " << std::endl;
            std::getline(std::cin >> std::ws, kernel_version);
            littlerabbit.send_message(queue_to, host, os_name + " " + os_version + " " + kernel_version);
        } 
        else if (choice_c == 'r') 
        {
            size = littlerabbit.get_count_messages();
            std::cout << "You have " << size << " message(s)!" << std::endl;
            std::cout << "Which message do you want to get?" << std::endl;
            std::cin >> choice_i;
            choice_i--;
            if (choice_i < size)
            {
                std::vector<GottenMessages> my_message = littlerabbit.get_messages(choice_i);
                std::cout << "From: " << my_message[choice_i].sender_person << std::endl;
                std::cout << "OS name: " << my_message[choice_i].os_name << std::endl;
                std::cout << "OS Version:" << my_message[choice_i].os_version << std::endl;
                std::cout << "Kernel Version: " << my_message[choice_i].kernel_version << std::endl;
            } 
            else {std::cout << "This message doesn't exist!" << std::endl;}
        }
        else if (choice_c == 'q') 
        {
            break;
        } 
        else 
        {
            std::cout << "Wrong answer!" << std::endl;
        }
    }
}
