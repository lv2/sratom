// Copyright 2026 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "test_uri_map.h"

#include <lv2/atom/atom.h>
#include <lv2/urid/urid.h>
#include <serd/serd.h>
#include <sratom/sratom.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define NS_EG "http://example.org/"

#define USTR(s) ((const uint8_t*)(s))

static SerdStatus
on_statement(void* const              handle,
             const SerdStatementFlags flags,
             const SerdNode* const    graph,
             const SerdNode* const    subject,
             const SerdNode* const    predicate,
             const SerdNode* const    object,
             const SerdNode* const    object_datatype,
             const SerdNode* const    object_lang)
{
  (void)flags;
  (void)graph;
  (void)subject;
  (void)predicate;
  (void)object;
  (void)object_datatype;
  (void)object_lang;

  ++(*(unsigned*)handle);
  return SERD_SUCCESS;
}

static SerdStatus
on_end(void* const handle, const SerdNode* const node)
{
  (void)handle;
  (void)node;
  return SERD_SUCCESS;
}

static void
test_bad_vector_child_size(void)
{
  Uris           uris         = {NULL, 0};
  LV2_URID_Map   map          = {&uris, urid_map};
  LV2_URID_Unmap unmap        = {&uris, urid_unmap};
  const LV2_URID atom_Int     = map.map(map.handle, LV2_ATOM__Int);
  const LV2_URID atom_Vector  = map.map(map.handle, LV2_ATOM__Vector);
  unsigned       n_statements = 0U;
  Sratom* const  sratom       = sratom_new(&map);

  sratom_set_sink(sratom, NS_EG, on_statement, on_end, &n_statements);

  const SerdNode        s   = serd_node_from_string(SERD_URI, USTR(NS_EG "s"));
  const SerdNode        p   = serd_node_from_string(SERD_URI, USTR(NS_EG "p"));
  const LV2_Atom_Vector vec = {{sizeof(LV2_Atom_Vector), atom_Vector},
                               {0U, atom_Int}};

  assert(sratom_write(sratom,
                      &unmap,
                      0U,
                      &s,
                      &p,
                      vec.atom.type,
                      vec.atom.size,
                      LV2_ATOM_BODY(&vec)) == SERD_ERR_BAD_ARG);

  sratom_free(sratom);
  free_uris(&uris);
}

int
main(void)
{
  test_bad_vector_child_size();
  return 0;
}
