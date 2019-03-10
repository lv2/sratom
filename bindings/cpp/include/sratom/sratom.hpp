/*
  Copyright 2019-2021 David Robillard <d@drobilla.net>

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

/// @file sratom.hpp C++ API for Sratom

#ifndef SRATOM_SRATOM_HPP
#define SRATOM_SRATOM_HPP

#include "lv2/urid/urid.h"
#include "serd/serd.hpp"
#include "sratom/sratom.h"

#include <cstdint>
#include <sstream>

/**
   @defgroup sratomxx Sratomxx
   C++ bindings for Sratom, a library for serialising LV2 atoms.
   @{
*/

namespace sratom {

namespace detail {

struct Deleter {
  void operator()(void* ptr) { sratom_free(ptr); }
};

} // namespace detail

/// Flags to control how atoms are written and read
typedef enum {
  /**
     Write the main subject with a label.

     If set, the main subject will be written using its blank node label,
     instead of as an anonymous node.
  */
  named_subject = SRATOM_NAMED_SUBJECT,

  /**
     Write pretty numeric literals.

     If set, numbers may be written as pretty literals instead of string
     literals with explicit data types.  Note that enabling this means that
     types will not survive a round trip.
  */
  pretty_numbers = SRATOM_PRETTY_NUMBERS,

  /**
     Write terse output without newlines.

     If set, top level atoms will be written as a single long line.
  */
  terse = SRATOM_TERSE,
} Flag;

using Flags = serd::detail::Flags<Flag>;

class Dumper
  : public serd::detail::BasicWrapper<SratomDumper, sratom_dumper_free>
{
public:
  Dumper(serd::World& world, LV2_URID_Map& map, LV2_URID_Unmap& unmap)
    : BasicWrapper(sratom_dumper_new(world.cobj(), &map, &unmap))
  {}

  int write(const serd::Env& env,
            serd::SinkView   sink,
            const LV2_Atom&  atom,
            const Flags      flags = {})
  {
    return sratom_dump_atom(
      cobj(), env.cobj(), sink.cobj(), nullptr, nullptr, &atom, flags);
  }

  int write(const serd::Env&  env,
            serd::SinkView    sink,
            const serd::Node& subject,
            const serd::Node& predicate,
            const LV2_Atom&   atom,
            const Flags       flags = {})
  {
    return sratom_dump_atom(cobj(),
                            env.cobj(),
                            sink.cobj(),
                            subject.cobj(),
                            predicate.cobj(),
                            &atom,
                            flags);
  }

  int write(const serd::Env&  env,
            serd::SinkView    sink,
            const serd::Node& subject,
            const serd::Node& predicate,
            LV2_URID          type,
            uint32_t          size,
            const void*       body,
            const Flags       flags = {})
  {
    return sratom_dump(cobj(),
                       env.cobj(),
                       sink.cobj(),
                       subject.cobj(),
                       predicate.cobj(),
                       type,
                       size,
                       body,
                       flags);
  }

  std::string to_string(const serd::Env& env,
                        const LV2_Atom&  atom,
                        const Flags      flags = {})
  {
    char*       c_str  = sratom_to_string(cobj(), env.cobj(), &atom, flags);
    std::string result = c_str;
    sratom_free(c_str);
    return result;
  }

private:
  static size_t string_sink(const void* buf, size_t, size_t nmemb, void* stream)
  {
    std::string& string = *(std::string*)stream;
    string.insert(string.size(), static_cast<const char*>(buf), nmemb);
    return nmemb;
  }
};

class Loader
  : public serd::detail::BasicWrapper<SratomLoader, sratom_loader_free>
{
public:
  using AtomPtr = std::unique_ptr<LV2_Atom, detail::Deleter>;

  Loader(serd::World& world, LV2_URID_Map& map)
    : BasicWrapper(sratom_loader_new(world.cobj(), &map))
  {}

  int read(const serd::Optional<serd::Node>& base_uri,
           LV2_Atom_Forge&                   forge,
           const serd::Model&                model,
           const serd::Node&                 node)
  {
    return sratom_load(
      cobj(), base_uri.cobj(), &forge, model.cobj(), node.cobj());
  }

  AtomPtr read_string(serd::Env& env, const char* str)
  {
    return AtomPtr{sratom_from_string(cobj(), env.cobj(), str)};
  }

  AtomPtr read_model(const serd::Optional<serd::Node>& base_uri,
                     const serd::Model&                model,
                     const serd::Node&                 subject)
  {
    return AtomPtr{
      sratom_from_model(cobj(), base_uri.cobj(), model.cobj(), subject.cobj())};
  }
};

} // namespace sratom

/**
   @}
*/

#endif // SRATOM_SRATOM_HPP
