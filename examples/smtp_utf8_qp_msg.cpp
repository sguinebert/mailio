/*

smtp_utf8_qp_msg.cpp
--------------------

Connects to an SMTP server and sends a message with UTF8 content and subject.


Copyright (C) 2016, Tomislav Karastojkovic (http://www.alepho.com).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/


#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <mailio/mime/message.hpp>
#include <mailio/mime/mime.hpp>
#include <mailio/smtp/client.hpp>


using mailio::codec;
using mailio::string_t;
using mailio::message;
using mailio::mail_address;
using mailio::mime;
using mailio::smtp::auth_method;
using mailio::smtp::client;
using mailio::smtp::error;
using mailio::dialog_error;
using std::cout;
using std::endl;


int main()
{
    boost::asio::io_context io_ctx;

    boost::asio::co_spawn(io_ctx,
        [&]() -> boost::asio::awaitable<void>
        {
            try
            {
                // create mail message
                message msg;
                msg.from(mail_address(string_t("mailio library", "ASCII", codec::codec_t::BASE64), "mailio@mailserver.com"));// set the correct sender name and address
                msg.add_recipient(mail_address(string_t("mailio library", "ASCII", codec::codec_t::BASE64), "mailio@gmail.com"));// set the correct recipent name and address
                msg.add_recipient(mail_address(string_t("mailio library", "ASCII", codec::codec_t::BASE64), "mailio@outlook.com"));// add more recipients
                msg.add_cc_recipient(mail_address(string_t("mailio library", "ASCII", codec::codec_t::BASE64), "mailio@yahoo.com"));// add CC recipient
                msg.add_bcc_recipient(mail_address(string_t("mailio library", "ASCII", codec::codec_t::BASE64), "mailio@zoho.com"));

                msg.subject("smtp utf8 quoted printable message");
                // create message in Cyrillic alphabet
                // set Transfer Encoding to Quoted Printable and set Content Type to UTF8
                msg.content_transfer_encoding(mime::content_transfer_encoding_t::QUOTED_PRINTABLE);
                msg.content_type(message::media_type_t::TEXT, "plain", "utf-8");

                msg.content(
                    u8"Ово је јако дугачка порука која има и празних линија и предугачких линија. Није јасно како ће се текст преломити\r\n"
                    u8"па се надам да ће то овај текст показати.\r\n"
                    u8"\r\n"
                    u8"Треба видети како познати мејл клијенти ломе текст, па на\r\n"
                    u8"основу тога дорадити форматирање мејла. А можда и нема потребе, јер libmailio није замишљен да се\r\n"
                    u8"бави форматирањем текста.\r\n"
                    u8"\r\n\r\n"
                    u8"У сваком случају, после провере латинице треба урадити и проверу utf8 карактера одн. ћирилице\r\n"
                    u8"и видети како се прелама текст када су карактери вишебајтни. Требало би да је небитно да ли је енкодинг\r\n"
                    u8"base64 или quoted printable, јер се ascii карактери преламају у нове линије. Овај тест би требало да\r\n"
                    u8"покаже има ли багова у логици форматирања,\r\n"
                    u8"а исто то треба проверити са парсирањем.\r\n"
                    u8"\r\n\r\n\r\n\r\n"
                    u8"Овде је и провера за низ празних линија.");

                // use a server with plain (non-SSL) connectivity
                client conn(io_ctx.get_executor());
                co_await conn.connect("smtp.mailserver.com", "587");
                co_await conn.read_greeting();
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
