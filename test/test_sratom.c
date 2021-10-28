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

#include "test_utils.h"

#include "sratom/sratom.h"

#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2/atom/util.h"
#include "lv2/midi/midi.h"
#include "lv2/urid/urid.h"
#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_ATOM "http://lv2plug.in/ns/ext/atom#"
#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

static int
check_round_trip(Uris*                   uris,
                 SerdEnv*                env,
                 SratomDumper*           dumper,
                 SratomLoader*           loader,
                 const LV2_Atom*         obj,
                 const SratomDumperFlags flags)
{
  (void)uris; // FIXME

  // Serialise atom and print string
  char* const outstr = sratom_to_string(dumper, env, obj, flags);
  assert(outstr);

  printf("%s\n", outstr);

  // Parse serialised string back into an atom
  LV2_Atom* const parsed = sratom_from_string(loader, env, outstr);
  assert(parsed);

  if (!(flags & SRATOM_PRETTY_NUMBERS)) {
    // Check that round tripped atom is identical to original
    assert(obj->type == parsed->type);
    assert(lv2_atom_equals(obj, parsed));

    /* char* const instr = sratom_to_string(writer, env, parsed, flags); */
    /* if ( */
    /* printf("# Turtle => Atom\n\n%s", instr); */
  }

  sratom_free(parsed);
  sratom_free(outstr);

  return 0;
}

