/*
  Copyright 2012 David Robillard <http://drobilla.net>

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

/**
   @file seriatom.h API for Seriatom, an LV2 Atom RDF serialisation library.
*/

#ifndef SERIATOM_SERIATOM_H
#define SERIATOM_SERIATOM_H

#include <stdint.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "serd/serd.h"

#ifdef SERIATOM_SHARED
#    ifdef _WIN32
#        define SERIATOM_LIB_IMPORT __declspec(dllimport)
#        define SERIATOM_LIB_EXPORT __declspec(dllexport)
#    else
#        define SERIATOM_LIB_IMPORT __attribute__((visibility("default")))
#        define SERIATOM_LIB_EXPORT __attribute__((visibility("default")))
#    endif
#    ifdef SERIATOM_INTERNAL
#        define SERIATOM_API SERIATOM_LIB_EXPORT
#    else
#        define SERIATOM_API SERIATOM_LIB_IMPORT
#    endif
#else
#    define SERIATOM_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
   @defgroup seriatom Seriatom
   An LV2 Atom RDF serialisation library.
   @{
*/

/**
   Atom serialiser.
*/
typedef struct SeriatomImpl Seriatom;

/**
   Create a new Atom serialiser.
*/
SERIATOM_API
Seriatom*
seriatom_new(LV2_URID_Map*   map,
             LV2_URID_Unmap* unmap);

/**
   Free an Atom serialisation.
*/
SERIATOM_API
void
seriatom_free(Seriatom* seriatom);

/**
   Serialise an Atom to a SerdWriter.
*/
SERIATOM_API
void
atom_to_rdf(Seriatom*       seriatom,
            const SerdNode* subject,
            const SerdNode* predicate,
            const LV2_Atom* atom,
            uint32_t        flags);

/**
   Serialise an Atom body to a SerdWriter.
*/
SERIATOM_API
void
atom_body_to_rdf(Seriatom*       seriatom,
                 const SerdNode* subject,
                 const SerdNode* predicate,
                 uint32_t        type_urid,
                 uint32_t        size,
                 const void*     body,
                 uint32_t        flags);

/**
   Serialise an Atom to a Turtle string.
   The returned string must be free()'d by the caller.
*/
SERIATOM_API
char*
atom_to_turtle(Seriatom*       seriatom,
               const SerdNode* subject,
               const SerdNode* predicate,
               const LV2_Atom* atom);

/**
   @}
*/

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* SERIATOM_SERIATOM_H */
