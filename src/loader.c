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

#include "sratom/sratom.h"

#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2/midi/midi.h"
#include "lv2/urid/urid.h"
#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

#define LOAD_ERROR(msg) \
  serd_world_logf(loader->world, "sratom", SERD_LOG_LEVEL_ERROR, 0, NULL, msg);

typedef enum { MODE_SUBJECT, MODE_BODY, MODE_SEQUENCE } ReadMode;

struct SratomLoaderImpl {
  LV2_URID_Map*  map;
  LV2_Atom_Forge forge;
  SerdWorld*     world;
  LV2_URID       atom_frameTime;
  LV2_URID       atom_beatTime;
  LV2_URID       midi_MidiEvent;
  struct {
    const SerdNode* atom_beatTime;
    const SerdNode* atom_childType;
    const SerdNode* atom_frameTime;
    const SerdNode* rdf_first;
    const SerdNode* rdf_rest;
    const SerdNode* rdf_type;
    const SerdNode* rdf_value;
    const SerdNode* xsd_base64Binary;
  } nodes;
};

typedef struct {
  SratomLoader*   loader;
  const SerdNode* base_uri;
  LV2_URID        seq_unit;
} LoadContext;

static void
read_node(LoadContext*     ctx,
          LV2_Atom_Forge*  forge,
          const SerdModel* model,
          const SerdNode*  node,
          ReadMode         mode);

SratomLoader*
sratom_loader_new(SerdWorld* world, LV2_URID_Map* map)
{
  SratomLoader* sratom = (SratomLoader*)calloc(1, sizeof(SratomLoader));
  if (!sratom) {
    return NULL;
  }

  sratom->world          = world;
  sratom->map            = map;
  sratom->atom_frameTime = map->map(map->handle, LV2_ATOM__frameTime);
  sratom->atom_beatTime  = map->map(map->handle, LV2_ATOM__beatTime);
  sratom->midi_MidiEvent = map->map(map->handle, LV2_MIDI__MidiEvent);
  lv2_atom_forge_init(&sratom->forge, map);

#define MANAGE_URI(uri)                      \
  serd_nodes_manage(serd_world_nodes(world), \
                    serd_new_uri(SERD_STATIC_STRING(uri)))

  sratom->nodes.atom_beatTime    = MANAGE_URI(LV2_ATOM__beatTime);
  sratom->nodes.atom_childType   = MANAGE_URI(LV2_ATOM__childType);
  sratom->nodes.atom_frameTime   = MANAGE_URI(LV2_ATOM__frameTime);
  sratom->nodes.rdf_first        = MANAGE_URI(NS_RDF "first");
  sratom->nodes.rdf_rest         = MANAGE_URI(NS_RDF "rest");
  sratom->nodes.rdf_type         = MANAGE_URI(NS_RDF "type");
  sratom->nodes.rdf_value        = MANAGE_URI(NS_RDF "value");
  sratom->nodes.xsd_base64Binary = MANAGE_URI(NS_XSD "base64Binary");

#undef INTERN_URI

  return sratom;
}

void
sratom_loader_free(SratomLoader* loader)
{
  free(loader);
}

static void
read_list_value(LoadContext*     ctx,
                LV2_Atom_Forge*  forge,
                const SerdModel* model,
                const SerdNode*  node,
                ReadMode         mode)
{
  const SerdNode* fst =
    serd_model_get(model, node, ctx->loader->nodes.rdf_first, NULL, NULL);
  const SerdNode* rst =
    serd_model_get(model, node, ctx->loader->nodes.rdf_rest, NULL, NULL);
  if (fst && rst) {
    read_node(ctx, forge, model, fst, mode);
    read_list_value(ctx, forge, model, rst, mode);
  }
}

static void
read_resource(LoadContext*     ctx,
              LV2_Atom_Forge*  forge,
              const SerdModel* model,
              const SerdNode*  node,
              LV2_URID         otype)
{
  LV2_URID_Map* map = ctx->loader->map;
  SerdRange*    r   = serd_model_range(model, node, NULL, NULL, NULL);
  for (; !serd_range_empty(r); serd_range_next(r)) {
    const SerdStatement* match = serd_range_front(r);
    const SerdNode*      p     = serd_statement_predicate(match);
    const SerdNode*      o     = serd_statement_object(match);
    if (p) {
      const char* p_uri  = serd_node_string(p);
      uint32_t    p_urid = map->map(map->handle, p_uri);
      if (!(serd_node_equals(p, ctx->loader->nodes.rdf_type) &&
            serd_node_type(o) == SERD_URI &&
            map->map(map->handle, serd_node_string(o)) == otype)) {
        lv2_atom_forge_key(forge, p_urid);
        read_node(ctx, forge, model, o, MODE_BODY);
      }
    }
  }
  serd_range_free(r);
}

