# Copyright 2021-2026 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

###################
# Header Warnings #
###################

subdir('headers')

##############
# Unit Tests #
##############

unit_test_names = [
  'trip',
  'write',
]

foreach name : unit_test_names
  test(
    name,
    executable(
      'test_' + name,
      files('test_@0@.c'.format(name), 'test_uri_map.c'),
      c_args: c_suppressions,
      dependencies: [lv2_dep, serd_dep, sratom_dep],
      implicit_include_directories: false,
    ),
    suite: 'unit',
  )
endforeach

########
# Lint #
########

if get_option('lint')
  subdir('lint')
endif
