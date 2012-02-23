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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"

#include "sratom/sratom.h"

#define NS_RDF  (const uint8_t*)"http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_XSD  (const uint8_t*)"http://www.w3.org/2001/XMLSchema#"
#define NS_MIDI (const uint8_t*)"http://lv2plug.in/ns/ext/midi#"

#define USTR(str) ((const uint8_t*)(str))

struct SratomImpl {
	SerdWriter*     writer;
	LV2_URID_Map*   map;
	LV2_URID_Unmap* unmap;
	LV2_Atom_Forge  forge;
	LV2_URID        atom_Event;
	LV2_URID        midi_MidiEvent;
	unsigned        next_id;
};

SRATOM_API
Sratom*
sratom_new(LV2_URID_Map*   map,
           LV2_URID_Unmap* unmap)
{
	Sratom* sratom = (Sratom*)malloc(sizeof(Sratom));
	sratom->writer         = NULL;
	sratom->map            = map;
	sratom->unmap          = unmap;
	sratom->atom_Event     = map->map(map->handle, LV2_ATOM_URI "#Event");
	sratom->midi_MidiEvent = map->map(map->handle, (const char*)NS_MIDI "MidiEvent");
	sratom->next_id        = 0;
	lv2_atom_forge_init(&sratom->forge, map);
	return sratom;
}

SRATOM_API
void
sratom_free(Sratom* sratom)
{
	free(sratom);
}

typedef struct {
	char*  buf;
	size_t len;
} String;

static size_t
string_sink(const void* buf, size_t len, void* stream)
{
	String* str = (String*)stream;
	str->buf = realloc(str->buf, str->len + len);
	memcpy(str->buf + str->len, buf, len);
	str->len += len;
	return len;
}

static void
gensym(SerdNode* out, char c, unsigned num)
{
	out->n_bytes = out->n_chars = snprintf(
		(char*)out->buf, 10, "%c%u", c, num);
}

static void
list_append(Sratom*   sratom,
            unsigned* flags,
            SerdNode* s,
            SerdNode* p,
            SerdNode* node,
            uint32_t  type,
            uint32_t  size,
            void*     body)
{
	// Generate a list node
	gensym(node, 'l', sratom->next_id);
	serd_writer_write_statement(sratom->writer, *flags, NULL,
	                            s, p, node, NULL, NULL);

	// _:node rdf:first value
	*flags = SERD_LIST_CONT;
	*p = serd_node_from_string(SERD_URI, NS_RDF "first");
	sratom_write(sratom, SERD_LIST_CONT, node, p, type, size, body);

	// Set subject to node and predicate to rdf:rest for next time
	gensym(node, 'l', ++sratom->next_id);
	*s = *node;
	*p = serd_node_from_string(SERD_URI, NS_RDF "rest");
}

static void
list_end(SerdWriter* writer, unsigned* flags, SerdNode* s, SerdNode* p)
{
	// _:node rdf:rest rdf:nil
	const SerdNode nil = serd_node_from_string(SERD_URI, NS_RDF "nil");
	serd_writer_write_statement(writer, *flags, NULL,
	                            s, p, &nil, NULL, NULL);
}

static void
start_object(Sratom*         sratom,
             uint32_t        flags,
             const SerdNode* subject,
             const SerdNode* predicate,
             const SerdNode* node,
             const char*     type)
{
	serd_writer_write_statement(
		sratom->writer, flags|SERD_ANON_O_BEGIN, NULL,
		subject, predicate, node, NULL, NULL);
	if (type) {
		SerdNode p = serd_node_from_string(SERD_URI, NS_RDF "type");
		SerdNode o = serd_node_from_string(SERD_URI, USTR(type));
		serd_writer_write_statement(
			sratom->writer, SERD_ANON_CONT, NULL,
			node, &p, &o, NULL, NULL);
	}
}

