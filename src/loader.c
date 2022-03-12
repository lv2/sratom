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

// IWYU pragma: no_forward_declare SratomLoaderImpl

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

#define LOAD_ERROR(msg) serd_logf(loader->world, SERD_LOG_LEVEL_ERROR, msg)

typedef enum { MODE_SUBJECT, MODE_BODY, MODE_SEQUENCE } ReadMode;

typedef struct {
  const SerdNode* atom_beatTime;
  const SerdNode* atom_childType;
  const SerdNode* atom_frameTime;
  const SerdNode* rdf_first;
  const SerdNode* rdf_rest;
  const SerdNode* rdf_type;
  const SerdNode* rdf_value;
  const SerdNode* xsd_base64Binary;
} LoaderNodes;

struct SratomLoaderImpl {
  LV2_URID_Map*  map;
  LV2_Atom_Forge forge;
  SerdWorld*     world;
  LV2_URID       atom_frameTime;
  LV2_URID       atom_beatTime;
  LV2_URID       midi_MidiEvent;
  LoaderNodes    nodes;
};

typedef struct {
  SratomLoader* SERD_NONNULL   loader;
  const SerdNode* SERD_NONNULL base_uri;
  LV2_URID                     seq_unit;
} LoadContext;

static SratomStatus
read_node(LoadContext*     ctx,
          LV2_Atom_Forge*  forge,
          const SerdModel* model,
          const SerdNode*  node,
          ReadMode         mode);

SratomLoader*
sratom_loader_new(SerdWorld* const world, LV2_URID_Map* const map)
{
  SratomLoader* const loader = (SratomLoader*)calloc(1, sizeof(SratomLoader));
  if (!loader) {
    return NULL;
  }

  loader->world          = world;
  loader->map            = map;
  loader->atom_frameTime = map->map(map->handle, LV2_ATOM__frameTime);
  loader->atom_beatTime  = map->map(map->handle, LV2_ATOM__beatTime);
  loader->midi_MidiEvent = map->map(map->handle, LV2_MIDI__MidiEvent);

  lv2_atom_forge_init(&loader->forge, map);

#define MANAGE_URI(uri) \
  serd_nodes_uri(serd_world_nodes(world), serd_string(uri))

  loader->nodes.atom_beatTime    = MANAGE_URI(LV2_ATOM__beatTime);
  loader->nodes.atom_childType   = MANAGE_URI(LV2_ATOM__childType);
  loader->nodes.atom_frameTime   = MANAGE_URI(LV2_ATOM__frameTime);
  loader->nodes.rdf_first        = MANAGE_URI(NS_RDF "first");
  loader->nodes.rdf_rest         = MANAGE_URI(NS_RDF "rest");
  loader->nodes.rdf_type         = MANAGE_URI(NS_RDF "type");
  loader->nodes.rdf_value        = MANAGE_URI(NS_RDF "value");
  loader->nodes.xsd_base64Binary = MANAGE_URI(NS_XSD "base64Binary");

#undef MANAGE_URI

  return loader;
}

void
sratom_loader_free(SratomLoader* const loader)
{
  free(loader);
}

static void
read_list_value(LoadContext* const     ctx,
                LV2_Atom_Forge* const  forge,
                const SerdModel* const model,
                const SerdNode* const  node,
                const ReadMode         mode)
{
  const SerdNode* const fst =
    serd_model_get(model, node, ctx->loader->nodes.rdf_first, NULL, NULL);

  const SerdNode* const rst =
    serd_model_get(model, node, ctx->loader->nodes.rdf_rest, NULL, NULL);

  if (fst && rst) {
    read_node(ctx, forge, model, fst, mode);
    read_list_value(ctx, forge, model, rst, mode);
  }
}

static void
read_resource(LoadContext* const     ctx,
              LV2_Atom_Forge* const  forge,
              const SerdModel* const model,
              const SerdNode* const  node,
              const LV2_URID         otype)
{
  LV2_URID_Map* const map = ctx->loader->map;
  SerdCursor* const   r   = serd_model_find(model, node, NULL, NULL, NULL);
  for (; !serd_cursor_is_end(r); serd_cursor_advance(r)) {
    const SerdStatement* match = serd_cursor_get(r);
    if (match) {
      const SerdNode* const p      = serd_statement_predicate(match);
      const SerdNode* const o      = serd_statement_object(match);
      const char* const     p_uri  = serd_node_string(p);
      const uint32_t        p_urid = map->map(map->handle, p_uri);
      if (!(serd_node_equals(p, ctx->loader->nodes.rdf_type) &&
            serd_node_type(o) == SERD_URI &&
            map->map(map->handle, serd_node_string(o)) == otype)) {
        lv2_atom_forge_key(forge, p_urid);
        read_node(ctx, forge, model, o, MODE_BODY);
      }
    }
  }
  serd_cursor_free(r);
}