static uint32_t
atom_size(SratomLoader* loader, uint32_t type_urid)
{
  if (type_urid == loader->forge.Int) {
    return sizeof(int32_t);
  } else if (type_urid == loader->forge.Long) {
    return sizeof(int64_t);
  } else if (type_urid == loader->forge.Float) {
    return sizeof(float);
  } else if (type_urid == loader->forge.Double) {
    return sizeof(double);
  } else if (type_urid == loader->forge.Bool) {
    return sizeof(int32_t);
  } else if (type_urid == loader->forge.URID) {
    return sizeof(uint32_t);
  }
  return 0;
}

static void
read_uri(LoadContext* ctx, LV2_Atom_Forge* forge, const SerdNode* node)
{
  SratomLoader* const loader = ctx->loader;
  LV2_URID_Map* const map    = loader->map;
  const char* const   str    = serd_node_string(node);

  if (!strcmp(str, NS_RDF "nil")) {
    lv2_atom_forge_atom(forge, 0, 0);
  } else if (!strncmp(str, "file://", 7)) {
    const SerdURIView base_uri = serd_node_uri_view(ctx->base_uri);
    const SerdURIView uri      = serd_parse_uri(str);
    if (serd_uri_is_within(uri, base_uri)) {
      SerdNode* const rel = serd_new_parsed_uri(serd_relative_uri(
        serd_parse_uri(str), serd_node_uri_view(ctx->base_uri)));

      char* path = serd_parse_file_uri(serd_node_string(rel), NULL);
      lv2_atom_forge_path(forge, path, strlen(path));
      serd_free(path);
      serd_node_free(rel);
    } else {
      char* const path = serd_parse_file_uri(str, NULL);
      lv2_atom_forge_path(forge, path, strlen(path));
      serd_free(path);
    }
  } else {
    lv2_atom_forge_urid(forge, map->map(map->handle, str));
  }
}

static void
read_typed_literal(SratomLoader* const   loader,
                   LV2_Atom_Forge* const forge,
                   const SerdNode* const node,
                   const SerdNode* const datatype)
{
  const char* const str      = serd_node_string(node);
  const size_t      len      = serd_node_length(node);
  const char* const type_uri = serd_node_string(datatype);

  if (!strcmp(type_uri, NS_XSD "int") || !strcmp(type_uri, NS_XSD "integer")) {
    lv2_atom_forge_int(forge, strtol(str, NULL, 10));
  } else if (!strcmp(type_uri, NS_XSD "long")) {
    lv2_atom_forge_long(forge, strtol(str, NULL, 10));
  } else if (!strcmp(type_uri, NS_XSD "float") ||
             !strcmp(type_uri, NS_XSD "decimal")) {
    lv2_atom_forge_float(forge, serd_get_float(node));
  } else if (!strcmp(type_uri, NS_XSD "double")) {
    lv2_atom_forge_double(forge, serd_get_double(node));
  } else if (!strcmp(type_uri, NS_XSD "boolean")) {
    lv2_atom_forge_bool(forge, serd_get_boolean(node));
  } else if (!strcmp(type_uri, NS_XSD "base64Binary")) {
    size_t size = 0;
    void*  body = serd_base64_decode(str, len, &size);
    lv2_atom_forge_atom(forge, size, forge->Chunk);
    lv2_atom_forge_write(forge, body, size);
    free(body);
  } else if (!strcmp(type_uri, LV2_ATOM__Path)) {
    lv2_atom_forge_path(forge, str, len);
  } else if (!strcmp(type_uri, LV2_MIDI__MidiEvent)) {
    lv2_atom_forge_atom(forge, len / 2, loader->midi_MidiEvent);
    for (const char* s = str; s < str + len; s += 2) {
      unsigned num;
      sscanf(s, "%2X", &num);
      const uint8_t c = num;
      lv2_atom_forge_raw(forge, &c, 1);
    }
    lv2_atom_forge_pad(forge, len / 2);
  } else {
    lv2_atom_forge_literal(
      forge, str, len, loader->map->map(loader->map->handle, type_uri), 0);
  }
}

static void
read_plain_literal(SratomLoader* const   loader,
                   LV2_Atom_Forge* const forge,
                   const SerdNode* const node,
                   const SerdNode* const language)
{
  const char* const str      = serd_node_string(node);
  const size_t      len      = serd_node_length(node);
  const char*       lang_str = serd_node_string(language);
  const char*       prefix   = "http://lexvo.org/id/iso639-3/";
  const size_t      lang_len = strlen(prefix) + strlen(lang_str);
  char* const       lang_uri = (char*)calloc(lang_len + 1, 1);

  snprintf(lang_uri, lang_len + 1, "%s%s", prefix, lang_str);

  lv2_atom_forge_literal(
    forge, str, len, 0, loader->map->map(loader->map->handle, lang_uri));

  free(lang_uri);
}

