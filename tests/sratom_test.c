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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "sratom/sratom.h"

#define NS_RDF  "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_MIDI "http://lv2plug.in/ns/ext/midi#"

#define USTR(s) ((const uint8_t*)(s))

char** uris   = NULL;
size_t n_uris = 0;

char*
copy_string(const char* str)
{
	const size_t len = strlen(str);
	char*        dup = (char*)malloc(len + 1);
	memcpy(dup, str, len + 1);
	return dup;
}

LV2_URID
urid_map(LV2_URID_Map_Handle handle, const char* uri)
{
	for (size_t i = 0; i < n_uris; ++i) {
		if (!strcmp(uris[i], uri)) {
			return i + 1;
		}
	}

	uris = (char**)realloc(uris, ++n_uris * sizeof(char*));
	uris[n_uris - 1] = copy_string(uri);
	return n_uris;
}

const char*
urid_unmap(LV2_URID_Unmap_Handle handle,
           LV2_URID              urid)
{
	if (urid <= n_uris) {
		return uris[urid - 1];
	}
	return NULL;
}

int
test_fail(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	return 1;
}

int
main()
{
	LV2_URID_Map   map   = { NULL, urid_map };
	LV2_URID_Unmap unmap = { NULL, urid_unmap };
	LV2_Atom_Forge forge;
	lv2_atom_forge_init(&forge, &map);

	Sratom* sratom = sratom_new(&map, &unmap);

	LV2_URID eg_Object  = urid_map(NULL, "http://example.org/Object");
	LV2_URID eg_one     = urid_map(NULL, "http://example.org/one");
	LV2_URID eg_two     = urid_map(NULL, "http://example.org/two");
	LV2_URID eg_three   = urid_map(NULL, "http://example.org/three");
	LV2_URID eg_four    = urid_map(NULL, "http://example.org/four");
	LV2_URID eg_true    = urid_map(NULL, "http://example.org/true");
	LV2_URID eg_false   = urid_map(NULL, "http://example.org/false");
	LV2_URID eg_path    = urid_map(NULL, "http://example.org/path");
	LV2_URID eg_uri     = urid_map(NULL, "http://example.org/uri");
	LV2_URID eg_urid    = urid_map(NULL, "http://example.org/urid");
	LV2_URID eg_string  = urid_map(NULL, "http://example.org/string");
	LV2_URID eg_langlit = urid_map(NULL, "http://example.org/langlit");
	LV2_URID eg_typelit = urid_map(NULL, "http://example.org/typelit");
	LV2_URID eg_blank   = urid_map(NULL, "http://example.org/blank");
	LV2_URID eg_tuple   = urid_map(NULL, "http://example.org/tuple");
	LV2_URID eg_vector  = urid_map(NULL, "http://example.org/vector");
	LV2_URID eg_seq     = urid_map(NULL, "http://example.org/seq");

	uint8_t buf[1024];
	lv2_atom_forge_set_buffer(&forge, buf, sizeof(buf));

	LV2_Atom_Forge_Frame obj_frame;
	LV2_Atom* obj = (LV2_Atom*)lv2_atom_forge_blank(
		&forge, &obj_frame, 1, eg_Object);

	// eg_one = (Int32)1
	lv2_atom_forge_property_head(&forge, eg_one, 0);
	LV2_Atom_Int32* one = lv2_atom_forge_int32(&forge, 1);
	if (one->body != 1) {
		return test_fail("%d != 1\n", one->body);
	}

	// eg_two = (Int64)2
	lv2_atom_forge_property_head(&forge, eg_two, 0);
	LV2_Atom_Int64* two = lv2_atom_forge_int64(&forge, 2);
	if (two->body != 2) {
		return test_fail("%ld != 2\n", two->body);
	}

	// eg_three = (Float)3.0
	lv2_atom_forge_property_head(&forge, eg_three, 0);
	LV2_Atom_Float* three = lv2_atom_forge_float(&forge, 3.0f);
	if (three->body != 3) {
		return test_fail("%f != 3\n", three->body);
	}

	// eg_four = (Double)4.0
	lv2_atom_forge_property_head(&forge, eg_four, 0);
	LV2_Atom_Double* four = lv2_atom_forge_double(&forge, 4.0);
	if (four->body != 4) {
		return test_fail("%ld != 4\n", four->body);
	}

	// eg_true = (Bool)1
	lv2_atom_forge_property_head(&forge, eg_true, 0);
	LV2_Atom_Bool* t = lv2_atom_forge_bool(&forge, true);
	if (t->body != 1) {
		return test_fail("%ld != 1 (true)\n", t->body);
	}

	// eg_false = (Bool)0
	lv2_atom_forge_property_head(&forge, eg_false, 0);
	LV2_Atom_Bool* f = lv2_atom_forge_bool(&forge, false);
	if (f->body != 0) {
		return test_fail("%ld != 0 (false)\n", f->body);
	}

	// eg_path = (Path)"/foo/bar"
	const uint8_t* pstr     = (const uint8_t*)"/foo/bar";
	const size_t   pstr_len = strlen((const char*)pstr);
	lv2_atom_forge_property_head(&forge, eg_path, 0);
	LV2_Atom_String* path  = lv2_atom_forge_uri(&forge, pstr, pstr_len);
	uint8_t*         pbody = (uint8_t*)LV2_ATOM_BODY(path);
	if (strcmp((const char*)pbody, (const char*)pstr)) {
		return test_fail("%s != \"%s\"\n",
		                 (const char*)pbody, (const char*)pstr);
	}

	// eg_uri = (URI)"http://example.org/value"
	const uint8_t* ustr     = (const uint8_t*)"http://example.org/value";
	const size_t   ustr_len = strlen((const char*)ustr);
	lv2_atom_forge_property_head(&forge, eg_uri, 0);
	LV2_Atom_String* uri   = lv2_atom_forge_uri(&forge, ustr, ustr_len);
	uint8_t*         ubody = (uint8_t*)LV2_ATOM_BODY(uri);
	if (strcmp((const char*)ubody, (const char*)ustr)) {
		return test_fail("%s != \"%s\"\n",
		                 (const char*)ubody, (const char*)ustr);
	}

	// eg_urid = (URID)"http://example.org/value"
	LV2_URID eg_value = urid_map(NULL, "http://example.org/value");
	lv2_atom_forge_property_head(&forge, eg_urid, 0);
	LV2_Atom_URID* urid = lv2_atom_forge_urid(&forge, eg_value);
	if (urid->body != eg_value) {
		return test_fail("%u != %u\n", urid->body, eg_value);
	}

	// eg_string = (String)"hello"
	lv2_atom_forge_property_head(&forge, eg_string, 0);
	LV2_Atom_String* string = lv2_atom_forge_string(
		&forge, (const uint8_t*)"hello", strlen("hello"));
	uint8_t* sbody = (uint8_t*)LV2_ATOM_BODY(string);
	if (strcmp((const char*)sbody, "hello")) {
		return test_fail("%s != \"hello\"\n", (const char*)sbody);
	}

	// eg_langlit = (Literal)"bonjour"@fr
	lv2_atom_forge_property_head(&forge, eg_langlit, 0);
	LV2_Atom_Literal* langlit = lv2_atom_forge_literal(
		&forge, (const uint8_t*)"bonjour", strlen("bonjour"),
		0, urid_map(NULL, "http://lexvo.org/id/iso639-3/fra"));
	uint8_t* llbody = (uint8_t*)LV2_ATOM_CONTENTS(LV2_Atom_Literal, langlit);
	if (strcmp((const char*)llbody, "bonjour")) {
		return test_fail("%s != \"bonjour\"\n", (const char*)llbody);
	}

	// eg_typelit = (Literal)"bonjour"@fr
	lv2_atom_forge_property_head(&forge, eg_typelit, 0);
	LV2_Atom_Literal* typelit = lv2_atom_forge_literal(
		&forge, (const uint8_t*)"value", strlen("value"),
		urid_map(NULL, "http://example.org/Type"), 0);
	uint8_t* tlbody = (uint8_t*)LV2_ATOM_CONTENTS(LV2_Atom_Literal, typelit);
	if (strcmp((const char*)tlbody, "value")) {
		return test_fail("%s != \"value\"\n", (const char*)tlbody);
	}

	// eg_blank = [ a <http://example.org/Object> ]
	lv2_atom_forge_property_head(&forge, eg_blank, 0);
	LV2_Atom_Forge_Frame blank_frame;
	lv2_atom_forge_blank(&forge, &blank_frame, 2, eg_Object);
	lv2_atom_forge_pop(&forge, &blank_frame);

	// eg_tuple = "foo",true
	lv2_atom_forge_property_head(&forge, eg_tuple, 0);
	LV2_Atom_Forge_Frame tuple_frame;
	LV2_Atom_Tuple*      tuple = (LV2_Atom_Tuple*)lv2_atom_forge_tuple(
		&forge, &tuple_frame);
	LV2_Atom_String* tup0 = lv2_atom_forge_string(
		&forge, (const uint8_t*)"foo", strlen("foo"));
	LV2_Atom_Bool* tup1 = lv2_atom_forge_bool(&forge, true);
	lv2_atom_forge_pop(&forge, &tuple_frame);
	LV2_Atom_Tuple_Iter i = lv2_tuple_begin(tuple);
	if (lv2_tuple_is_end(tuple, i)) {
		return test_fail("Tuple iterator is empty\n");
	}
	LV2_Atom* tup0i = (LV2_Atom*)lv2_tuple_iter_get(i);
	if (!lv2_atom_equals((LV2_Atom*)tup0, tup0i)) {
		return test_fail("Corrupt tuple element 0\n");
	}
	i = lv2_tuple_iter_next(i);
	if (lv2_tuple_is_end(tuple, i)) {
		return test_fail("Premature end of tuple iterator\n");
	}
	LV2_Atom* tup1i = lv2_tuple_iter_get(i);
	if (!lv2_atom_equals((LV2_Atom*)tup1, tup1i)) {
		return test_fail("Corrupt tuple element 1\n");
	}
	i = lv2_tuple_iter_next(i);
	if (!lv2_tuple_is_end(tuple, i)) {
		return test_fail("Tuple iter is not at end\n");
	}

	// eg_vector = (Vector<Int32>)1,2,3,4
	lv2_atom_forge_property_head(&forge, eg_vector, 0);
	int32_t elems[] = { 1, 2, 3, 4 };
	LV2_Atom_Vector* vector = lv2_atom_forge_vector(
		&forge, 4, forge.Int32, sizeof(int32_t), elems);
	void* vec_body = LV2_ATOM_CONTENTS(LV2_Atom_Vector, vector);
	if (memcmp(elems, vec_body, sizeof(elems))) {
		return test_fail("Corrupt vector\n");
	}

	// eg_seq = (Sequence)1, 2
	LV2_URID midi_midiEvent = map.map(map.handle, NS_MIDI "MidiEvent");
	lv2_atom_forge_property_head(&forge, eg_seq, 0);
	LV2_Atom_Forge_Frame seq_frame;
	lv2_atom_forge_sequence_head(&forge, &seq_frame, 0);
	
	const uint8_t ev1[3] = { 0x90, 0x1A, 0x1 };
	lv2_atom_forge_frame_time(&forge, 1);
	lv2_atom_forge_atom(&forge, midi_midiEvent, sizeof(ev1));
	lv2_atom_forge_raw(&forge, ev1, sizeof(ev1));
	lv2_atom_forge_pad(&forge, sizeof(ev1));

	const uint8_t ev2[3] = { 0x90, 0x2B, 0x2 };
	lv2_atom_forge_frame_time(&forge, 3);
	lv2_atom_forge_atom(&forge, midi_midiEvent, sizeof(ev2));
	lv2_atom_forge_raw(&forge, ev2, sizeof(ev2));
	lv2_atom_forge_pad(&forge, sizeof(ev2));

	lv2_atom_forge_pop(&forge, &seq_frame);
	lv2_atom_forge_pop(&forge, &obj_frame);

	SerdNode s = serd_node_from_string(SERD_BLANK, USTR("obj"));
	SerdNode p = serd_node_from_string(SERD_URI, USTR(NS_RDF "value"));
	printf("%s", atom_to_turtle(sratom, &s, &p, obj));

	printf("All tests passed.\n");
	sratom_free(sratom);
	for (uint32_t i = 0; i < n_uris; ++i) {
		free(uris[i]);
	}
	free(uris);
	return 0;
}
