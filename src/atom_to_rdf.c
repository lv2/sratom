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

#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "seriatom/seriatom.h"

#define NS_RDF (const uint8_t*)"http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_XSD (const uint8_t*)"http://www.w3.org/2001/XMLSchema#"

#define USTR(str) ((const uint8_t*)(str))

struct SeriatomImpl {
	SerdWriter*     writer;
	LV2_URID_Unmap* unmap;
	unsigned        next_id;
};

SERIATOM_API
Seriatom*
seriatom_new(LV2_URID_Unmap* unmap)
{
	Seriatom* seriatom = (Seriatom*)malloc(sizeof(Seriatom));
	seriatom->writer  = NULL;
	seriatom->unmap   = unmap;
	seriatom->next_id = 0;
	return seriatom;
}

SERIATOM_API
void
seriatom_free(Seriatom* seriatom)
{
	free(seriatom);
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
list_append(Seriatom* seriatom,
            unsigned* flags,
            SerdNode* s,
            SerdNode* p,
            SerdNode* node,
            LV2_Atom* atom)
{
	// Generate a list node
	gensym(node, 'l', seriatom->next_id);
	serd_writer_write_statement(seriatom->writer, *flags, NULL,
	                            s, p, node, NULL, NULL);

	// _:node rdf:first value
	*flags = SERD_LIST_CONT;
	*p = serd_node_from_string(SERD_URI, NS_RDF "first");
	atom_to_rdf(seriatom, node, p, atom, SERD_LIST_CONT);

	// Set subject to node and predicate to rdf:rest for next time
	gensym(node, 'l', ++seriatom->next_id);
	*s = *node;
	*p = serd_node_from_string(SERD_URI, NS_RDF "rest");
}

static void
list_end(SerdWriter*     writer,
         LV2_URID_Unmap* unmap,
         unsigned*       flags,
         SerdNode*       s,
         SerdNode*       p)

{
	// _:node rdf:rest rdf:nil
	const SerdNode nil = serd_node_from_string(SERD_URI, NS_RDF "nil");
	serd_writer_write_statement(writer, *flags, NULL,
	                            s, p, &nil, NULL, NULL);
}

static void
start_object(Seriatom*       seriatom,
             uint32_t        flags,
             const SerdNode* subject,
             const SerdNode* predicate,
             const SerdNode* node,
             const char*     type)
{
	serd_writer_write_statement(
		seriatom->writer, flags|SERD_ANON_O_BEGIN, NULL,
		subject, predicate, node, NULL, NULL);
	if (type) {
		SerdNode p = serd_node_from_string(SERD_URI, NS_RDF "type");
		SerdNode o = serd_node_from_string(SERD_URI, USTR(type));
		serd_writer_write_statement(
			seriatom->writer, flags|SERD_ANON_CONT, NULL,
			node, &p, &o, NULL, NULL);
	}
}

SERIATOM_API
void
atom_to_rdf(Seriatom*       seriatom,
            const SerdNode* subject,
            const SerdNode* predicate,
            const LV2_Atom* atom,
            uint32_t        flags)
{
	LV2_URID_Unmap*   unmap       = seriatom->unmap;
	SerdWriter*       writer      = seriatom->writer;
	const char* const type        = unmap->unmap(unmap->handle, atom->type);
	uint8_t           idbuf[12]   = "b0000000000";
	SerdNode          id          = serd_node_from_string(SERD_BLANK, idbuf);
	uint8_t           nodebuf[12] = "b0000000000";
	SerdNode          node        = serd_node_from_string(SERD_BLANK, nodebuf);
	SerdNode          object      = SERD_NODE_NULL;
	SerdNode          datatype    = SERD_NODE_NULL;
	SerdNode          language    = SERD_NODE_NULL;
	bool              new_node    = false;
	if (atom->type == 0 && atom->size == 0) {
		object = serd_node_from_string(SERD_BLANK, USTR("null"));
	} else if (!strcmp(type, LV2_ATOM__String)) {
		const uint8_t* str = USTR(LV2_ATOM_BODY(atom));
		object = serd_node_from_string(SERD_LITERAL, str);
	} else if (!strcmp(type, LV2_ATOM__Literal)) {
		LV2_Atom_Literal* lit = (LV2_Atom_Literal*)atom;
		const uint8_t*    str = USTR(LV2_ATOM_CONTENTS(LV2_Atom_Literal, lit));
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
	} else if (!strcmp(type, LV2_ATOM__URID)) {
		const uint32_t id  = *(const uint32_t*)LV2_ATOM_BODY(atom);
		const uint8_t* str = USTR(unmap->unmap(unmap->handle, id));
		object = serd_node_from_string(SERD_URI, str);
	} else if (!strcmp(type, LV2_ATOM__Path)) {
		const uint8_t* str = USTR(LV2_ATOM_BODY(atom));
		object = serd_node_from_string(SERD_LITERAL, str);
		datatype = serd_node_from_string(SERD_URI, USTR(LV2_ATOM__Path));
	} else if (!strcmp(type, LV2_ATOM__URI)) {
		const uint8_t* str = USTR(LV2_ATOM_BODY(atom));
		object = serd_node_from_string(SERD_URI, str);
	} else if (!strcmp(type, LV2_ATOM__Int32)) {
		new_node = true;
		object   = serd_node_new_integer(*(int32_t*)LV2_ATOM_BODY(atom));
		datatype = serd_node_from_string(SERD_URI, NS_XSD "int");
	} else if (!strcmp(type, LV2_ATOM__Int64)) {
		new_node = true;
		object   = serd_node_new_integer(*(int64_t*)LV2_ATOM_BODY(atom));
		datatype = serd_node_from_string(SERD_URI, NS_XSD "long");
	} else if (!strcmp(type, LV2_ATOM__Float)) {
		new_node = true;
		object   = serd_node_new_decimal(*(float*)LV2_ATOM_BODY(atom), 8);
		datatype = serd_node_from_string(SERD_URI, NS_XSD "float");
	} else if (!strcmp(type, LV2_ATOM__Double)) {
		new_node = true;
		object   = serd_node_new_decimal(*(double*)LV2_ATOM_BODY(atom), 16);
		datatype = serd_node_from_string(SERD_URI, NS_XSD "double");
	} else if (!strcmp(type, LV2_ATOM__Bool)) {
		const int32_t val = *(const int32_t*)LV2_ATOM_BODY(atom);
		datatype = serd_node_from_string(SERD_URI, NS_XSD "boolean");
		object   = serd_node_from_string(SERD_LITERAL,
		                                 USTR(val ? "true" : "false"));
	} else if (!strcmp(type, LV2_ATOM__Tuple)) {
		gensym(&id, 't', seriatom->next_id++);
		start_object(seriatom, flags, subject, predicate, &id, type);
		SerdNode p = serd_node_from_string(SERD_URI, NS_RDF "value");
		flags |= SERD_LIST_O_BEGIN;
		LV2_TUPLE_FOREACH((LV2_Atom_Tuple*)atom, i) {
			list_append(seriatom, &flags, &id, &p, &node, i);
		}
		list_end(writer, unmap, &flags, &id, &p);
		serd_writer_end_anon(writer, &id);
	} else if (!strcmp(type, LV2_ATOM__Blank)) {
		const LV2_Atom_Object* obj   = (const LV2_Atom_Object*)atom;
		const char*            otype = unmap->unmap(unmap->handle, obj->otype);
		gensym(&id, 'b', seriatom->next_id++);
		start_object(seriatom, flags, subject, predicate, &id, otype);
		LV2_OBJECT_FOREACH(obj, i) {
			const LV2_Atom_Property_Body* prop = lv2_object_iter_get(i);
			const char* const key = unmap->unmap(unmap->handle, prop->key);
			SerdNode pred = serd_node_from_string(SERD_URI, USTR(key));
			atom_to_rdf(seriatom, &id, &pred, &prop->value, flags|SERD_ANON_CONT);
		}
		serd_writer_end_anon(seriatom->writer, &id);
	} else {
		object = serd_node_from_string(SERD_LITERAL, USTR("(unknown)"));
	}

	if (object.buf) {
		serd_writer_write_statement(seriatom->writer, flags, NULL,
		                            subject, predicate, &object,
		                            &datatype, &language);
	}

	if (new_node) {
		serd_node_free(&object);
	}
}

SERIATOM_API
char*
atom_to_turtle(Seriatom*       seriatom,
               const SerdNode* subject,
               const SerdNode* predicate,
               const LV2_Atom* atom)
{
	SerdURI  base_uri = SERD_URI_NULL;
	SerdEnv* env      = serd_env_new(NULL);
	String   str      = { NULL, 0 };

	serd_env_set_prefix_from_strings(env, USTR("atom"),
	                                 USTR(LV2_ATOM_URI "#"));
	serd_env_set_prefix_from_strings(env, USTR("rdf"), NS_RDF);
	serd_env_set_prefix_from_strings(env, USTR("xsd"), NS_XSD);

	seriatom->writer = serd_writer_new(
		SERD_TURTLE,
		SERD_STYLE_ABBREVIATED|SERD_STYLE_RESOLVED|SERD_STYLE_CURIED,
		env, &base_uri, string_sink, &str);

	atom_to_rdf(seriatom, subject, predicate, atom, 0);
	serd_writer_finish(seriatom->writer);
	string_sink("", 1, &str);

	serd_writer_free(seriatom->writer);
	serd_env_free(env);
	return str.buf;
}
