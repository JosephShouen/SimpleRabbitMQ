#include <iostream>
#include <thread>
#include <amqpcpp.h>
#include "conn_handler.h"
#include <sstream>
#include <unistd.h>
#include <string>
#include <fstream>
#include <cstdio>
#include "srmq.h"

void SimpleRabbitMQ::server()
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
                GottenMessage NewMessage;
                messages.push_back(NewMessage);
                queue_name = "";
                bool space = false; //чтобы пропускал только первый пробел, отделяющий имя очереди от сообщения
                for (int i = 0; i < msg.message().length(); i++)
                {
                    if ((msg.message()[i] == ' ') && (space == false))
                    {
                        space = true;
                        continue;
                    } 
                    else if (space == false)
                    {
                        queue_name = queue_name + msg.message()[i];
                    } 
                    else 
                    {
                        messages.at(messages.size()-1).gotten_messages.push_back(msg.message()[i]);
                    }
                }
                messages.at(messages.size()-1).sender_person = queue_name;
            }
        );    
    handler.Start();
    connection.close();
    return;
}

void SimpleRabbitMQ::run()
{
    listen = std::thread(&SimpleRabbitMQ::server, this);
    listen.detach();
}

void SimpleRabbitMQ::send_message(const std::string &queue_to, const std::string &host, const std::string &message)
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

int SimpleRabbitMQ::get_count_message() const
{
    return messages.size();
}

std::vector<GottenMessage> SimpleRabbitMQ::get_message(int i) const
{
    if ( i<messages.size()) 
    {
        return messages;
    }
}