static uint32_t
atom_size(SratomLoader* const loader, const uint32_t type_urid)
{
  if (type_urid == loader->forge.Int || type_urid == loader->forge.Bool) {
    return sizeof(int32_t);
  }

  if (type_urid == loader->forge.URID) {
    return sizeof(uint32_t);
  }

  if (type_urid == loader->forge.Long) {
    return sizeof(int64_t);
  }

  if (type_urid == loader->forge.Float) {
    return sizeof(float);
  }

  if (type_urid == loader->forge.Double) {
    return sizeof(double);
  }

  return 0;
}

static SratomStatus
read_uri(LoadContext* const    ctx,
         LV2_Atom_Forge* const forge,
         const SerdNode* const node)
{
  SratomLoader* const loader = ctx->loader;
  LV2_URID_Map* const map    = loader->map;
  const char* const   str    = serd_node_string(node);
  LV2_Atom_Forge_Ref  ref    = 0;

  if (!strcmp(str, NS_RDF "nil")) {
    ref = lv2_atom_forge_atom(forge, 0, 0);
  } else if (!strncmp(str, "file://", 7)) {
    const SerdURIView uri = serd_parse_uri(str);
    if (ctx->base_uri &&
        serd_uri_is_within(uri, serd_node_uri_view(ctx->base_uri))) {
      SerdNode* const rel = serd_new_parsed_uri(
        NULL,
        serd_relative_uri(serd_parse_uri(str),
                          serd_node_uri_view(ctx->base_uri)));

      char* const path = serd_parse_file_uri(NULL, serd_node_string(rel), NULL);
      if (!path) {
        return SRATOM_BAD_FILE_URI;
      }

      ref = lv2_atom_forge_path(forge, path, strlen(path));
      serd_free(NULL, path);
      serd_node_free(NULL, rel);
    } else {
      char* const path = serd_parse_file_uri(NULL, str, NULL);
      if (!path) {
        return SRATOM_BAD_FILE_URI;
      }

      ref = lv2_atom_forge_path(forge, path, strlen(path));
      serd_free(NULL, path);
    }
  } else {
    ref = lv2_atom_forge_urid(forge, map->map(map->handle, str));
  }

  return ref ? SRATOM_SUCCESS : SRATOM_BAD_FORGE;
}

static SratomStatus
read_typed_literal(SratomLoader* const   loader,
                   LV2_Atom_Forge* const forge,
                   const SerdNode* const node,
                   const SerdNode* const datatype)
{
  const char* const  str      = serd_node_string(node);
  const size_t       len      = serd_node_length(node);
  const char* const  type_uri = serd_node_string(datatype);
  LV2_Atom_Forge_Ref ref      = 0;

  if (!strcmp(type_uri, NS_XSD "int") || !strcmp(type_uri, NS_XSD "integer")) {
    ref = lv2_atom_forge_int(forge, strtol(str, NULL, 10));
  } else if (!strcmp(type_uri, NS_XSD "long")) {
    ref = lv2_atom_forge_long(forge, strtol(str, NULL, 10));
  } else if (!strcmp(type_uri, NS_XSD "decimal") ||
             !strcmp(type_uri, NS_XSD "double")) {
    ref = lv2_atom_forge_double(forge, serd_get_value(node).data.as_double);
  } else if (!strcmp(type_uri, NS_XSD "float")) {
    ref = lv2_atom_forge_float(forge, serd_get_value(node).data.as_float);
  } else if (!strcmp(type_uri, NS_XSD "boolean")) {
    ref = lv2_atom_forge_bool(forge, serd_get_value(node).data.as_bool);
  } else if (!strcmp(type_uri, NS_XSD "base64Binary")) {
    const size_t          size = serd_get_blob_size(node);
    void* const           body = malloc(size);
    const SerdWriteResult r    = serd_get_blob(node, size, body);
    if (r.status) {
      return SRATOM_BAD_FORGE;
    }

    if ((ref = lv2_atom_forge_atom(forge, r.count, forge->Chunk))) {
      ref = lv2_atom_forge_write(forge, body, r.count);
    }
    free(body);
  } else if (!strcmp(type_uri, LV2_ATOM__Path)) {
    ref = lv2_atom_forge_path(forge, str, len);
  } else if (!strcmp(type_uri, LV2_MIDI__MidiEvent)) {
    if ((ref = lv2_atom_forge_atom(forge, len / 2, loader->midi_MidiEvent))) {
      for (const char* s = str; s < str + len; s += 2) {
        unsigned num = 0u;
        sscanf(s, "%2X", &num);
        const uint8_t c = num;
        if (!(ref = lv2_atom_forge_raw(forge, &c, 1))) {
          return SRATOM_BAD_FORGE;
        }
      }
      lv2_atom_forge_pad(forge, len / 2);
    }
  } else {
    ref = lv2_atom_forge_literal(
      forge, str, len, loader->map->map(loader->map->handle, type_uri), 0);
  }

  return ref ? SRATOM_SUCCESS : SRATOM_BAD_FORGE;
}

