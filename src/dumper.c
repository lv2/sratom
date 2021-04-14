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
#include "lv2/atom/util.h"
#include "lv2/midi/midi.h"
#include "lv2/urid/urid.h"
#include "serd/serd.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

#define DUMP_WARN(msg) \
  serd_world_logf(writer->world, "sratom", SERD_LOG_LEVEL_WARNING, 0, NULL, msg)

#define DUMP_ERRORF(msg, ...) \
  serd_world_logf(            \
    writer->world, "sratom", SERD_LOG_LEVEL_ERROR, 0, NULL, msg, __VA_ARGS__)

struct SratomDumperImpl {
  LV2_URID_Unmap* unmap;
  LV2_Atom_Forge  forge;
  SerdWorld*      world;
  LV2_URID        atom_Event;
  LV2_URID        atom_beatTime;
  LV2_URID        midi_MidiEvent;
  struct {
    const SerdNode* atom_Path;
    const SerdNode* atom_beatTime;
    const SerdNode* atom_childType;
    const SerdNode* atom_frameTime;
    const SerdNode* midi_MidiEvent;
    const SerdNode* rdf_first;
    const SerdNode* rdf_nil;
    const SerdNode* rdf_rest;
    const SerdNode* rdf_type;
    const SerdNode* rdf_value;
    const SerdNode* xsd_base64Binary;
    const SerdNode* xsd_boolean;
    const SerdNode* xsd_decimal;
    const SerdNode* xsd_double;
    const SerdNode* xsd_float;
    const SerdNode* xsd_int;
    const SerdNode* xsd_integer;
    const SerdNode* xsd_long;
  } nodes;
};

typedef struct {
  SratomDumper*           writer;
  const SerdEnv*          env;
  const SerdSink*         sink;
  const SratomDumperFlags flags;
  SerdStatementFlags      sflags;
  LV2_URID                seq_unit;
} StreamContext;

void
sratom_free(void* ptr)
{
  /* The only sratom memory the user frees comes from sratom_from_model(),
     which is allocated by serd_buffer_sink. */
  serd_free(ptr);
}

SratomDumper*
sratom_dumper_new(SerdWorld* const      world,
                  LV2_URID_Map* const   map,
                  LV2_URID_Unmap* const unmap)
{
  SratomDumper* const dumper = (SratomDumper*)calloc(1, sizeof(SratomDumper));
  if (!dumper) {
    return NULL;
  }

  dumper->world          = world;
  dumper->unmap          = unmap;
  dumper->atom_Event     = map->map(map->handle, LV2_ATOM__Event);
  dumper->atom_beatTime  = map->map(map->handle, LV2_ATOM__beatTime);
  dumper->midi_MidiEvent = map->map(map->handle, LV2_MIDI__MidiEvent);
  lv2_atom_forge_init(&dumper->forge, map);

#define MANAGE_URI(uri)                      \
  serd_nodes_manage(serd_world_nodes(world), \
                    serd_new_uri(SERD_STATIC_STRING(uri)))

  dumper->nodes.atom_Path        = MANAGE_URI(LV2_ATOM__Path);
  dumper->nodes.atom_beatTime    = MANAGE_URI(LV2_ATOM__beatTime);
  dumper->nodes.atom_childType   = MANAGE_URI(LV2_ATOM__childType);
  dumper->nodes.atom_frameTime   = MANAGE_URI(LV2_ATOM__frameTime);
  dumper->nodes.midi_MidiEvent   = MANAGE_URI(LV2_MIDI__MidiEvent);
  dumper->nodes.rdf_first        = MANAGE_URI(NS_RDF "first");
  dumper->nodes.rdf_nil          = MANAGE_URI(NS_RDF "nil");
  dumper->nodes.rdf_rest         = MANAGE_URI(NS_RDF "rest");
  dumper->nodes.rdf_type         = MANAGE_URI(NS_RDF "type");
  dumper->nodes.rdf_value        = MANAGE_URI(NS_RDF "value");
  dumper->nodes.xsd_base64Binary = MANAGE_URI(NS_XSD "base64Binary");
  dumper->nodes.xsd_boolean      = MANAGE_URI(NS_XSD "boolean");
  dumper->nodes.xsd_decimal      = MANAGE_URI(NS_XSD "decimal");
  dumper->nodes.xsd_double       = MANAGE_URI(NS_XSD "double");
  dumper->nodes.xsd_float        = MANAGE_URI(NS_XSD "float");
  dumper->nodes.xsd_int          = MANAGE_URI(NS_XSD "int");
  dumper->nodes.xsd_integer      = MANAGE_URI(NS_XSD "integer");
  dumper->nodes.xsd_long         = MANAGE_URI(NS_XSD "long");

#undef MANAGE_URI

  return dumper;
}

