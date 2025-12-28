/*

mailio.cppm
-----------

C++20 root module interface for mailio library.
This module re-exports all mailio submodules for convenient single-import usage.

Usage:
    import mailio;           // Import everything
    // or selectively:
    import mailio.smtp;      // Just SMTP client
    import mailio.imap;      // Just IMAP client
    import mailio.pop3;      // Just POP3 client
    import mailio.mime;      // Just MIME/message handling
    import mailio.codec;     // Just encoding/decoding
    import mailio.net;       // Just networking primitives

Copyright (C) 2025, Sylvain Guinebert (github.com/sguinebert).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/

export module mailio;

// Re-export all submodules
export import mailio.codec;
export import mailio.net;
export import mailio.mime;
export import mailio.smtp;
export import mailio.pop3;
export import mailio.imap;
