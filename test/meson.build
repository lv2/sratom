# Copyright 2021-2024 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

###################
# Header Warnings #
###################

subdir('headers')

##############
# Unit Tests #
##############

test(
  'trip',
  executable(
    'test_trip',
    files('test_trip.c'),
    c_args: c_suppressions,
    dependencies: [lv2_dep, serd_dep, sratom_dep],
    implicit_include_directories: false,
  ),
  suite: 'unit',
)

########
# Lint #
########

if get_option('lint')
  subdir('lint')
endif