void
sratom_dumper_free(SratomDumper* const writer)
{
  free(writer);
}

static SratomStatus
write_atom(StreamContext*  ctx,
           const SerdNode* subject,
           const SerdNode* predicate,
           LV2_URID        type,
           uint32_t        size,
           const void*     body);

static void
list_append(StreamContext* const   ctx,
            SerdNode** const       s,
            const SerdNode** const p,
            const uint32_t         size,
            const uint32_t         type,
            const void* const      body)
{
  // Generate a list node
  SerdNode* const node =
    serd_node_copy(serd_world_get_blank(ctx->writer->world));
  serd_sink_write(ctx->sink, ctx->sflags, *s, *p, node, NULL);

  // _:node rdf:first value
  ctx->sflags = 0;
  *p          = ctx->writer->nodes.rdf_first;
  write_atom(ctx, node, *p, type, size, body);

  // Set subject to node and predicate to rdf:rest for next time
  serd_node_free(*s);
  *s = node;
  *p = ctx->writer->nodes.rdf_rest;
}

static void
list_end(const StreamContext* const ctx,
         const SerdNode* const      s,
         const SerdNode* const      p)
{
  // _:node rdf:rest rdf:nil
  serd_sink_write(ctx->sink, 0, s, p, ctx->writer->nodes.rdf_nil, NULL);
}

static void
start_object(StreamContext* const  ctx,
             const SerdNode* const subject,
             const SerdNode* const predicate,
             const SerdNode* const node,
             const char* const     type)
{
  if (subject && predicate) {
    serd_sink_write(
      ctx->sink, ctx->sflags | SERD_ANON_O, subject, predicate, node, NULL);
  } else {
    ctx->sflags |= SERD_EMPTY_S;
  }

  if (type) {
    SerdNode* const o = serd_new_uri(SERD_MEASURE_STRING(type));

    serd_sink_write(
      ctx->sink, ctx->sflags, node, ctx->writer->nodes.rdf_type, o, NULL);

    serd_node_free(o);
  }
}

static void
end_object(const StreamContext* const ctx,
           const SerdNode* const      subject,
           const SerdNode* const      predicate,
           const SerdNode* const      node)
{
  if (subject && predicate) {
    serd_sink_write_end(ctx->sink, node);
  }
}

static bool
path_is_absolute(const char* const path)
{
  return (path[0] == '/' || (isalpha(path[0]) && path[1] == ':' &&
                             (path[2] == '/' || path[2] == '\\')));
}

static const SerdNode*
number_type(const StreamContext* const ctx, const SerdNode* const type)
{
  SratomDumper* const writer = ctx->writer;
  const bool          pretty = (ctx->flags & SRATOM_PRETTY_NUMBERS);

  if (pretty) {
    if (type == writer->nodes.xsd_int || type == writer->nodes.xsd_long) {
      return writer->nodes.xsd_integer;
    }

    if (type == writer->nodes.xsd_float || type == writer->nodes.xsd_double) {
      return writer->nodes.xsd_decimal;
    }
  }

  return type;
}

static bool
is_primitive_type(const StreamContext* const ctx, const LV2_URID type)
{
  SratomDumper* const writer = ctx->writer;
  return (!type || type == writer->forge.Bool || type == writer->forge.Double ||
          type == writer->forge.Float || type == writer->forge.Int ||
          type == writer->forge.Literal || type == writer->forge.Long ||
          type == writer->forge.Path || type == writer->forge.String ||
          type == writer->forge.URI || type == writer->forge.URID);
}

