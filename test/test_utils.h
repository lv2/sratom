/*
  Copyright 2019-2021 David Robillard <d@drobilla.net>

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

#include "lv2/urid/urid.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/// Simple O(n) URI map
typedef struct {
  char**   uris;
  uint32_t n_uris;
} Uris;

static char*
copy_string(const char* str)
{
  const size_t len = strlen(str);
  char*        dup = (char*)malloc(len + 1);
  memcpy(dup, str, len + 1);
  return dup;
}

static LV2_URID
urid_map(LV2_URID_Map_Handle handle, const char* uri)
{
  Uris* const uris = (Uris*)handle;

  for (uint32_t i = 0; i < uris->n_uris; ++i) {
    if (!strcmp(uris->uris[i], uri)) {
      return i + 1;
    }
  }

  uris->uris = (char**)realloc(uris->uris, ++uris->n_uris * sizeof(char*));
  uris->uris[uris->n_uris - 1] = copy_string(uri);
  return uris->n_uris;
}

static const char*
urid_unmap(LV2_URID_Unmap_Handle handle, LV2_URID urid)
{
  Uris* const uris = (Uris*)handle;

  if (urid > 0 && urid <= uris->n_uris) {
    return uris->uris[urid - 1];
  }

  return NULL;
}
