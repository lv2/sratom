# Copyright 2020-2024 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

# Project-specific warning suppressions

warning_level = get_option('warning_level')

#####
# C #
#####

c_suppressions = []

if cc.get_id() == 'clang'
  if warning_level == 'everything'
    c_suppressions += [
      '-Wno-cast-align',
      '-Wno-cast-function-type-strict',
      '-Wno-cast-qual',
      '-Wno-declaration-after-statement',
      '-Wno-documentation-unknown-command',
      '-Wno-double-promotion',
      '-Wno-float-conversion',
      '-Wno-implicit-float-conversion',
      '-Wno-implicit-int-conversion',
      '-Wno-padded',
      '-Wno-shorten-64-to-32',
      '-Wno-sign-conversion',
      '-Wno-unsafe-buffer-usage',
    ]

    if host_machine.system() == 'windows'
      c_suppressions += [
        '-Wno-deprecated-declarations',
        '-Wno-format-nonliteral',
      ]
    endif

    if not meson.is_cross_build()
      c_suppressions += [
        '-Wno-poison-system-directories',
      ]
    endif
  endif

  if warning_level in ['everything', '3']
    c_suppressions += [
      '-Wno-nullability-extension',
    ]
  endif

elif cc.get_id() == 'gcc'
  if warning_level == 'everything'
    c_suppressions += [
      '-Wno-cast-align',
      '-Wno-cast-qual',
      '-Wno-conversion',
      '-Wno-inline',
      '-Wno-padded',
      '-Wno-suggest-attribute=pure',
      '-Wno-unsuffixed-float-constants',
      '-Wno-unused-const-variable',
    ]

    if host_machine.system() == 'windows'
      c_suppressions += [
        '-Wno-suggest-attribute=format',
      ]
    endif
  endif

elif cc.get_id() == 'msvc'
  if warning_level == 'everything'
    c_suppressions += [
      '/wd4242', # conversion, possible loss of data
      '/wd4365', # signed/unsigned mismatch
      '/wd4514', # unreferenced inline function has been removed
      '/wd4706', # assignment within conditional expression
      '/wd4710', # function not inlined
      '/wd4711', # function selected for automatic inline expansion
      '/wd4820', # padding added after construct
      '/wd5045', # will insert Spectre mitigation for memory load
    ]
  endif

  if warning_level in ['everything', '3', '2']
    c_suppressions += [
      '/wd4267', # conversion from size_t, possible loss of data
      '/wd4996', # function or variable may be unsafe
    ]
  endif

  if warning_level in ['everything', '3', '2', '1']
    c_suppressions += [
      '/wd4244', # conversion from floating point, possible loss of data
    ]
  endif
endif

c_suppressions = cc.get_supported_arguments(c_suppressions)