static SratomStatus
write_atom(StreamContext* const  ctx,
           const SerdNode* const subject,
           const SerdNode* const predicate,
           const LV2_URID        type,
           const uint32_t        size,
           const void* const     body)
{
  SratomDumper* const   writer   = ctx->writer;
  LV2_URID_Unmap* const unmap    = writer->unmap;
  const SerdSink* const sink     = ctx->sink;
  const SerdEnv* const  env      = ctx->env;
  const char* const     type_uri = unmap->unmap(unmap->handle, type);
  SerdNode*             object   = NULL;
  SratomStatus          st       = SRATOM_SUCCESS;

  if (type == 0 && size == 0) {
    object = serd_node_copy(writer->nodes.rdf_nil);
  } else if (type == writer->forge.String) {
    object = serd_new_string(SERD_MEASURE_STRING((const char*)body));
  } else if (type == writer->forge.Chunk) {
    object = serd_new_blob(body, size, NULL);
  } else if (type == writer->forge.Literal) {
    const LV2_Atom_Literal_Body* lit = (const LV2_Atom_Literal_Body*)body;
    const char*                  str = (const char*)(lit + 1);
    if (lit->datatype) {
      const SerdStringView datatype_uri =
        SERD_MEASURE_STRING(unmap->unmap(unmap->handle, lit->datatype));
      object = serd_new_typed_literal(SERD_MEASURE_STRING(str), datatype_uri);
    } else if (lit->lang) {
      const char*  lang       = unmap->unmap(unmap->handle, lit->lang);
      const char*  prefix     = "http://lexvo.org/id/iso639-3/";
      const size_t prefix_len = strlen(prefix);
      if (lang && !strncmp(lang, prefix, prefix_len)) {
        object = serd_new_plain_literal(SERD_MEASURE_STRING(str),
                                        SERD_MEASURE_STRING(lang + prefix_len));
      } else {
        DUMP_ERRORF("Unknown language URID %u\n", lit->lang);
      }
    }
  } else if (type == writer->forge.URID) {
    const uint32_t urid = *(const uint32_t*)body;
    const char*    str  = unmap->unmap(unmap->handle, urid);

    object = serd_new_uri(SERD_MEASURE_STRING(str));
  } else if (type == writer->forge.Path) {
    const SerdStringView str = SERD_MEASURE_STRING((const char*)body);
    if (path_is_absolute(str.buf)) {
      object = serd_new_file_uri(str, SERD_EMPTY_STRING());
    } else {
      const SerdNode* base_uri = serd_env_base_uri(env);
      if (!base_uri || strncmp(serd_node_string(base_uri), "file://", 7)) {
        DUMP_WARN("Relative path but base is not a file URI.\n");
        DUMP_WARN("Writing ambiguous atom:Path literal.\n");
        object = serd_new_typed_literal(
          str, serd_node_string_view(writer->nodes.atom_Path));
      } else {
        SerdNode* const rel = serd_new_file_uri(str, SERD_EMPTY_STRING());

        object = serd_new_parsed_uri(serd_resolve_uri(
          serd_node_uri_view(rel), serd_node_uri_view(base_uri)));

        serd_node_free(rel);
      }
    }
  } else if (type == writer->forge.URI) {
    object = serd_new_uri(SERD_MEASURE_STRING((const char*)body));
  } else if (type == writer->forge.Int) {
    object = serd_new_integer(*(const int32_t*)body,
                              number_type(ctx, writer->nodes.xsd_int));
  } else if (type == writer->forge.Long) {
    object = serd_new_integer(*(const int64_t*)body,
                              number_type(ctx, writer->nodes.xsd_long));
  } else if (type == writer->forge.Float) {
    object = serd_new_decimal(*(const float*)body,
                              number_type(ctx, writer->nodes.xsd_float));
  } else if (type == writer->forge.Double) {
    object = serd_new_decimal(*(const double*)body,
                              number_type(ctx, writer->nodes.xsd_double));
  } else if (type == writer->forge.Bool) {
    object = serd_new_boolean(*(const int32_t*)body);
  } else if (type == writer->midi_MidiEvent) {
    const size_t len = 2 * size;
    char* const  str = (char*)calloc(len + 1, 1);
    for (uint32_t i = 0; i < size; ++i) {
      snprintf(str + (2 * i),
               size * 2 + 1,
               "%02X",
               (unsigned)*((const uint8_t*)body + i));
    }

    object = serd_new_typed_literal(
      SERD_STRING_VIEW(str, len),
      serd_node_string_view(writer->nodes.midi_MidiEvent));

    free(str);
  } else if (type == writer->atom_Event) {
    const LV2_Atom_Event* const ev = (const LV2_Atom_Event*)body;
    const SerdNode* const       id = serd_world_get_blank(writer->world);
    start_object(ctx, subject, predicate, id, NULL);
    SerdNode*       time = NULL;
    const SerdNode* p    = NULL;
    if (ctx->seq_unit == writer->atom_beatTime) {
      p    = writer->nodes.atom_beatTime;
      time = serd_new_double(ev->time.beats);
    } else {
      p    = writer->nodes.atom_frameTime;
      time = serd_new_integer(ev->time.frames,
                              number_type(ctx, writer->nodes.xsd_long));
    }
    serd_sink_write(sink, 0, id, p, time, NULL);
    serd_node_free(time);

    p = writer->nodes.rdf_value;
    write_atom(
      ctx, id, p, ev->body.type, ev->body.size, LV2_ATOM_BODY_CONST(&ev->body));
    end_object(ctx, subject, predicate, id);
  } else if (type == writer->forge.Tuple) {
    SerdNode*       id = serd_node_copy(serd_world_get_blank(writer->world));
    const SerdNode* p  = writer->nodes.rdf_value;
    start_object(ctx, subject, predicate, id, type_uri);

    ctx->sflags |= SERD_LIST_O | SERD_TERSE_O;
    LV2_ATOM_TUPLE_BODY_FOREACH (body, size, i) {
      if (!is_primitive_type(ctx, i->type)) {
        ctx->sflags &= ~SERD_TERSE_O;
      }
    }

    LV2_ATOM_TUPLE_BODY_FOREACH (body, size, i) {
      list_append(ctx, &id, &p, i->size, i->type, LV2_ATOM_BODY(i));
    }

    serd_sink_write(ctx->sink, 0, id, p, ctx->writer->nodes.rdf_nil, NULL);
    end_object(ctx, subject, predicate, id);
    serd_node_free(id);
  } else if (type == writer->forge.Vector) {
    const LV2_Atom_Vector_Body* const vec = (const LV2_Atom_Vector_Body*)body;

    SerdNode* id = serd_node_copy(serd_world_get_blank(writer->world));
    start_object(ctx, subject, predicate, id, type_uri);

    const SerdNode* p          = writer->nodes.atom_childType;
    SerdNode* const child_type = serd_new_uri(
      SERD_MEASURE_STRING(unmap->unmap(unmap->handle, vec->child_type)));

    serd_sink_write(sink, ctx->sflags, id, p, child_type, NULL);
    p = writer->nodes.rdf_value;
    serd_node_free(child_type);
    ctx->sflags |= SERD_LIST_O;
    if (is_primitive_type(ctx, vec->child_type)) {
      ctx->sflags |= SERD_TERSE_O;
    }

    for (const char* i = (const char*)(vec + 1); i < (const char*)vec + size;
         i += vec->child_size) {
      list_append(ctx, &id, &p, vec->child_size, vec->child_type, i);
    }

    serd_sink_write(ctx->sink, 0, id, p, ctx->writer->nodes.rdf_nil, NULL);
    end_object(ctx, subject, predicate, id);
    serd_node_free(id);
  } else if (lv2_atom_forge_is_object_type(&writer->forge, type)) {
    const LV2_Atom_Object_Body* obj   = (const LV2_Atom_Object_Body*)body;
    const char* const           otype = unmap->unmap(unmap->handle, obj->otype);

    SerdNode* id = NULL;
    if (lv2_atom_forge_is_blank(&writer->forge, type, obj)) {
      id = serd_node_copy(serd_world_get_blank(writer->world));
      start_object(ctx, subject, predicate, id, otype);
    } else {
      id =
        serd_new_uri(SERD_MEASURE_STRING(unmap->unmap(unmap->handle, obj->id)));
      ctx->sflags = 0;
      start_object(ctx, NULL, NULL, id, otype);
    }
    LV2_ATOM_OBJECT_BODY_FOREACH (obj, size, prop) {
      const char* const key  = unmap->unmap(unmap->handle, prop->key);
      SerdNode* const   pred = serd_new_uri(SERD_MEASURE_STRING(key));
      write_atom(ctx,
                 id,
                 pred,
                 prop->value.type,
                 prop->value.size,
                 LV2_ATOM_BODY(&prop->value));
      serd_node_free(pred);
    }
    end_object(ctx, subject, predicate, id);
    serd_node_free(id);
  } else if (type == writer->forge.Sequence) {
    const LV2_Atom_Sequence_Body* seq = (const LV2_Atom_Sequence_Body*)body;
    SerdNode* id = serd_node_copy(serd_world_get_blank(writer->world));
    start_object(ctx, subject, predicate, id, type_uri);
    const SerdNode* p = writer->nodes.rdf_value;
    ctx->sflags |= SERD_LIST_O;
    LV2_ATOM_SEQUENCE_BODY_FOREACH (seq, size, ev) {
      ctx->seq_unit = seq->unit;
      list_append(ctx,
                  &id,
                  &p,
                  sizeof(LV2_Atom_Event) + ev->body.size,
                  writer->atom_Event,
                  ev);
    }
    list_end(ctx, id, p);
    end_object(ctx, subject, predicate, id);
    serd_node_free(id);
  } else {
    const SerdNode* id = serd_world_get_blank(writer->world);
    start_object(ctx, subject, predicate, id, type_uri);
    const SerdNode* p = writer->nodes.rdf_value;
    SerdNode*       o = serd_new_blob(body, size, NULL);
    serd_sink_write(sink, ctx->sflags, id, p, o, NULL);
    end_object(ctx, subject, predicate, id);
    serd_node_free(o);
  }

  if (object) {
    if (!subject || !predicate) {
      const SerdNode* const blank = serd_world_get_blank(writer->world);
      serd_sink_write(sink,
                      ctx->sflags | SERD_LIST_S | SERD_TERSE_S,
                      blank,
                      writer->nodes.rdf_first,
                      object,
                      NULL);
      serd_sink_write(sink,
                      SERD_TERSE_S,
                      blank,
                      writer->nodes.rdf_rest,
                      writer->nodes.rdf_nil,
                      NULL);
    } else {
      serd_sink_write(sink, ctx->sflags, subject, predicate, object, NULL);
    }
  }

  serd_node_free(object);

  return st;
}

