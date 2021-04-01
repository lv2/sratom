project('sratom', ['c'],
        version: '1.0.0',
        license: 'ISC',
        meson_version: '>= 0.49.2',
        default_options: [
          'b_ndebug=if-release',
          'buildtype=release',
          'c_std=c99',
          'default_library=shared',
          'warning_level=3',
        ])

sratom_src_root = meson.current_source_dir()
major_version = meson.project_version().split('.')[0]
version_suffix = '-@0@'.format(major_version)
versioned_name = 'sratom' + version_suffix

# Load build tools
pkg = import('pkgconfig')
cc = meson.get_compiler('c')

if add_languages('cpp', required: get_option('bindings_cpp'))
  cpp = meson.get_compiler('cpp')
endif

# Set ultra strict warnings for developers, if requested
c_warnings = []
if get_option('strict')
  subdir('meson')

  c_warnings = all_c_warnings
  if cc.get_id() == 'clang'
    c_warnings += [
      '-Wno-cast-align',
      '-Wno-cast-qual',
      '-Wno-documentation-unknown-command',
      '-Wno-double-promotion',
      '-Wno-float-conversion',
      '-Wno-implicit-float-conversion',
      '-Wno-implicit-int-conversion',
      '-Wno-nullability-extension',
      '-Wno-nullable-to-nonnull-conversion',
      '-Wno-padded',
      '-Wno-shorten-64-to-32',
      '-Wno-sign-conversion',
    ]
  elif cc.get_id() == 'gcc'
    c_warnings += [
      '-Wno-cast-align',
      '-Wno-cast-qual',
      '-Wno-conversion',
      '-Wno-padded',
      '-Wno-suggest-attribute=pure',
      '-Wno-unsuffixed-float-constants',
      '-Wno-unused-const-variable',

      # FIXME: Just for tests
      '-Wno-parentheses',
    ]
  elif cc.get_id() == 'msvc'
    c_warnings += [
      '/wd4242'  # conversion with possible loss of data
    ]
  endif

else
  if cc.get_id() == 'clang'
    c_warnings += [
      '-Wno-nullability-extension',
    ]
  endif
endif

add_project_arguments(cc.get_supported_arguments(c_warnings),
                      language: ['c'])

# Add special arguments for MSVC
if cc.get_id() == 'msvc'
  msvc_args = [
    '/D_CRT_SECURE_NO_WARNINGS',
    '/TP',
    '/experimental:external',
    '/external:W0',
    '/external:anglebrackets',
  ]

  add_project_arguments(msvc_args, language: ['c'])
endif

sratom_c_headers = ['include/sratom/sratom.h']
sratom_c_header_files = files(sratom_c_headers)

sources = [
  'src/dumper.c',
  'src/loader.c',
]

# System libraries
m_dep = cc.find_library('m', required: false)

# Dependencies

lv2_dep = dependency('lv2',
                     version: '>= 1.18.2',
                     fallback: ['lv2', 'lv2_dep'])

serd_dep = dependency('serd-1',
                      version: '>= 1.0.0',
                      fallback: ['serd', 'serd_dep'])

# Determine library type and the flags needed to build it
if get_option('default_library') == 'both'
  if host_machine.system() == 'windows'
    error('default_library=both is not supported on Windows')
  endif

  library_type = 'both_libraries'
  library_args = ['-DSRATOM_INTERNAL']
  prog_args = []
elif get_option('default_library') == 'shared'
  library_type = 'shared_library'
  library_args = ['-DSRATOM_INTERNAL']
  prog_args = []
else
  library_type = 'static_library'
  library_args = ['-DSRATOM_INTERNAL', '-DSRATOM_STATIC']
  prog_args = ['-DSRATOM_STATIC']
endif

# Build shared and/or static library/libraries
libsratom = build_target(
  versioned_name,
  sources,
  version: meson.project_version(),
  include_directories: include_directories(['include']),
  c_args: library_args,
  dependencies: [m_dep, lv2_dep, serd_dep],
  gnu_symbol_visibility: 'hidden',
  install: true,
  target_type: library_type)

sratom_dep = declare_dependency(
  include_directories: include_directories(['include']),
  dependencies: [m_dep, lv2_dep, serd_dep],
  link_with: libsratom)

pkg.generate(
  libsratom,
  name: 'Sratom',
  filebase: versioned_name,
  subdirs: [versioned_name],
  version: meson.project_version(),
  description: 'A library for serializing LV2 atoms')

# Install header to a versioned include directory
install_headers(sratom_c_header_files, subdir: versioned_name / 'sratom')

if not get_option('docs').disabled()
  subdir('doc')
endif

if get_option('tests')
  subdir('test')
endif

# C++ bindings
if is_variable('cpp')
  subdir('bindings/cpp')
endif

if not meson.is_subproject() and meson.version().version_compare('>=0.53.0')
  summary('Tests', get_option('tests'), bool_yn: true)

  summary('Install prefix', get_option('prefix'))

  summary('Headers', get_option('prefix') / get_option('includedir'))
  summary('Libraries', get_option('prefix') / get_option('libdir'))
endif

# meson.override_dependency(versioned_name, sratom_dep)
