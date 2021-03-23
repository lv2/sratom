/*
  Copyright 2018-2021 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#undef NDEBUG

#include "test_utils.h"

#include "lv2/urid/urid.h"
#include "serd/serd.hpp"
#include "sratom/sratom.hpp"

namespace sratom {
namespace test {
namespace {

void
test_dumper()
{
  serd::World    world;
  serd::Env      env;
  Uris           uris{nullptr, 0};
  LV2_URID_Map   map{&uris, urid_map};
  LV2_URID_Unmap unmap{&uris, urid_unmap};
  Dumper         dumper{world, map, unmap};

  const LV2_Atom_Int a_int = {{sizeof(int32_t), urid_map(&uris, LV2_ATOM__Int)},
                              42};

  assert(dumper.to_string(env, a_int.atom, {}) ==
         "( \"42\"^^<http://www.w3.org/2001/XMLSchema#int> ) .\n");

  assert(dumper.to_string(env, a_int.atom, sratom::Flag::pretty_numbers) ==
         "( 42 ) .\n");
}

} // namespace
} // namespace test
} // namespace sratom

int
main()
{
  sratom::test::test_dumper();

  return 0;
}