static SratomStatus
read_plain_literal(SratomLoader* const   loader,
                   LV2_Atom_Forge* const forge,
                   const SerdNode* const node,
                   const SerdNode* const language)
{
  static const char* const prefix = "http://lexvo.org/id/iso639-3/";

  const char* const str          = serd_node_string(node);
  const size_t      len          = serd_node_length(node);
  const char*       lang_str     = serd_node_string(language);
  const size_t      lang_str_len = serd_node_length(language);
  const size_t      prefix_len   = strlen(prefix);
  const size_t      lang_uri_len = prefix_len + lang_str_len;
  char* const       lang_uri     = (char*)calloc(lang_uri_len + 1, 1);

  memcpy(lang_uri, prefix, prefix_len);
  memcpy(lang_uri + prefix_len, lang_str, lang_str_len);

  const LV2_Atom_Forge_Ref ref = lv2_atom_forge_literal(
    forge, str, len, 0, loader->map->map(loader->map->handle, lang_uri));

  free(lang_uri);

  return ref ? SRATOM_SUCCESS : SRATOM_BAD_FORGE;
}

static SratomStatus
read_literal(SratomLoader* const   loader,
             LV2_Atom_Forge* const forge,
             const SerdNode* const node)
{
  assert(serd_node_type(node) == SERD_LITERAL);

  const SerdNode* const datatype = serd_node_datatype(node);
  if (datatype) {
    return read_typed_literal(loader, forge, node, datatype);
  }

  const SerdNode* const language = serd_node_language(node);
  if (language) {
    return read_plain_literal(loader, forge, node, language);
  }

  const LV2_Atom_Forge_Ref ref = lv2_atom_forge_string(
    forge, serd_node_string(node), serd_node_length(node));

  return ref ? SRATOM_SUCCESS : SRATOM_BAD_FORGE;
}

static SratomStatus
read_object(LoadContext* const     ctx,
            LV2_Atom_Forge* const  forge,
            const SerdModel* const model,
            const SerdNode* const  node,
            const ReadMode         mode)
{
  SratomLoader* const loader = ctx->loader;
  LV2_URID_Map* const map    = loader->map;
  const char* const   str    = serd_node_string(node);
  SratomStatus        st     = SRATOM_SUCCESS;

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
      const SerdValue frameTime = serd_get_value_as(time, SERD_LONG, true);
      lv2_atom_forge_frame_time(forge, frameTime.data.as_long);
      seq_unit = loader->atom_frameTime;

    } else if ((time = serd_model_get(
                  model, node, loader->nodes.atom_beatTime, NULL, NULL))) {
      const SerdValue beatTime = serd_get_value_as(time, SERD_DOUBLE, true);
      lv2_atom_forge_beat_time(forge, beatTime.data.as_double);
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

    LV2_Atom_Sequence* const seq =
      (LV2_Atom_Sequence*)lv2_atom_forge_deref(forge, ref);
    seq->body.unit =
      ((ctx->seq_unit == loader->atom_frameTime) ? 0 : ctx->seq_unit);

  } else if (type_urid == loader->forge.Vector) {
    const SerdNode* child_type_node =
      serd_model_get(model, node, loader->nodes.atom_childType, NULL, NULL);
    if (!child_type_node) {
      return SRATOM_BAD_VECTOR;
    }

    const uint32_t child_type =
      map->map(map->handle, serd_node_string(child_type_node));
    const uint32_t child_size = atom_size(loader, child_type);
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
    const size_t          size = serd_get_blob_size(value);
    void* const           body = malloc(size);
    const SerdWriteResult r    = serd_get_blob(value, size, body);
    if (r.status) {
      return SRATOM_BAD_FORGE;
    }

    lv2_atom_forge_atom(forge, r.count, type_urid);
    lv2_atom_forge_write(forge, body, r.count);
    free(body);

  } else if (serd_node_type(node) == SERD_URI) {
    const LV2_URID urid = map->map(map->handle, str);
    if (serd_model_count(model, node, NULL, NULL, NULL)) {
      lv2_atom_forge_object(forge, &frame, urid, type_urid);
      read_resource(ctx, forge, model, node, type_urid);
    } else {
      st = read_uri(ctx, forge, node);
    }

  } else {
    lv2_atom_forge_object(forge, &frame, 0, type_urid);
    read_resource(ctx, forge, model, node, type_urid);
  }

  if (frame.ref) {
    lv2_atom_forge_pop(forge, &frame);
  }

  return st;
}

