/***************************************************************************
* Copyright (c) 2016, Johan Mabille, Sylvain Corlay, Martin Renou          *
* Copyright (c) 2016, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XEUS_SERVER_ZMQ_HPP
#define XEUS_SERVER_ZMQ_HPP

#include <functional>
#include <optional>

#include "xeus/xeus_context.hpp"
#include "xeus/xkernel_configuration.hpp"
#include "xeus/xserver.hpp"

#include "xeus-zmq.hpp"

namespace xeus
{
    class xserver_zmq_impl;
    
    class XEUS_ZMQ_API xserver_zmq : public xserver
    {
    public:

        ~xserver_zmq() override;

        using xserver::notify_internal_listener;

        // LOCAL PATCH (mx-kernel) -- idle callback and configurable poll
        // timeout. See patches/README.md.
        //
        // An embedded kernel needs two things the blocking poll cannot give:
        // a chance to notice that stop() was requested, and a chance to do
        // work on the thread that owns the ZMQ sockets (publishing on IOPub
        // from any other thread is a data race). Polling with a timeout and
        // invoking a callback on each idle tick provides both.
        //
        // Both setters must be called before start(); they are not
        // synchronised, and the server thread reads them once it is running.
        using idle_callback_type = std::function<void()>;
        void set_idle_callback(idle_callback_type cb);

        // Poll timeout in milliseconds. A negative value blocks indefinitely,
        // which restores the historical behaviour but means stop() is not
        // observed until the next message arrives.
        void set_poll_timeout(long timeout_ms);
        long get_poll_timeout() const;

    protected:

        // Invoked by inheriting classes when a poll times out with no message.
        void notify_idle();

        xserver_zmq(xcontext& context,
                    const xconfiguration& config,
                    nl::json::error_handler_t eh);

        // API for inheriting classes
        void start_publisher_thread();
        void start_heartbeat_thread();
        void stop_channels();

        void set_request_stop(bool stop);
        bool is_stopped() const;

        // The following methods must be called in the same thread
        using message_channel = std::pair<xmessage, channel>;
        std::optional<message_channel> poll_channels(long timeout);
        void send_shell_message(xmessage msg);
        void send_control_message(xmessage msg);

    private:

        // Implementation of xserver virtual methods
        xcontrol_messenger& get_control_messenger_impl() override;

        void send_shell_impl(xmessage msg) override;
        void send_control_impl(xmessage msg) override;
        void send_stdin_impl(xmessage msg) override;
        void publish_impl(xpub_message msg, channel c) override;

        void abort_queue_impl(const listener& l, long polling_interval) override;
        void update_config_impl(xconfiguration& config) const override;

        std::unique_ptr<xserver_zmq_impl> p_impl;

        // LOCAL PATCH (mx-kernel)
        idle_callback_type m_idle_callback;
        long m_poll_timeout = 100;
    };

    XEUS_ZMQ_API
    std::unique_ptr<xserver> make_xserver_default(xcontext& context,
                                                  const xconfiguration& config,
                                                  nl::json::error_handler_t eh = nl::json::error_handler_t::strict);
}

#endif
