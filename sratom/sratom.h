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
   @file sratom.h API for Sratom, an LV2 Atom RDF serialisation library.
*/

#ifndef SRATOM_SRATOM_H
#define SRATOM_SRATOM_H

#include <stdint.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "serd/serd.h"

#ifdef SRATOM_SHARED
#    ifdef _WIN32
#        define SRATOM_LIB_IMPORT __declspec(dllimport)
#        define SRATOM_LIB_EXPORT __declspec(dllexport)
#    else
#        define SRATOM_LIB_IMPORT __attribute__((visibility("default")))
#        define SRATOM_LIB_EXPORT __attribute__((visibility("default")))
#    endif
#    ifdef SRATOM_INTERNAL
#        define SRATOM_API SRATOM_LIB_EXPORT
#    else
#        define SRATOM_API SRATOM_LIB_IMPORT
#    endif
#else
#    define SRATOM_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
   @defgroup sratom Sratom
   An LV2 Atom RDF serialisation library.
   @{
*/

/**
   Atom serialiser.
*/
typedef struct SratomImpl Sratom;

/**
   Create a new Atom serialiser.
*/
SRATOM_API
Sratom*
sratom_new(LV2_URID_Map*   map,
           LV2_URID_Unmap* unmap);

/**
   Free an Atom serialisation.
*/
SRATOM_API
void
sratom_free(Sratom* sratom);

/**
   Serialise an Atom to a SerdWriter.
*/
SRATOM_API
void
atom_to_rdf(Sratom*         sratom,
            const SerdNode* subject,
            const SerdNode* predicate,
            const LV2_Atom* atom,
            uint32_t        flags);

/**
   Serialise an Atom body to a SerdWriter.
*/
SRATOM_API
void
atom_body_to_rdf(Sratom*         sratom,
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
SRATOM_API
char*
atom_to_turtle(Sratom*         sratom,
               const SerdNode* subject,
               const SerdNode* predicate,
               const LV2_Atom* atom);

/**
   @}
*/

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* SRATOM_SRATOM_H */