static void
read_literal(SratomLoader* loader, LV2_Atom_Forge* forge, const SerdNode* node)
{
  assert(serd_node_type(node) == SERD_LITERAL);

  const SerdNode* const datatype = serd_node_datatype(node);
  const SerdNode* const language = serd_node_language(node);
  if (datatype) {
    read_typed_literal(loader, forge, node, datatype);
  } else if (language) {
    read_plain_literal(loader, forge, node, language);
  } else {
    lv2_atom_forge_string(
      forge, serd_node_string(node), serd_node_length(node));
  }
}

static void
read_object(LoadContext*     ctx,
            LV2_Atom_Forge*  forge,
            const SerdModel* model,
            const SerdNode*  node,
            ReadMode         mode)
{
  SratomLoader* const loader = ctx->loader;
  LV2_URID_Map* const map    = loader->map;
  const char* const   str    = serd_node_string(node);

  const SerdNode* const type =
    serd_model_get(model, node, loader->nodes.rdf_type, NULL, NULL);

  const SerdNode* const value =
    serd_model_get(model, node, loader->nodes.rdf_value, NULL, NULL);

  const char* type_uri  = NULL;
  uint32_t    type_urid = 0;
  if (type) {
    type_uri  = serd_node_string(type);
    type_urid = map->map(map->handle, type_uri);
  }

  LV2_Atom_Forge_Frame frame = {0, 0};
  if (mode == MODE_SEQUENCE) {
    LV2_URID        seq_unit = 0u;
    const SerdNode* time     = NULL;

    if ((time = serd_model_get(
           model, node, loader->nodes.atom_frameTime, NULL, NULL))) {
      lv2_atom_forge_frame_time(forge, time ? serd_get_integer(time) : 0);
      seq_unit = loader->atom_frameTime;

    } else if ((time = serd_model_get(
                  model, node, loader->nodes.atom_beatTime, NULL, NULL))) {
      lv2_atom_forge_beat_time(forge, serd_get_double(time));
      seq_unit = loader->atom_beatTime;
    }

    read_node(ctx, forge, model, value, MODE_BODY);
    ctx->seq_unit = seq_unit;

  } else if (type_urid == loader->forge.Tuple) {
    lv2_atom_forge_tuple(forge, &frame);
    read_list_value(ctx, forge, model, value, MODE_BODY);

  } else if (type_urid == loader->forge.Sequence) {
    const LV2_Atom_Forge_Ref ref =
      lv2_atom_forge_sequence_head(forge, &frame, 0);
    ctx->seq_unit = 0;
    read_list_value(ctx, forge, model, value, MODE_SEQUENCE);

    LV2_Atom_Sequence* seq =
      (LV2_Atom_Sequence*)lv2_atom_forge_deref(forge, ref);
    seq->body.unit =
      ((ctx->seq_unit == loader->atom_frameTime) ? 0 : ctx->seq_unit);

  } else if (type_urid == loader->forge.Vector) {
    const SerdNode* child_type_node =
      serd_model_get(model, node, loader->nodes.atom_childType, NULL, NULL);
    uint32_t child_type =
      map->map(map->handle, serd_node_string(child_type_node));
    uint32_t child_size = atom_size(loader, child_type);
    if (child_size > 0) {
      LV2_Atom_Forge_Ref ref =
        lv2_atom_forge_vector_head(forge, &frame, child_size, child_type);
      read_list_value(ctx, forge, model, value, MODE_BODY);
      lv2_atom_forge_pop(forge, &frame);
      frame.ref = 0;
      lv2_atom_forge_pad(forge, lv2_atom_forge_deref(forge, ref)->size);
    }

  } else if (value && serd_node_equals(serd_node_datatype(value),
                                       loader->nodes.xsd_base64Binary)) {
    const char*  vstr = serd_node_string(value);
    const size_t vlen = serd_node_length(value);
    size_t       size = 0;
    void*        body = serd_base64_decode(vstr, vlen, &size);
    lv2_atom_forge_atom(forge, size, type_urid);
    lv2_atom_forge_write(forge, body, size);
    free(body);

  } else if (serd_node_type(node) == SERD_URI) {
    const LV2_URID urid = map->map(map->handle, str);
    if (serd_model_count(model, node, NULL, NULL, NULL)) {
      lv2_atom_forge_object(forge, &frame, urid, type_urid);
      read_resource(ctx, forge, model, node, type_urid);
    } else {
      read_uri(ctx, forge, node);
    }

  } else {
    lv2_atom_forge_object(forge, &frame, 0, type_urid);
    read_resource(ctx, forge, model, node, type_urid);
  }

  if (frame.ref) {
    lv2_atom_forge_pop(forge, &frame);
  }
}

