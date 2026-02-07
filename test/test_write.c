// Copyright 2026 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#undef NDEBUG

#include "test_uri_map.h"

#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>
#include <serd/serd.h>
#include <sratom/sratom.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NS_EG "http://example.org/"

#define USTR(s) ((const uint8_t*)(s))

typedef struct {
  unsigned fail_on_statement; ///< Index of statement to fail on
  unsigned n_statements;      ///< Number of statements so far
} WriteContext;

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

  WriteContext* const ctx = (WriteContext*)handle;

  return (ctx->n_statements++ == ctx->fail_on_statement) ? SERD_ERR_BAD_WRITE
                                                         : SERD_SUCCESS;
}

static SerdStatus
on_end(void* const handle, const SerdNode* const node)
{
  (void)handle;
  (void)node;
  return SERD_SUCCESS;
}

static void
check_turtle(Sratom* const         sratom,
             LV2_URID_Unmap* const unmap,
             const LV2_Atom* const atom,
             const char* const     expected)
{
  const SerdNode s = serd_node_from_string(SERD_URI, USTR(NS_EG "s"));
  const SerdNode p = serd_node_from_string(SERD_URI, USTR(NS_EG "p"));

  char* const ttl = sratom_to_turtle(
    sratom, unmap, NS_EG, &s, &p, atom->type, atom->size, LV2_ATOM_BODY(atom));

  assert(ttl);
  assert(!strcmp(ttl, expected));
  free(ttl);
}

// Not covered by round-trip test because it comes back as a string
static void
test_bare_literal(void)
{
  Uris           uris   = {NULL, 0};
  LV2_URID_Map   map    = {&uris, urid_map};
  LV2_URID_Unmap unmap  = {&uris, urid_unmap};
  Sratom* const  sratom = sratom_new(&map);

  LV2_Atom_Forge forge;
  LV2_Atom       buf[8];
  lv2_atom_forge_init(&forge, &map);
  lv2_atom_forge_set_buffer(&forge, (uint8_t*)buf, sizeof(buf));
  lv2_atom_forge_literal(&forge, "test", 4, 0, 0);

  check_turtle(sratom,
               &unmap,
               buf,
               "<http://example.org/s>\n\t<http://example.org/p> \"test\" .\n");

  sratom_free(sratom);
  free_uris(&uris);
}

// Not covered by round-trip test because it comes back as a URID
static void
test_uri(void)
{
  Uris           uris   = {NULL, 0};
  LV2_URID_Map   map    = {&uris, urid_map};
  LV2_URID_Unmap unmap  = {&uris, urid_unmap};
  Sratom* const  sratom = sratom_new(&map);

  LV2_Atom_Forge forge;
  LV2_Atom       buf[8];
  lv2_atom_forge_init(&forge, &map);
  lv2_atom_forge_set_buffer(&forge, (uint8_t*)buf, sizeof(buf));
  lv2_atom_forge_uri(&forge, NS_EG "o", strlen(NS_EG "o"));

  check_turtle(sratom,
               &unmap,
               buf,
               "<http://example.org/s>\n\t<http://example.org/p> "
               "<http://example.org/o> .\n");

  sratom_free(sratom);
  free_uris(&uris);
}

static void
test_bad_language(void)
{
  Uris           uris   = {NULL, 0};
  LV2_URID_Map   map    = {&uris, urid_map};
  LV2_URID_Unmap unmap  = {&uris, urid_unmap};
  const LV2_URID lang   = urid_map(&uris, NS_EG "l");
  WriteContext   ctx    = {0, 0};
  Sratom* const  sratom = sratom_new(&map);

  sratom_set_sink(sratom, NS_EG, on_statement, on_end, &ctx);

  LV2_Atom_Forge forge;
  LV2_Atom       buf[8];
  lv2_atom_forge_init(&forge, &map);
  lv2_atom_forge_set_buffer(&forge, (uint8_t*)buf, sizeof(buf));
  lv2_atom_forge_literal(&forge, "test", 4, 0, lang);

  const SerdNode        s   = serd_node_from_string(SERD_URI, USTR(NS_EG "s"));
  const SerdNode        p   = serd_node_from_string(SERD_URI, USTR(NS_EG "p"));
  const LV2_Atom* const obj = buf;
  assert(
    sratom_write(
      sratom, &unmap, 0U, &s, &p, obj->type, obj->size, LV2_ATOM_BODY(obj)) ==
    SERD_ERR_BAD_ARG);

  assert(!ctx.n_statements);
  sratom_free(sratom);
  free_uris(&uris);
}