SRATOM_API
void
sratom_write(Sratom*         sratom,
             uint32_t        flags,
             const SerdNode* subject,
             const SerdNode* predicate,
             uint32_t        type_urid,
             uint32_t        size,
             const void*     body)
{
	LV2_URID_Unmap*   unmap       = sratom->unmap;
	const char* const type        = unmap->unmap(unmap->handle, type_urid);
	uint8_t           idbuf[12]   = "b0000000000";
	SerdNode          id          = serd_node_from_string(SERD_BLANK, idbuf);
	uint8_t           nodebuf[12] = "b0000000000";
	SerdNode          node        = serd_node_from_string(SERD_BLANK, nodebuf);
	SerdNode          object      = SERD_NODE_NULL;
	SerdNode          datatype    = SERD_NODE_NULL;
	SerdNode          language    = SERD_NODE_NULL;
	bool              new_node    = false;
	if (type_urid == 0 && size == 0) {
		object = serd_node_from_string(SERD_BLANK, USTR("null"));
	} else if (type_urid == sratom->forge.String) {
		object = serd_node_from_string(SERD_LITERAL, (const uint8_t*)body);
	} else if (type_urid == sratom->forge.Literal) {
		LV2_Atom_Literal_Body* lit = (LV2_Atom_Literal_Body*)body;
		const uint8_t*         str = USTR(lit + 1);
		object = serd_node_from_string(SERD_LITERAL, str);
		if (lit->datatype) {
			datatype = serd_node_from_string(
				SERD_URI, USTR(unmap->unmap(unmap->handle, lit->datatype)));
		} else if (lit->lang) {
			const char*  lang       = unmap->unmap(unmap->handle, lit->lang);
			const char*  prefix     = "http://lexvo.org/id/iso639-3/";
			const size_t prefix_len = strlen(prefix);
			if (lang && !strncmp(lang, prefix, prefix_len)) {
				language = serd_node_from_string(
					SERD_LITERAL, USTR(lang + prefix_len));
			} else {
				fprintf(stderr, "Unknown language URI <%s>\n", lang);
			}
		}
	} else if (type_urid == sratom->forge.URID) {
		const uint32_t id  = *(const uint32_t*)body;
		const uint8_t* str = USTR(unmap->unmap(unmap->handle, id));
		object = serd_node_from_string(SERD_URI, str);
	} else if (type_urid == sratom->forge.Path) {
		const uint8_t* str = USTR(body);
		object = serd_node_from_string(SERD_LITERAL, str);
		datatype = serd_node_from_string(SERD_URI, USTR(LV2_ATOM__Path));
	} else if (type_urid == sratom->forge.URI) {
		const uint8_t* str = USTR(body);
		object = serd_node_from_string(SERD_URI, str);
	} else if (type_urid == sratom->forge.Int32) {
		new_node = true;
		object   = serd_node_new_integer(*(int32_t*)body);
		datatype = serd_node_from_string(SERD_URI, NS_XSD "int");
	} else if (type_urid == sratom->forge.Int64) {
		new_node = true;
		object   = serd_node_new_integer(*(int64_t*)body);
		datatype = serd_node_from_string(SERD_URI, NS_XSD "long");
	} else if (type_urid == sratom->forge.Float) {
		new_node = true;
		object   = serd_node_new_decimal(*(float*)body, 8);
		datatype = serd_node_from_string(SERD_URI, NS_XSD "float");
	} else if (type_urid == sratom->forge.Double) {
		new_node = true;
		object   = serd_node_new_decimal(*(double*)body, 16);
		datatype = serd_node_from_string(SERD_URI, NS_XSD "double");
	} else if (type_urid == sratom->forge.Bool) {
		const int32_t val = *(const int32_t*)body;
		datatype = serd_node_from_string(SERD_URI, NS_XSD "boolean");
		object   = serd_node_from_string(SERD_LITERAL,
		                                 USTR(val ? "true" : "false"));
	} else if (type_urid == sratom->midi_MidiEvent) {
		new_node = true;
		datatype = serd_node_from_string(SERD_URI, NS_MIDI "MidiEvent");
		uint8_t* str = calloc(size * 2, 1);
		for (uint32_t i = 0; i < size; ++i) {
			sprintf((char*)str + (2 * i), "%02X",
			        (unsigned)(uint8_t)*((uint8_t*)body + i));
		}
		object = serd_node_from_string(SERD_LITERAL, USTR(str));
	} else if (type_urid == sratom->atom_Event) {
		const LV2_Atom_Event* ev = (const LV2_Atom_Event*)body;
		gensym(&id, 'e', sratom->next_id++);
		start_object(sratom, flags, subject, predicate, &id, NULL);
		// TODO: beat time
		SerdNode p    = serd_node_from_string(SERD_URI, USTR(LV2_ATOM__frameTime));
		SerdNode time = serd_node_new_integer(ev->time.frames);
		datatype = serd_node_from_string(SERD_URI, NS_XSD "decimal");
		serd_writer_write_statement(sratom->writer, SERD_ANON_CONT, NULL,
		                            &id, &p, &time,
		                            &datatype, &language);
		serd_node_free(&time);
		
		p = serd_node_from_string(SERD_URI, NS_RDF "value");
		sratom_write(sratom, SERD_ANON_CONT, &id, &p,
		             ev->body.type, ev->body.size,
		             LV2_ATOM_BODY(&ev->body));
		serd_writer_end_anon(sratom->writer, &id);
	} else if (type_urid == sratom->forge.Tuple) {
		gensym(&id, 't', sratom->next_id++);
		start_object(sratom, flags, subject, predicate, &id, type);
		SerdNode p = serd_node_from_string(SERD_URI, NS_RDF "value");
		flags |= SERD_LIST_O_BEGIN;
		LV2_TUPLE_BODY_FOREACH(body, size, i) {
			list_append(sratom, &flags, &id, &p, &node,
			            i->type, i->size, LV2_ATOM_BODY(i));
		}
		list_end(sratom->writer, &flags, &id, &p);
		serd_writer_end_anon(sratom->writer, &id);
	} else if (type_urid == sratom->forge.Vector) {
		const LV2_Atom_Vector_Body* vec  = (const LV2_Atom_Vector_Body*)body;
		gensym(&id, 'v', sratom->next_id++);
		start_object(sratom, flags, subject, predicate, &id, type);
		SerdNode p = serd_node_from_string(SERD_URI, NS_RDF "value");
		const uint32_t content_size = size - sizeof(LV2_Atom_Vector_Body);
		const uint32_t elem_size    = content_size / vec->elem_count;
		flags |= SERD_LIST_O_BEGIN;
		for (char* i = (char*)(vec + 1);
		     i < (char*)vec + size;
		     i += elem_size) {
			list_append(sratom, &flags, &id, &p, &node,
			            vec->elem_type, elem_size, i);
		}
		list_end(sratom->writer, &flags, &id, &p);
		serd_writer_end_anon(sratom->writer, &id);
	} else if (type_urid == sratom->forge.Blank) {
		const LV2_Atom_Object_Body* obj   = (const LV2_Atom_Object_Body*)body;
		const char*                 otype = unmap->unmap(unmap->handle, obj->otype);
		gensym(&id, 'b', sratom->next_id++);
		start_object(sratom, flags, subject, predicate, &id, otype);
		LV2_OBJECT_BODY_FOREACH(obj, size, i) {
			const LV2_Atom_Property_Body* prop = lv2_object_iter_get(i);
			const char* const key = unmap->unmap(unmap->handle, prop->key);
			SerdNode pred = serd_node_from_string(SERD_URI, USTR(key));
			sratom_write(sratom, flags|SERD_ANON_CONT, &id, &pred,
			             prop->value.type, prop->value.size,
			             LV2_ATOM_BODY(&prop->value));
		}
		serd_writer_end_anon(sratom->writer, &id);
	} else if (type_urid == sratom->forge.Sequence) {
		const LV2_Atom_Sequence_Body* seq  = (const LV2_Atom_Sequence_Body*)body;
		gensym(&id, 'v', sratom->next_id++);
		start_object(sratom, flags, subject, predicate, &id, type);
		SerdNode p = serd_node_from_string(SERD_URI, NS_RDF "value");
		flags |= SERD_LIST_O_BEGIN;
		LV2_SEQUENCE_BODY_FOREACH(seq, size, i) {
			LV2_Atom_Event* ev = lv2_sequence_iter_get(i);
			list_append(sratom, &flags, &id, &p, &node,
			            sratom->atom_Event,
			            sizeof(LV2_Atom_Event) + ev->body.size,
			            ev);
		}
		list_end(sratom->writer, &flags, &id, &p);
		serd_writer_end_anon(sratom->writer, &id);
	} else {
		object = serd_node_from_string(SERD_LITERAL, USTR("(unknown)"));
	}

	if (object.buf) {
		serd_writer_write_statement(sratom->writer, flags, NULL,
		                            subject, predicate, &object,
		                            &datatype, &language);
	}

	if (new_node) {
		serd_node_free(&object);
	}
}

