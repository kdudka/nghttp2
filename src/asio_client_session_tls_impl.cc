/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "asio_client_session_tls_impl.h"
#include "asio_common.h"

namespace nghttp2 {
namespace asio_http2 {
namespace client {

session_tls_impl::session_tls_impl(boost::asio::io_service &io_service,
                                   boost::asio::ssl::context &tls_ctx,
                                   const std::string &host,
                                   const std::string &service)
    : session_impl(io_service), socket_(io_service, tls_ctx) {
  // this callback setting is no effect is
  // ssl::context::set_verify_mode(boost::asio::ssl::verify_peer) is
  // not used, which is what we want.
  socket_.set_verify_callback(boost::asio::ssl::rfc2818_verification(host));

  start_resolve(host, service);
}

session_tls_impl::~session_tls_impl() {}

void session_tls_impl::start_connect(tcp::resolver::iterator endpoint_it) {
  boost::asio::async_connect(socket(), endpoint_it,
                             [this](const boost::system::error_code &ec,
                                    tcp::resolver::iterator endpoint_it) {
    if (ec) {
      not_connected(ec);
      return;
    }

    socket_.async_handshake(
        boost::asio::ssl::stream_base::client,
        [this, endpoint_it](const boost::system::error_code &ec) {
          if (ec) {
            not_connected(ec);
            return;
          }

          if (!tls_h2_negotiated(socket_)) {
            not_connected(
                make_error_code(NGHTTP2_ASIO_ERR_TLS_NO_APP_PROTO_NEGOTIATED));
            return;
          }

          connected(endpoint_it);
        });
  });
}

tcp::socket &session_tls_impl::socket() { return socket_.next_layer(); }

void session_tls_impl::read_socket(
    std::function<void(const boost::system::error_code &ec, std::size_t n)> h) {
  socket_.async_read_some(boost::asio::buffer(rb_), h);
}

void session_tls_impl::write_socket(
    std::function<void(const boost::system::error_code &ec, std::size_t n)> h) {
  boost::asio::async_write(socket_, boost::asio::buffer(wb_, wblen_), h);
}

void session_tls_impl::shutdown_socket() {
  socket_.async_shutdown([](const boost::system::error_code &ec) {});
}

} // namespace client
} // namespace asio_http2
} // namespace nghttp2
