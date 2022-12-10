# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

header_c_suppressions = []

if get_option('strict')
  if cc.get_id() == 'clang'
    header_c_suppressions += [
      '-Wno-cast-align', # LV2
      '-Wno-cast-qual', # LV2
      '-Wno-declaration-after-statement', # LV2
    ]
  elif cc.get_id() == 'gcc'
    header_c_suppressions += [
      '-Wno-cast-align', # LV2
      '-Wno-cast-qual', # LV2
      '-Wno-padded', # Serd
      '-Wno-sign-conversion', # LV2
      '-Wno-unused-const-variable', # Serd
    ]
  elif cc.get_id() == 'msvc'
    header_c_suppressions += [
      '/wd4820', # padding added after construct
    ]
  endif
endif

if cc.get_id() == 'clang'
  header_c_suppressions += [
    '-Wno-nullability-extension',
  ]
endif

test(
  'headers',
  executable(
    'test_headers',
    files('test_headers.c'),
    c_args: header_c_suppressions,
    dependencies: [sratom_dep],
  ),
  suite: 'unit',
)
