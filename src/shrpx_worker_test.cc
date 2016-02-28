/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2016 Tatsuhiro Tsujikawa
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
#include "shrpx_worker_test.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif // HAVE_UNISTD_H

#include <cstdlib>

#include <CUnit/CUnit.h>

#include "shrpx_worker.h"
#include "shrpx_connect_blocker.h"

namespace shrpx {

void test_shrpx_worker_match_downstream_addr_group(void) {
  auto groups = std::vector<DownstreamAddrGroup>();
  for (auto &s : {"nghttp2.org/", "nghttp2.org/alpha/bravo/",
                  "nghttp2.org/alpha/charlie", "nghttp2.org/delta%3A",
                  "www.nghttp2.org/", "[::1]/", "nghttp2.org/alpha/bravo/delta",
                  // Check that match is done in the single node
                  "example.com/alpha/bravo", "192.168.0.1/alpha/"}) {
    groups.push_back(DownstreamAddrGroup{ImmutableString(s)});
  }

  Router router;

  for (size_t i = 0; i < groups.size(); ++i) {
    auto &g = groups[i];
    router.add_route(StringRef{g.pattern}, i);
  }

  CU_ASSERT(0 == match_downstream_addr_group(
                     router, StringRef::from_lit("nghttp2.org"),
                     StringRef::from_lit("/"), groups, 255));

  // port is removed
  CU_ASSERT(0 == match_downstream_addr_group(
                     router, StringRef::from_lit("nghttp2.org:8080"),
                     StringRef::from_lit("/"), groups, 255));

  // host is case-insensitive
  CU_ASSERT(4 == match_downstream_addr_group(
                     router, StringRef::from_lit("WWW.nghttp2.org"),
                     StringRef::from_lit("/alpha"), groups, 255));

  CU_ASSERT(1 == match_downstream_addr_group(
                     router, StringRef::from_lit("nghttp2.org"),
                     StringRef::from_lit("/alpha/bravo/"), groups, 255));

  // /alpha/bravo also matches /alpha/bravo/
  CU_ASSERT(1 == match_downstream_addr_group(
                     router, StringRef::from_lit("nghttp2.org"),
                     StringRef::from_lit("/alpha/bravo"), groups, 255));

  // path part is case-sensitive
  CU_ASSERT(0 == match_downstream_addr_group(
                     router, StringRef::from_lit("nghttp2.org"),
                     StringRef::from_lit("/Alpha/bravo"), groups, 255));

  CU_ASSERT(1 == match_downstream_addr_group(
                     router, StringRef::from_lit("nghttp2.org"),
                     StringRef::from_lit("/alpha/bravo/charlie"), groups, 255));

  CU_ASSERT(2 == match_downstream_addr_group(
                     router, StringRef::from_lit("nghttp2.org"),
                     StringRef::from_lit("/alpha/charlie"), groups, 255));

  // pattern which does not end with '/' must match its entirely.  So
  // this matches to group 0, not group 2.
  CU_ASSERT(0 == match_downstream_addr_group(
                     router, StringRef::from_lit("nghttp2.org"),
                     StringRef::from_lit("/alpha/charlie/"), groups, 255));

  CU_ASSERT(255 == match_downstream_addr_group(
                       router, StringRef::from_lit("example.org"),
                       StringRef::from_lit("/"), groups, 255));

  CU_ASSERT(255 == match_downstream_addr_group(router, StringRef::from_lit(""),
                                               StringRef::from_lit("/"), groups,
                                               255));

  CU_ASSERT(255 == match_downstream_addr_group(router, StringRef::from_lit(""),
                                               StringRef::from_lit("alpha"),
                                               groups, 255));

  CU_ASSERT(255 ==
            match_downstream_addr_group(router, StringRef::from_lit("foo/bar"),
                                        StringRef::from_lit("/"), groups, 255));

  // If path is StringRef::from_lit("*", only match with host + "/").
  CU_ASSERT(0 == match_downstream_addr_group(
                     router, StringRef::from_lit("nghttp2.org"),
                     StringRef::from_lit("*"), groups, 255));

  CU_ASSERT(5 ==
            match_downstream_addr_group(router, StringRef::from_lit("[::1]"),
                                        StringRef::from_lit("/"), groups, 255));
  CU_ASSERT(5 == match_downstream_addr_group(
                     router, StringRef::from_lit("[::1]:8080"),
                     StringRef::from_lit("/"), groups, 255));
  CU_ASSERT(255 ==
            match_downstream_addr_group(router, StringRef::from_lit("[::1"),
                                        StringRef::from_lit("/"), groups, 255));
  CU_ASSERT(255 == match_downstream_addr_group(
                       router, StringRef::from_lit("[::1]8000"),
                       StringRef::from_lit("/"), groups, 255));

  // Check the case where adding route extends tree
  CU_ASSERT(6 == match_downstream_addr_group(
                     router, StringRef::from_lit("nghttp2.org"),
                     StringRef::from_lit("/alpha/bravo/delta"), groups, 255));

  CU_ASSERT(1 == match_downstream_addr_group(
                     router, StringRef::from_lit("nghttp2.org"),
                     StringRef::from_lit("/alpha/bravo/delta/"), groups, 255));

  // Check the case where query is done in a single node
  CU_ASSERT(7 == match_downstream_addr_group(
                     router, StringRef::from_lit("example.com"),
                     StringRef::from_lit("/alpha/bravo"), groups, 255));

  CU_ASSERT(255 == match_downstream_addr_group(
                       router, StringRef::from_lit("example.com"),
                       StringRef::from_lit("/alpha/bravo/"), groups, 255));

  CU_ASSERT(255 == match_downstream_addr_group(
                       router, StringRef::from_lit("example.com"),
                       StringRef::from_lit("/alpha"), groups, 255));

  // Check the case where quey is done in a single node
  CU_ASSERT(8 == match_downstream_addr_group(
                     router, StringRef::from_lit("192.168.0.1"),
                     StringRef::from_lit("/alpha"), groups, 255));

  CU_ASSERT(8 == match_downstream_addr_group(
                     router, StringRef::from_lit("192.168.0.1"),
                     StringRef::from_lit("/alpha/"), groups, 255));

  CU_ASSERT(8 == match_downstream_addr_group(
                     router, StringRef::from_lit("192.168.0.1"),
                     StringRef::from_lit("/alpha/bravo"), groups, 255));

  CU_ASSERT(255 == match_downstream_addr_group(
                       router, StringRef::from_lit("192.168.0.1"),
                       StringRef::from_lit("/alph"), groups, 255));

  CU_ASSERT(255 == match_downstream_addr_group(
                       router, StringRef::from_lit("192.168.0.1"),
                       StringRef::from_lit("/"), groups, 255));

  router.dump();
}

} // namespace shrpx
