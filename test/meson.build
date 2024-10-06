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
    include_directories: include_directories('../src'),
  ),
  suite: 'unit',
)

########
# Lint #
########

if get_option('lint')
  all_sources = sources + test_sratom_source + test_headers_source

  if not meson.is_subproject()
    # Check release metadata
    autoship = find_program('autoship', required: false)
    if autoship.found()
      test('autoship', autoship, args: ['test', sratom_src_root], suite: 'data')
    endif
  endif

  # Check licensing metadata
  reuse = find_program('reuse', required: false)
  if reuse.found()
    reuse_args = ['--root', sratom_src_root, 'lint']
    test('REUSE', reuse, args: reuse_args, suite: 'data')
  endif

  # Check code formatting
  clang_format = find_program('clang-format', required: false)
  if clang_format.found()
    clang_format_args = ['--Werror', '--dry-run'] + c_headers + all_sources
    test('format', clang_format, args: clang_format_args, suite: 'code')
  endif
endif