static void
read_node(LoadContext*     ctx,
          LV2_Atom_Forge*  forge,
          const SerdModel* model,
          const SerdNode*  node,
          ReadMode         mode)
{
  if (serd_node_type(node) == SERD_LITERAL) {
    read_literal(ctx->loader, forge, node);
  } else if (serd_node_type(node) == SERD_URI && mode != MODE_SUBJECT) {
    read_uri(ctx, forge, node);
  } else {
    read_object(ctx, forge, model, node, mode);
  }
}

int
sratom_load(SratomLoader*    loader,
            const SerdNode*  base_uri,
            LV2_Atom_Forge*  forge,
            const SerdModel* model,
            const SerdNode*  node)
{
  LoadContext ctx = {loader, base_uri, 0};
  read_node(&ctx, forge, model, node, MODE_SUBJECT);
  return 0;
}

static SerdModel*
model_from_string(SratomLoader* loader, SerdEnv* env, const char* str)
{
  SerdModel* const model    = serd_model_new(loader->world, SERD_INDEX_SPO);
  SerdSink* const  inserter = serd_inserter_new(model, env, NULL);

  SerdNode* const       name   = serd_new_string(SERD_STATIC_STRING("string"));
  SerdByteSource* const source = serd_byte_source_new_string(str, name);
  SerdReader* const     reader = serd_reader_new(
    loader->world, SERD_TURTLE, SERD_READ_LAX, env, inserter, 4096);

  serd_reader_start(reader, source);

  const SerdStatus st = serd_reader_read_document(reader);

  serd_reader_free(reader);
  serd_byte_source_free(source);
  serd_node_free(name);
  serd_sink_free(inserter);

  if (st) {
    serd_model_free(model);
    return NULL;
  }

  return model;
}

LV2_Atom*
sratom_from_string(SratomLoader* const loader,
                   SerdEnv* const      env,
                   const char* const   str)
{
  SerdModel* model = model_from_string(loader, env, str);
  if (!model || serd_model_empty(model)) {
    LOAD_ERROR("Failed to read string into model");
    return NULL;
  }

  const SerdNode*      node  = NULL;
  SerdIter*            begin = serd_model_begin(model);
  const SerdStatement* s     = serd_iter_get(begin);
  if (serd_model_size(model) == 2 &&
      serd_node_type(serd_statement_subject(s)) == SERD_BLANK &&
      serd_node_equals(serd_statement_predicate(s), loader->nodes.rdf_first)) {
    // Special single-element list syntax for literals
    node = serd_statement_object(s);
  } else {
    node = serd_statement_subject(s);
  }

  if (!node) {
    LOAD_ERROR("Failed to find a node to parse");
    return NULL;
  }

  LV2_Atom* atom =
    sratom_from_model(loader, serd_env_base_uri(env), model, node);

  serd_iter_free(begin);
  serd_model_free(model);
  return atom;
}

static LV2_Atom_Forge_Ref
sratom_forge_sink(LV2_Atom_Forge_Sink_Handle handle,
                  const void*                buf,
                  uint32_t                   size)
{
  SerdBuffer*              chunk = (SerdBuffer*)handle;
  const LV2_Atom_Forge_Ref ref   = chunk->len + 1;
  serd_buffer_sink(buf, 1, size, chunk);
  return ref;
}

static LV2_Atom*
sratom_forge_deref(LV2_Atom_Forge_Sink_Handle handle, LV2_Atom_Forge_Ref ref)
{
  SerdBuffer* chunk = (SerdBuffer*)handle;
  return (LV2_Atom*)((char*)chunk->buf + ref - 1);
}

LV2_Atom*
sratom_from_model(SratomLoader*    loader,
                  const SerdNode*  base_uri,
                  const SerdModel* model,
                  const SerdNode*  subject)
{
  if (!subject) {
    return NULL;
  }

  SerdBuffer out = {NULL, 0};
  lv2_atom_forge_set_sink(
    &loader->forge, sratom_forge_sink, sratom_forge_deref, &out);

  int st = sratom_load(loader, base_uri, &loader->forge, model, subject);
  if (st) {
    sratom_free(out.buf);
    out.buf = NULL;
  }

  return (LV2_Atom*)out.buf;
}