SratomStatus
sratom_dump(SratomDumper* const     writer,
            const SerdEnv* const    env,
            const SerdSink* const   sink,
            const SerdNode* const   subject,
            const SerdNode* const   predicate,
            const LV2_URID          type,
            const uint32_t          size,
            const void* const       body,
            const SratomDumperFlags flags)
{
  StreamContext ctx = {writer,
                       env,
                       sink,
                       flags,
                       (flags & SRATOM_NAMED_SUBJECT) ? 0 : SERD_EMPTY_S,
                       0};

  return write_atom(&ctx, subject, predicate, type, size, body);
}

SratomStatus
sratom_dump_atom(SratomDumper* const     writer,
                 const SerdEnv* const    env,
                 const SerdSink* const   sink,
                 const SerdNode* const   subject,
                 const SerdNode* const   predicate,
                 const LV2_Atom* const   atom,
                 const SratomDumperFlags flags)
{
  return sratom_dump(writer,
                     env,
                     sink,
                     subject,
                     predicate,
                     atom->type,
                     atom->size,
                     atom + 1,
                     flags);
}

char*
sratom_to_string(SratomDumper* const     writer,
                 const SerdEnv* const    env,
                 const LV2_Atom* const   atom,
                 const SratomDumperFlags flags)
{
  SerdBuffer          buffer = {NULL, 0};
  SerdByteSink* const out    = serd_byte_sink_new_buffer(&buffer);

  SerdWriter* const ttl_writer =
    serd_writer_new(writer->world,
                    SERD_TURTLE,
                    flags & SRATOM_TERSE ? SERD_WRITE_TERSE : 0,
                    env,
                    out);

  const SerdSink* const sink = serd_writer_sink(ttl_writer);

  sratom_dump_atom(writer, env, sink, NULL, NULL, atom, flags);
  serd_writer_finish(ttl_writer);
  serd_writer_free(ttl_writer);

  return serd_buffer_sink_finish(&buffer);
}
