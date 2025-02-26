# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

header_c_suppressions = []

if get_option('warning_level') == 'everything'
  if cc.get_id() == 'clang'
    if not meson.is_cross_build()
      header_c_suppressions += ['-Wno-poison-system-directories']
    endif
    header_c_suppressions += ['-Wno-poison-system-directories']
  elif cc.get_id() == 'msvc'
    header_c_suppressions += [
      '/wd4820', # padding added after construct
    ]
  endif
endif

header_c_suppressions = cc.get_supported_arguments(header_c_suppressions)

test(
  'headers',
  executable(
    'test_headers',
    files('test_headers.c'),
    c_args: header_c_suppressions,
    dependencies: [sratom_dep],
    implicit_include_directories: false,
  ),
  suite: 'unit',
)
