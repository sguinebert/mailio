/*

smtps_multipart.cpp
-------------------

Connects to an SMTP server via START_TLS and sends a multipart message.


Copyright (C) 2023, Tomislav Karastojkovic (http://www.alepho.com).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/


#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <mailio/mime/message.hpp>
#include <mailio/smtp/client.hpp>


using mailio::message;
using mailio::mail_address;
using mailio::mime;
using mailio::smtp::auth_method;
using mailio::smtp::client;
using mailio::smtp::error;
using mailio::dialog_error;
using std::cout;
using std::endl;
using std::ifstream;
using std::ostringstream;


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
                msg.from(mail_address("mailio library", "mailio@mailserver.com"));// set the correct sender name and address
                msg.add_recipient(mail_address("mailio library", "mailio@mailserver.com"));// set the correct recipent name and address
                msg.subject("smtps multipart message");
                msg.content_type(message::media_type_t::MULTIPART, "related");
                msg.content_type().boundary("012456789@mailio.dev");

                mime title;
                title.content_type(message::media_type_t::TEXT, "html", "utf-8");
                title.content_transfer_encoding(mime::content_transfer_encoding_t::BIT_8);
                title.content("<html><head></head><body><h1>Здраво, Свете!</h1></body></html>");

                ifstream ifs("aleph0.png");
                ostringstream ofs;
                ofs << ifs.rdbuf();

                mime img;
                img.content_type(message::media_type_t::IMAGE, "png");
                img.content_transfer_encoding(mime::content_transfer_encoding_t::BASE_64);
                img.content_disposition(mime::content_disposition_t::INLINE);
                img.content(ofs.str());
                img.name("a0.png");

                msg.add_part(title);
                msg.add_part(img);

                // connect to server over start tls
                client conn(io_ctx.get_executor());
                co_await conn.connect("smtp.mailserver.com", "587");
                co_await conn.read_greeting();
                co_await conn.ehlo();
                co_await conn.start_tls(ssl_ctx, "smtp.mailserver.com");
                co_await conn.ehlo();

                // modify username/password to use real credentials
                co_await conn.authenticate("mailio@mailserver.com", "mailiopass", auth_method::login);
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
