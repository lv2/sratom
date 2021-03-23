/*
  Copyright 2012-2021 David Robillard <d@drobilla.net>

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

/// @file sratom.h Public API for Sratom

#ifndef SRATOM_SRATOM_H
#define SRATOM_SRATOM_H

#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2/atom/util.h"
#include "lv2/urid/urid.h"
#include "serd/serd.h"

#include <stdint.h>

#if defined(_WIN32) && !defined(SRATOM_STATIC) && defined(SRATOM_INTERNAL)
#  define SRATOM_API __declspec(dllexport)
#elif defined(_WIN32) && !defined(SRATOM_STATIC)
#  define SRATOM_API __declspec(dllimport)
#elif defined(__GNUC__)
#  define SRATOM_API __attribute__((visibility("default")))
#else
#  define SRATOM_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
   @defgroup sratom Sratom C API
   @{
*/

/// Return status code
typedef enum {
  SRATOM_SUCCESS,      ///< No error
  SRATOM_BAD_FILE_URI, ///< Invalid file URI encountered
  SRATOM_BAD_VECTOR,   ///< Invalid vector (missing atom:childType)
  SRATOM_BAD_FORGE,    ///< Error forging output atom (likely overflow)
} SratomStatus;

/// Free memory allocated in the sratom library
SRATOM_API
void
sratom_free(void* ptr);

/**
   @defgroup sratom_dumping Dumping Atoms
   Writing atoms to a statement stream or a string.
   @{
*/

/// Flags to control how atoms are written as statements
typedef enum {
  /**
     Write the main subject with a label.

     If set, the main subject will be written using its blank node label,
     instead of as an anonymous node.
  */
  SRATOM_NAMED_SUBJECT = 1u << 1u,

  /**
     Write pretty numeric literals in Turtle or TriG.

     If set, numbers may be written as "pretty" xsd:integer or xsd:decimal
     literals (without quoting or an explicit datatype) instead of string
     literals with explicit data types.  It is best to leave this off in
     contexts where human readability doesn't matter very much, since the
     explicit form is more consistent and likely to survive storage,
     transmission, and transformation with perfect fidelity.
  */
  SRATOM_PRETTY_NUMBERS = 1u << 2u,

  /**
     Write terse output without newlines.

     If set when writing to a string, top-level atoms will be written as a
     single long line.  From a machine perspective, the output is identical,
     but this can be convenient in contexts where taking up less space for the
     representation of atoms is desirable, such as developer logs or
     transmission over a network.
  */
  SRATOM_TERSE = 1u << 3u,
} SratomDumperFlag;

/// Bitwise OR of SratomDumperFlag values
typedef uint32_t SratomDumperFlags;

/// Dumper that writes atoms to a statement sink
typedef struct SratomDumperImpl SratomDumper;

/**
   Create a new atom dumper.

   @param world Serd world.
   @param map URID mapper, only used during construction.
   @param unmap URID unmapper, used while dumping to write URI strings.

   @return A newly allocated object which must be freed with
   sratom_dumper_free().
*/
SRATOM_API
SratomDumper*
sratom_dumper_new(SerdWorld* world, LV2_URID_Map* map, LV2_URID_Unmap* unmap);

/// Free an atom dumper created with sratom_dumper_new()
SRATOM_API
void
sratom_dumper_free(SratomDumper* dumper);

/**
   Write an atom body to a statement sink.

   This is the fundamental write function that writes an atom to a sink as a
   series of statements.  It can be used with any sink, such as a Turtle
   writer, model inserter, or a custom sink provided by the application.

   This function takes the `type`, `size`, and `body` separately to enable
   writing values that are not a contiguous `LV2_Atom` in memory.  For writing
   existing atoms, the simpler sratom_dump_atom() can be used instead.

   A subject and predicate can be provided so that writing simple atoms will
   produce a statement like `subject predicate "literal"`.  If either `subject`
   or `predicate` are null, objects will be written as the subject, and
   literals will be written as the only element of an anonymous list.  For
   example:

   @verbatim
   my:object some:property 42 .
   [ some:property 42 ] .
   ( "literal" ) .
   @endverbatim

   Generally, this function is intended for writing the value of some property
   (for example, the current value of a plugin parameter in a preset), and the
   appropriate subject and predicate for the context should always be provided.
   This avoids any ambiguities and guarantees lossless round-tripping.

   @param dumper Dumper instance.
   @param env Environment defining the base URI and any prefixes.
   @param sink Sink which receives the statements describing the atom.
   @param subject Subject of first statement, or NULL.
   @param predicate Predicate of first statement, or NULL.
   @param type Type of the atom.
   @param size Size of the atom body in bytes.
   @param body Atom body.
   @param flags Option flags.
   @return Zero on success.
*/
SRATOM_API
SratomStatus
sratom_dump(SratomDumper*     dumper,
            const SerdEnv*    env,
            const SerdSink*   sink,
            const SerdNode*   subject,
            const SerdNode*   predicate,
            LV2_URID          type,
            uint32_t          size,
            const void*       body,
            SratomDumperFlags flags);

