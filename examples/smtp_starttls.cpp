#include <iostream>
#include <mailio/smtp.hpp>
#include <mailio/message.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

using boost::asio::ip::tcp;
using namespace mailio;

boost::asio::awaitable<void> send_email(boost::asio::io_context& io_ctx, boost::asio::ssl::context& ssl_ctx)
{
    try
    {
        // 1. Connect with plain TCP
        tcp::resolver resolver(io_ctx);
        auto endpoints = co_await resolver.async_resolve("smtp.gmail.com", "587", boost::asio::use_awaitable);
        tcp::socket socket(io_ctx);
        co_await boost::asio::async_connect(socket, endpoints, boost::asio::use_awaitable);

        // 2. Create client (moves socket)
        smtp_client<tcp::socket> client(std::move(socket));

        // 3. Read greeting and EHLO
        co_await client.async_read_greeting(boost::asio::use_awaitable);
        co_await client.async_ehlo(boost::asio::use_awaitable);

        // 4. Upgrade to STARTTLS
        // 'client' is moved-from after this call
        auto secure_client = co_await client.starttls(ssl_ctx);

        // 5. EHLO again (required after STARTTLS)
        co_await secure_client.async_ehlo(boost::asio::use_awaitable);

        // 6. Authenticate
        co_await secure_client.async_authenticate("user@gmail.com", "password", smtp_base::auth_method_t::LOGIN, boost::asio::use_awaitable);

        // 7. Send message
        message msg;
        msg.from(mail_address("Sender", "user@gmail.com"));
        msg.add_recipient(mail_address("Recipient", "recipient@example.com"));
        msg.subject("Test from mailio async");
        msg.content("Hello, World!");

        co_await secure_client.async_submit(msg, boost::asio::use_awaitable);

        std::cout << "Email sent successfully!" << std::endl;
    }
    catch (std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main()
{
    try
    {
        boost::asio::io_context io_ctx;
        boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
        ssl_ctx.set_default_verify_paths();
        ssl_ctx.set_verify_mode(boost::asio::ssl::verify_none); // For testing

        boost::asio::co_spawn(io_ctx, send_email(io_ctx, ssl_ctx), boost::asio::detached);
        io_ctx.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Main Error: " << e.what() << std::endl;
    }
    return 0;
}
