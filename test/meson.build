# Copyright 2021-2024 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

###################
# Header Warnings #
###################

subdir('headers')

##############
# Unit Tests #
##############

test_sratom_source = files('test_sratom.c')

test(
  'sratom',
  executable(
    'test_sratom',
    test_sratom_source,
    c_args: c_suppressions,
    dependencies: [sratom_dep, serd_dep, sord_dep],
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