static SratomStatus
read_node(LoadContext* const     ctx,
          LV2_Atom_Forge* const  forge,
          const SerdModel* const model,
          const SerdNode* const  node,
          const ReadMode         mode)
{
  if (serd_node_type(node) == SERD_LITERAL) {
    return read_literal(ctx->loader, forge, node);
  }

  if (serd_node_type(node) == SERD_URI && mode != MODE_SUBJECT) {
    return read_uri(ctx, forge, node);
  }

  return read_object(ctx, forge, model, node, mode);
}

SratomStatus
sratom_load(SratomLoader* const    loader,
            const SerdNode* const  base_uri,
            LV2_Atom_Forge* const  forge,
            const SerdModel* const model,
            const SerdNode* const  node)
{
  LoadContext ctx = {loader, base_uri, 0};
  return read_node(&ctx, forge, model, node, MODE_SUBJECT);
}

static SerdModel*
model_from_string(SratomLoader* const loader,
                  SerdEnv* const      env,
                  const char* const   str)
{
  SerdModel* const model    = serd_model_new(loader->world, SERD_ORDER_SPO, 0u);
  SerdSink* const  inserter = serd_inserter_new(model, NULL);

  SerdNode* const   name     = serd_new_string(NULL, serd_string("string"));
  const char*       position = str;
  SerdInputStream   in       = serd_open_input_string(&position);
  SerdReader* const reader   = serd_reader_new(
    loader->world, SERD_TURTLE, SERD_READ_LAX, env, inserter, 4096);

  SerdStatus st = serd_reader_start(reader, &in, name, 4096);

  if (!st) {
    st = serd_reader_read_document(reader);
  }

  serd_reader_free(reader);
  serd_close_input(&in);
  serd_node_free(NULL, name);
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
  SerdModel* const model = model_from_string(loader, env, str);
  if (!model || serd_model_empty(model)) {
    LOAD_ERROR("Failed to read string into model");
    return NULL;
  }

  const SerdNode*            node  = NULL;
  SerdCursor* const          begin = serd_model_begin(model);
  const SerdStatement* const s     = serd_cursor_get(begin);

  assert(s);
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

  serd_cursor_free(begin);
  serd_model_free(model);
  return atom;
}

static LV2_Atom_Forge_Ref
sratom_forge_sink(const LV2_Atom_Forge_Sink_Handle handle,
                  const void* const                buf,
                  const uint32_t                   size)
{
  SerdBuffer* const        chunk = (SerdBuffer*)handle;
  const LV2_Atom_Forge_Ref ref   = (LV2_Atom_Forge_Ref)(chunk->len + 1);

  serd_buffer_write(buf, 1, size, chunk);
  return ref;
}

static LV2_Atom*
sratom_forge_deref(const LV2_Atom_Forge_Sink_Handle handle,
                   const LV2_Atom_Forge_Ref         ref)
{
  SerdBuffer* const chunk = (SerdBuffer*)handle;

  return (LV2_Atom*)((char*)chunk->buf + ref - 1);
}

LV2_Atom*
sratom_from_model(SratomLoader* const    loader,
                  const SerdNode* const  base_uri,
                  const SerdModel* const model,
                  const SerdNode* const  subject)
{
  if (!subject) {
    return NULL;
  }

  SerdBuffer out = {NULL, NULL, 0};
  lv2_atom_forge_set_sink(
    &loader->forge, sratom_forge_sink, sratom_forge_deref, &out);

  int st = sratom_load(loader, base_uri, &loader->forge, model, subject);
  if (st) {
    sratom_free(out.buf);
    out.buf = NULL;
  }

  return (LV2_Atom*)out.buf;
}
