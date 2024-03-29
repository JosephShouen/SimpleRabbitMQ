#include <functional>
#include <unistd.h>
#include <event2/event.h>
#include <amqpcpp/libevent.h>

class LibEventHandlerMyError : public AMQP::LibEventHandler
{
public:
    LibEventHandlerMyError(struct event_base* evbase) : LibEventHandler(evbase), evbase_(evbase_) {}
    void onError(AMQP::TcpConnection *connection, const char *message) override
    {
        std::cout << "Error: " << message << std::endl;
        event_base_loopbreak(evbase_);
    }
private:
    struct event_base* evbase_ {nullptr};
};

class ConnHandler
{
public:
    using EventBasePtrT = std::unique_ptr<struct event_base, std::function<void(struct event_base*)> >;
    using EventPtrT = std::unique_ptr<struct event, std::function<void(struct event*)> >;

    ConnHandler()
        : evbase_(event_base_new(), event_base_free)
        , stdin_event_(event_new(evbase_.get(), STDIN_FILENO, EV_READ, stop,
                    evbase_.get()), event_free)
        , evhandler_(evbase_.get())
    {
        event_add(stdin_event_.get(), nullptr);
    }

    void Start()
    {
        event_base_dispatch(evbase_.get());
    }
    void Stop()
    {
        event_base_loopbreak(evbase_.get());
    }

    operator AMQP::TcpHandler*()
    {
        return &evhandler_;
    }

private:
    static void stop(evutil_socket_t fd, short what, void *evbase) { }
    EventBasePtrT evbase_;
    EventPtrT stdin_event_;
    LibEventHandlerMyError evhandler_;
};