SRATOM_API
char*
sratom_to_turtle(Sratom*         sratom,
                 const SerdNode* subject,
                 const SerdNode* predicate,
                 uint32_t        type,
                 uint32_t        size,
                 const void*     body)
{
	SerdURI  base_uri = SERD_URI_NULL;
	SerdEnv* env      = serd_env_new(NULL);
	String   str      = { NULL, 0 };

	serd_env_set_prefix_from_strings(env, USTR("midi"), NS_MIDI);
	serd_env_set_prefix_from_strings(env, USTR("atom"),
	                                 USTR(LV2_ATOM_URI "#"));
	serd_env_set_prefix_from_strings(env, USTR("rdf"), NS_RDF);
	serd_env_set_prefix_from_strings(env, USTR("xsd"), NS_XSD);
	serd_env_set_prefix_from_strings(env, USTR("eg"),
	                                 USTR("http://example.org/"));

	sratom->writer = serd_writer_new(
		SERD_TURTLE,
		SERD_STYLE_ABBREVIATED|SERD_STYLE_RESOLVED|SERD_STYLE_CURIED,
		env, &base_uri, string_sink, &str);

	sratom_write(sratom, 0, subject, predicate, type, size, body);
	serd_writer_finish(sratom->writer);
	string_sink("", 1, &str);

	serd_writer_free(sratom->writer);
	serd_env_free(env);
	free(str.buf);
	return str.buf;
}
