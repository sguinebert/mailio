/*

pop3s_attachment.cpp
--------------------

Fetches attachments of a message from a POP3 server via TLS.

For this sample to be executed properly, use the message sent by `smtps_attachment.cpp`.


Copyright (C) 2025, Sylvain Guinebert (github.com/sguinebert).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/


#include <fstream>
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <mailio/mime/message.hpp>
#include <mailio/net/tls_mode.hpp>
#include <mailio/pop3/client.hpp>


using mailio::codec;
using mailio::message;
using mailio::string_t;
using mailio::pop3::client;
using mailio::pop3::error;
using mailio::net::dialog_error;
using std::cout;
using std::endl;
using std::ofstream;
using std::string;


int main()
{
    boost::asio::io_context io_ctx;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);

    boost::asio::co_spawn(io_ctx,
        [&]() -> boost::asio::awaitable<void>
        {
            try
            {
                mailio::pop3::options options;
                options.tls.use_default_verify_paths = true;
                options.tls.verify = mailio::net::verify_mode::peer;
                options.tls.verify_host = true;

                client conn(io_ctx.get_executor(), options);
                co_await conn.connect("pop3.mailserver.com", "995",
                    mailio::net::tls_mode::implicit, &ssl_ctx, "pop3.mailserver.com");
                co_await conn.read_greeting();
                // modify to use real account
                co_await conn.login("mailio@mailserver.com", "mailiopass");

                std::string raw = co_await conn.retr(1);
                message msg;
                msg.line_policy(codec::line_len_policy_t::RECOMMENDED);
                msg.parse(raw);

                ofstream ofs1("alepho.png", std::ios::binary);
                string_t att1;
                msg.attachment(1, ofs1, att1);
                ofstream ofs2("infiniti.png", std::ios::binary);
                string_t att2;
                msg.attachment(2, ofs2, att2);
                cout << "Received message with subject `" << msg.subject() << "` and attached files `" <<
                    att1 << "` and `" << att2 << "` saved as `alepho.png` and `infiniti.png`." << endl;

                co_await conn.quit();
            }
            catch (const error& exc)
            {
                cout << exc.what() << endl;
            }
            catch (const dialog_error& exc)
            {
                cout << exc.what() << endl;
            }
            co_return;
        },
        boost::asio::detached);

    io_ctx.run();
    return EXIT_SUCCESS;
}
