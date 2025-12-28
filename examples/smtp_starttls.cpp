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

boost::asio::awaitable<void> send_email(boost::asio::io_context& io_ctx, boost::asio::ssl::context& ssl_ctx)
{
    try
    {
        mailio::smtp::options options;
        options.tls.use_default_verify_paths = true;
        options.tls.verify = mailio::net::verify_mode::peer;
        options.tls.verify_host = true;
        options.auto_starttls = true;

        client conn(io_ctx.get_executor(), options);
        co_await conn.connect("smtp.gmail.com", "587",
            mailio::net::tls_mode::starttls, &ssl_ctx, "smtp.gmail.com");

        co_await conn.authenticate("user@gmail.com", "password", auth_method::login);

        message msg;
        msg.from(mail_address("Sender", "user@gmail.com"));
        msg.add_recipient(mail_address("Recipient", "recipient@example.com"));
        msg.subject("Test from mailio async");
        msg.content("Hello, World!");

        co_await conn.send(msg);
        co_await conn.quit();

        std::cout << "Email sent successfully!" << std::endl;
    }
    catch (error& exc)
    {
        std::cerr << "Error: " << exc.what() << std::endl;
    }
    catch (dialog_error& exc)
    {
        std::cerr << "Dialog error: " << exc.what() << std::endl;
    }
}

int main()
{
    try
    {
        boost::asio::io_context io_ctx;
        boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);

        boost::asio::co_spawn(io_ctx, send_email(io_ctx, ssl_ctx), boost::asio::detached);
        io_ctx.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Main Error: " << e.what() << std::endl;
    }
    return 0;
}
