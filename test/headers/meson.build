# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

header_c_suppressions = []

if get_option('warning_level') == 'everything'
  if cc.get_id() == 'clang'
    if not meson.is_cross_build()
      c_suppressions += ['-Wno-poison-system-directories']
    endif
  elif cc.get_id() == 'msvc'
    header_c_suppressions += [
      '/wd4820', # padding added after construct
    ]
  endif
endif

header_c_suppressions = cc.get_supported_arguments(header_c_suppressions)

test_headers_source = files('test_headers.c')

test(
  'headers',
  executable(
    'test_headers',
    test_headers_source,
    c_args: header_c_suppressions,
    dependencies: [sratom_dep],
  ),
  suite: 'unit',
)