/**
   Write an atom to a statement sink

   Convenience wrapper that takes a pointer to a complete atom, see the
   sratom_dump() documentation for details.

   @param dumper Dumper instance.
   @param env Environment defining the base URI and any prefixes.
   @param sink Sink which receives the statements describing the atom.
   @param subject Subject of first statement, or NULL.
   @param predicate Predicate of first statement, or NULL.
   @param atom Atom to write.
   @param flags Option flags.
   @return Zero on success.
*/
SRATOM_API
SratomStatus
sratom_dump_atom(SratomDumper*     dumper,
                 const SerdEnv*    env,
                 const SerdSink*   sink,
                 const SerdNode*   subject,
                 const SerdNode*   predicate,
                 const LV2_Atom*   atom,
                 SratomDumperFlags flags);

/**
   Write an atom as a string.

   The returned string can be forged back into an atom using
   sratom_from_string().

   @param dumper Dumper instance.
   @param env Environment for namespaces and relative URIs.
   @param atom Atom to write.
   @param flags Option flags.
   @return A string that must be freed using sratom_free(), or NULL on error.
*/
SRATOM_API
char*
sratom_to_string(SratomDumper*     dumper,
                 const SerdEnv*    env,
                 const LV2_Atom*   atom,
                 SratomDumperFlags flags);

/**
   @}
   @defgroup sratom_loading Loading Atoms
   @{
*/

/// Atom loader that reads atoms from a document
typedef struct SratomLoaderImpl SratomLoader;

/**
   Create a new loader for forging atoms from a document.

   @param world RDF world.
   @param map URID mapper.
   @return A new object that must be freed with sratom_loader_free().
*/
SRATOM_API
SratomLoader*
sratom_loader_new(SerdWorld* world, LV2_URID_Map* map);

/// Free an atom loader created with sratom_loader_new()
SRATOM_API
void
sratom_loader_free(SratomLoader* loader);

/**
   Load an atom from a model by forging the binary representation.

   This reads `node` in `model` as an atom, constructing the result with
   `forge`.

   @param loader Loader set up with the appropriate world and URID map.

   @param base_uri Base URI for making relative URI references.  This is
   typically used with file URIs to construct atoms with relative paths.  Any
   URI within the given base URI will be written to `forge` as a relative URI
   reference.  For example, with base URI <file:///my/data/>, the URI
   <file:///my/data/dir/file.txt> will be written to the forge as the path
   "dir/file.txt".

   @param forge Atom forge where the result is written.

   @param model Model that contains a description of the atom.

   @param node Node for the atom.  This can be a simple literal node for
   primitive atoms, or the subject resource for more complex structures like
   objects and vectors.

   @return A status code which is zero on success.
*/
SRATOM_API
SratomStatus
sratom_load(SratomLoader*    loader,
            const SerdNode*  base_uri,
            LV2_Atom_Forge*  forge,
            const SerdModel* model,
            const SerdNode*  node);

/**
   Load an Atom from a Turtle string.

   The returned atom must be free()'d by the caller.
*/
SRATOM_API
LV2_Atom*
sratom_from_string(SratomLoader* loader, SerdEnv* env, const char* str);

/**
   Load an Atom from a model.

   The returned atom must be free()'d by the caller.
*/
SRATOM_API
LV2_Atom*
sratom_from_model(SratomLoader*    loader,
                  const SerdNode*  base_uri,
                  const SerdModel* model,
                  const SerdNode*  subject);

/**
   @}
   @}
*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SRATOM_SRATOM_H */