static void
test_bad_vector_child_size(void)
{
  Uris           uris        = {NULL, 0};
  LV2_URID_Map   map         = {&uris, urid_map};
  LV2_URID_Unmap unmap       = {&uris, urid_unmap};
  const LV2_URID atom_Int    = map.map(map.handle, LV2_ATOM__Int);
  const LV2_URID atom_Vector = map.map(map.handle, LV2_ATOM__Vector);
  WriteContext   ctx         = {0, 0};
  Sratom* const  sratom      = sratom_new(&map);

  sratom_set_sink(sratom, NS_EG, on_statement, on_end, &ctx);

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

  assert(!ctx.n_statements);
  sratom_free(sratom);
  free_uris(&uris);
}

static void
test_write_errors(void)
{
  static const int32_t elems[] = {1, 2, 3, 4};

  Uris           uris   = {NULL, 0};
  LV2_URID_Map   map    = {&uris, urid_map};
  LV2_URID_Unmap unmap  = {&uris, urid_unmap};
  WriteContext   ctx    = {0, 0};
  Sratom* const  sratom = sratom_new(&map);

  sratom_set_sink(sratom, NS_EG, on_statement, on_end, &ctx);

  const LV2_URID eg_Blob        = urid_map(&uris, NS_EG "Blob");
  const LV2_URID eg_Object      = urid_map(&uris, NS_EG "Object");
  const LV2_URID eg_p           = urid_map(&uris, NS_EG "p");
  const LV2_URID midi_MidiEvent = urid_map(&uris, LV2_MIDI__MidiEvent);

  LV2_Atom_Forge       forge;
  LV2_Atom             buf[32];
  LV2_Atom_Forge_Frame obj_frame;
  lv2_atom_forge_init(&forge, &map);
  lv2_atom_forge_set_buffer(&forge, (uint8_t*)buf, sizeof(buf));
  lv2_atom_forge_object(&forge, &obj_frame, 0, eg_Object);

  lv2_atom_forge_key(&forge, eg_p);
  lv2_atom_forge_vector(&forge, sizeof(int32_t), forge.Int, 4, elems);

  lv2_atom_forge_key(&forge, eg_p);
  lv2_atom_forge_atom(&forge, sizeof(elems), eg_Blob);
  lv2_atom_forge_write(&forge, elems, sizeof(elems));

  LV2_Atom_Forge_Frame tup_frame;
  lv2_atom_forge_key(&forge, eg_p);
  lv2_atom_forge_tuple(&forge, &tup_frame);
  lv2_atom_forge_int(&forge, 2);
  lv2_atom_forge_pop(&forge, &tup_frame);

  LV2_Atom_Forge_Frame seq_frame;
  lv2_atom_forge_key(&forge, eg_p);
  lv2_atom_forge_sequence_head(&forge, &seq_frame, 0);
  const uint8_t ev1[3] = {0x90, 0x60, 0x60};
  lv2_atom_forge_frame_time(&forge, 1);
  lv2_atom_forge_atom(&forge, sizeof(ev1), midi_MidiEvent);
  lv2_atom_forge_raw(&forge, ev1, sizeof(ev1));
  lv2_atom_forge_pad(&forge, sizeof(ev1));
  lv2_atom_forge_pop(&forge, &seq_frame);

  lv2_atom_forge_pop(&forge, &obj_frame);

  const SerdNode s = serd_node_from_string(SERD_URI, USTR(NS_EG "s"));
  const SerdNode p = serd_node_from_string(SERD_URI, USTR(NS_EG "p"));

  for (unsigned i = 0U; i < 29; ++i) {
    ctx.fail_on_statement = i;
    ctx.n_statements      = 0;

    const int st = sratom_write(
      sratom, &unmap, 0U, &s, &p, buf->type, buf->size, LV2_ATOM_BODY(buf));

    assert(st == SERD_ERR_BAD_WRITE);
    assert(ctx.n_statements == ctx.fail_on_statement + 1U);
  }

  sratom_free(sratom);
  free_uris(&uris);
}

int
main(void)
{
  test_bare_literal();
  test_uri();
  test_bad_language();
  test_bad_vector_child_size();
  test_write_errors();
  return 0;
}