static int
test(SerdEnv* env, const char* name, const SratomDumperFlags flags)
{
  Uris           uris  = {NULL, 0};
  LV2_URID_Map   map   = {&uris, urid_map};
  LV2_URID_Unmap unmap = {&uris, urid_unmap};
  LV2_Atom_Forge forge;
  lv2_atom_forge_init(&forge, &map);

  SerdWorld*    world  = serd_world_new(NULL);
  SratomDumper* dumper = sratom_dumper_new(world, &map, &unmap);
  SratomLoader* loader = sratom_loader_new(world, &map);

  LV2_URID eg_Object  = urid_map(&uris, "http://example.org/Object");
  LV2_URID eg_one     = urid_map(&uris, "http://example.org/a-one");
  LV2_URID eg_two     = urid_map(&uris, "http://example.org/b-two");
  LV2_URID eg_three   = urid_map(&uris, "http://example.org/c-three");
  LV2_URID eg_four    = urid_map(&uris, "http://example.org/d-four");
  LV2_URID eg_true    = urid_map(&uris, "http://example.org/e-true");
  LV2_URID eg_false   = urid_map(&uris, "http://example.org/f-false");
  LV2_URID eg_path    = urid_map(&uris, "http://example.org/g-path");
  LV2_URID eg_winpath = urid_map(&uris, "http://example.org/h-winpath");
  LV2_URID eg_relpath = urid_map(&uris, "http://example.org/i-relpath");
  LV2_URID eg_urid    = urid_map(&uris, "http://example.org/j-urid");
  LV2_URID eg_string  = urid_map(&uris, "http://example.org/k-string");
  LV2_URID eg_langlit = urid_map(&uris, "http://example.org/l-langlit");
  LV2_URID eg_typelit = urid_map(&uris, "http://example.org/m-typelit");
  LV2_URID eg_null    = urid_map(&uris, "http://example.org/n-null");
  LV2_URID eg_chunk   = urid_map(&uris, "http://example.org/o-chunk");
  LV2_URID eg_blob    = urid_map(&uris, "http://example.org/p-blob");
  LV2_URID eg_blank   = urid_map(&uris, "http://example.org/q-blank");
  LV2_URID eg_tuple   = urid_map(&uris, "http://example.org/r-tuple");
  LV2_URID eg_rectup  = urid_map(&uris, "http://example.org/s-rectup");
  LV2_URID eg_ivector = urid_map(&uris, "http://example.org/t-ivector");
  LV2_URID eg_lvector = urid_map(&uris, "http://example.org/u-lvector");
  LV2_URID eg_fvector = urid_map(&uris, "http://example.org/v-fvector");
  LV2_URID eg_dvector = urid_map(&uris, "http://example.org/w-dvector");
  LV2_URID eg_bvector = urid_map(&uris, "http://example.org/x-bvector");
  LV2_URID eg_fseq    = urid_map(&uris, "http://example.org/y-fseq");
  LV2_URID eg_bseq    = urid_map(&uris, "http://example.org/z-bseq");

  uint8_t buf[1024];
  lv2_atom_forge_set_buffer(&forge, buf, sizeof(buf));

  LV2_Atom_Forge_Frame obj_frame;
  lv2_atom_forge_object(&forge, &obj_frame, 0, eg_Object);
  LV2_Atom* obj = lv2_atom_forge_deref(&forge, obj_frame.ref);

  // eg_one = (Int32)1
  lv2_atom_forge_key(&forge, eg_one);
  lv2_atom_forge_int(&forge, 1);

  // eg_two = (Int64)2
  lv2_atom_forge_key(&forge, eg_two);
  lv2_atom_forge_long(&forge, 2);

  // eg_three = (Float)3.0
  lv2_atom_forge_key(&forge, eg_three);
  lv2_atom_forge_float(&forge, 3.0f);

  // eg_four = (Double)4.0
  lv2_atom_forge_key(&forge, eg_four);
  lv2_atom_forge_double(&forge, 4.0);

  // eg_true = (Bool)1
  lv2_atom_forge_key(&forge, eg_true);
  lv2_atom_forge_bool(&forge, true);

  // eg_false = (Bool)0
  lv2_atom_forge_key(&forge, eg_false);
  lv2_atom_forge_bool(&forge, false);

  // eg_path = (Path)"/absolute/path"
  const char*  pstr     = "/absolute/path";
  const size_t pstr_len = strlen(pstr);
  lv2_atom_forge_key(&forge, eg_path);
  lv2_atom_forge_path(&forge, pstr, pstr_len);

  // eg_winpath = (Path)"C:/Weird/File System"
  const char*  wpstr     = "C:/Weird/File System";
  const size_t wpstr_len = strlen(wpstr);
  lv2_atom_forge_key(&forge, eg_winpath);
  lv2_atom_forge_path(&forge, wpstr, wpstr_len);

  // eg_relpath = (Path)"foo/bar"
  const char*  rpstr     = "foo/bar";
  const size_t rpstr_len = strlen(rpstr);
  lv2_atom_forge_key(&forge, eg_relpath);
  lv2_atom_forge_path(&forge, rpstr, rpstr_len);

  // eg_urid = (URID)"http://example.org/value"
  LV2_URID eg_value = urid_map(&uris, "http://example.org/value");
  lv2_atom_forge_key(&forge, eg_urid);
  lv2_atom_forge_urid(&forge, eg_value);

  // eg_string = (String)"hello"
  lv2_atom_forge_key(&forge, eg_string);
  lv2_atom_forge_string(&forge, "hello", strlen("hello"));

  // eg_langlit = (Literal)"ni hao"@cmn (but in actual mandarin)
  const uint8_t ni_hao[] = {0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD};
  lv2_atom_forge_key(&forge, eg_langlit);
  lv2_atom_forge_literal(&forge,
                         (const char*)ni_hao,
                         6,
                         0,
                         urid_map(&uris, "http://lexvo.org/id/iso639-3/cmn"));

  // eg_typelit = (Literal)"value"^^<http://example.org/Type>
  lv2_atom_forge_key(&forge, eg_typelit);
  lv2_atom_forge_literal(&forge,
                         "value",
                         strlen("value"),
                         urid_map(&uris, "http://example.org/Type"),
                         0);

  // eg_null = null
  lv2_atom_forge_key(&forge, eg_null);
  lv2_atom_forge_atom(&forge, 0, 0);

  // eg_chunk = 0xBEEFDEAD
  uint8_t chunk_buf[] = {0xBE, 0xEF, 0xDE, 0xAD};
  lv2_atom_forge_key(&forge, eg_chunk);
  lv2_atom_forge_atom(&forge, sizeof(chunk_buf), forge.Chunk);
  lv2_atom_forge_write(&forge, chunk_buf, sizeof(chunk_buf));

  // eg_blob = 0xDEADBEEF
  uint32_t blob_type  = map.map(map.handle, "http://example.org/Blob");
  uint8_t  blob_buf[] = {0xDE, 0xAD, 0xBE, 0xEF};
  lv2_atom_forge_key(&forge, eg_blob);
  lv2_atom_forge_atom(&forge, sizeof(blob_buf), blob_type);
  lv2_atom_forge_write(&forge, blob_buf, sizeof(blob_buf));

  // eg_blank = [ a eg:Object ; blank [ a eg:Object] ]
  lv2_atom_forge_key(&forge, eg_blank);
  LV2_Atom_Forge_Frame blank_frame;
  lv2_atom_forge_object(&forge, &blank_frame, 0, eg_Object);
  lv2_atom_forge_key(&forge, eg_blank);
  LV2_Atom_Forge_Frame sub_blank_frame;
  lv2_atom_forge_object(&forge, &sub_blank_frame, 0, eg_Object);
  lv2_atom_forge_pop(&forge, &sub_blank_frame);
  lv2_atom_forge_pop(&forge, &blank_frame);

  // eg_tuple = "foo",true
  lv2_atom_forge_key(&forge, eg_tuple);
  LV2_Atom_Forge_Frame tuple_frame;
  lv2_atom_forge_tuple(&forge, &tuple_frame);
  lv2_atom_forge_string(&forge, "foo", strlen("foo"));
  lv2_atom_forge_bool(&forge, true);
  lv2_atom_forge_pop(&forge, &tuple_frame);

  // eg_rectup = "foo",true,("bar",false)
  lv2_atom_forge_key(&forge, eg_rectup);
  LV2_Atom_Forge_Frame rectup_frame;
  lv2_atom_forge_tuple(&forge, &rectup_frame);
  lv2_atom_forge_string(&forge, "foo", strlen("foo"));
  lv2_atom_forge_bool(&forge, true);
  LV2_Atom_Forge_Frame subrectup_frame;
  lv2_atom_forge_tuple(&forge, &subrectup_frame);
  lv2_atom_forge_string(&forge, "bar", strlen("bar"));
  lv2_atom_forge_bool(&forge, false);
  lv2_atom_forge_pop(&forge, &subrectup_frame);
  lv2_atom_forge_pop(&forge, &rectup_frame);

  // eg_ivector = (Vector<Int>)1,2,3,4,5
  lv2_atom_forge_key(&forge, eg_ivector);
  int32_t ielems[] = {1, 2, 3, 4, 5};
  lv2_atom_forge_vector(&forge, sizeof(int32_t), forge.Int, 5, ielems);

  // eg_lvector = (Vector<Long>)1,2,3,4
  lv2_atom_forge_key(&forge, eg_lvector);
  int64_t lelems[] = {1, 2, 3, 4};
  lv2_atom_forge_vector(&forge, sizeof(int64_t), forge.Long, 4, lelems);

  // eg_fvector = (Vector<Float>)1.0,2.0,3.0,4.0,5.0
  lv2_atom_forge_key(&forge, eg_fvector);
  float felems[] = {1, 2, 3, 4, 5};
  lv2_atom_forge_vector(&forge, sizeof(float), forge.Float, 5, felems);

  // eg_dvector = (Vector<Double>)1.0,2.0,3.0,4.0
  lv2_atom_forge_key(&forge, eg_dvector);
  double delems[] = {1, 2, 3, 4};
  lv2_atom_forge_vector(&forge, sizeof(double), forge.Double, 4, delems);

  // eg_bvector = (Vector<Bool>)1,0,1
  lv2_atom_forge_key(&forge, eg_bvector);
  int32_t belems[] = {true, false, true};
  lv2_atom_forge_vector(&forge, sizeof(int32_t), forge.Bool, 3, belems);

  // eg_fseq = (Sequence)1, 2
  LV2_URID midi_midiEvent = map.map(map.handle, LV2_MIDI__MidiEvent);
  lv2_atom_forge_key(&forge, eg_fseq);
  LV2_Atom_Forge_Frame fseq_frame;
  lv2_atom_forge_sequence_head(&forge, &fseq_frame, 0);

  const uint8_t ev1[3] = {0x90, 0x1A, 0x1};
  lv2_atom_forge_frame_time(&forge, 1);
  lv2_atom_forge_atom(&forge, sizeof(ev1), midi_midiEvent);
  lv2_atom_forge_raw(&forge, ev1, sizeof(ev1));
  lv2_atom_forge_pad(&forge, sizeof(ev1));

  const uint8_t ev2[3] = {0x90, 0x2B, 0x2};
  lv2_atom_forge_frame_time(&forge, 3);
  lv2_atom_forge_atom(&forge, sizeof(ev2), midi_midiEvent);
  lv2_atom_forge_raw(&forge, ev2, sizeof(ev2));
  lv2_atom_forge_pad(&forge, sizeof(ev2));

  lv2_atom_forge_pop(&forge, &fseq_frame);

  // eg_bseq = (Sequence)1.1, 2.2
  LV2_URID atom_beatTime = map.map(map.handle, LV2_ATOM__beatTime);
  lv2_atom_forge_key(&forge, eg_bseq);
  LV2_Atom_Forge_Frame bseq_frame;
  lv2_atom_forge_sequence_head(&forge, &bseq_frame, atom_beatTime);

  lv2_atom_forge_beat_time(&forge, 1.0);
  lv2_atom_forge_atom(&forge, sizeof(ev1), midi_midiEvent);
  lv2_atom_forge_raw(&forge, ev1, sizeof(ev1));
  lv2_atom_forge_pad(&forge, sizeof(ev1));

  lv2_atom_forge_beat_time(&forge, 2.0);
  lv2_atom_forge_atom(&forge, sizeof(ev2), midi_midiEvent);
  lv2_atom_forge_raw(&forge, ev2, sizeof(ev2));
  lv2_atom_forge_pad(&forge, sizeof(ev2));

  lv2_atom_forge_pop(&forge, &bseq_frame);

  lv2_atom_forge_pop(&forge, &obj_frame);

  printf("\n# %s\n\n", name);
  check_round_trip(&uris, env, dumper, loader, obj, flags);
  printf("\n");
  LV2_ATOM_OBJECT_FOREACH ((LV2_Atom_Object*)obj, prop) {
    check_round_trip(&uris, env, dumper, loader, &prop->value, flags);
  }

  sratom_dumper_free(dumper);
  sratom_loader_free(loader);
  for (uint32_t i = 0; i < uris.n_uris; ++i) {
    free(uris.uris[i]);
  }
  serd_world_free(world);

  free(uris.uris);

  return 0;
}

int
main(void)
{
  SerdWorld* const world = serd_world_new(NULL);
  SerdEnv* const   env = serd_env_new(world, SERD_STRING("file:///tmp/base/"));

  serd_env_set_prefix(
    env, SERD_STRING("eg"), SERD_STRING("http://example.org/"));

  serd_env_set_prefix(env, SERD_STRING("atom"), SERD_STRING(NS_ATOM));
  serd_env_set_prefix(env, SERD_STRING("rdf"), SERD_STRING(NS_RDF));
  serd_env_set_prefix(env, SERD_STRING("xsd"), SERD_STRING(NS_XSD));

  const int st =
    (test(env, "Default", 0) || //
     test(env, "Pretty", SRATOM_PRETTY_NUMBERS) ||
     test(env, "Terse", SRATOM_TERSE) ||
     test(env, "Pretty + Terse", SRATOM_PRETTY_NUMBERS | SRATOM_TERSE));

  serd_env_free(env);

  return st;
}
