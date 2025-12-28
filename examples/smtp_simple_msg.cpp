/*

smtps_simple_msg.cpp
--------------------

Connects to an SMTP server via START_TLS and sends a simple message.


Copyright (C) 2025, Sylvain Guinebert (github.com/sguinebert).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/


#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <mailio/mime/message.hpp>
#include <mailio/net/tls_mode.hpp>
#include <mailio/smtp/client.hpp>


using mailio::message;
using mailio::mail_address;
using mailio::smtp::auth_method;
using mailio::smtp::client;
using mailio::smtp::error;
using mailio::net::dialog_error;
using std::cout;
using std::endl;


int main()
{
    boost::asio::io_context io_ctx;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);

    boost::asio::co_spawn(io_ctx,
        [&]() -> boost::asio::awaitable<void>
        {
            try
            {
                // create mail message
                message msg;
                msg.from(mail_address("mailio library", "mailio@gmail.com"));// set the correct sender name and address
                msg.add_recipient(mail_address("mailio library", "mailio@gmail.com"));// set the correct recipent name and address
                msg.subject("smtps simple message");
                msg.content("Hello, World!");

                // connect to server
                mailio::smtp::options options;
                options.tls.use_default_verify_paths = true;
                options.tls.verify = mailio::net::verify_mode::peer;
                options.tls.verify_host = true;
                options.auto_starttls = true;

                client conn(io_ctx.get_executor(), options);
                co_await conn.connect("smtp.gmail.com", "587",
                    mailio::net::tls_mode::starttls, &ssl_ctx, "smtp.gmail.com");

                // modify username/password to use real credentials
                co_await conn.authenticate("mailio@gmail.com", "mailiopass", auth_method::login);
                co_await conn.send(msg);
                co_await conn.quit();
            }
            catch (error& exc)
            {
                cout << exc.what() << endl;
            }
            catch (dialog_error& exc)
            {
                cout << exc.what() << endl;
            }
            co_return;
        },
        boost::asio::detached);

    io_ctx.run();
    return EXIT_SUCCESS;
}
