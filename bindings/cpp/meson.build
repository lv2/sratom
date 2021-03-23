versioned_cpp_name = 'sratompp' + version_suffix

# Set ultra strict warnings for developers, if requested
cpp_suppressions = []
if get_option('strict')
  if cpp.get_id() == 'clang'
    cpp_suppressions += [
      '-Wno-documentation-unknown-command',
      '-Wno-format-nonliteral',
      '-Wno-nullability-extension',
      '-Wno-padded',
    ]
  elif cpp.get_id() == 'gcc'
    cpp_suppressions += [
      '-Wno-abi-tag',
      '-Wno-float-equal',
      '-Wno-inline',
      '-Wno-multiple-inheritance',
      '-Wno-padded',
      '-Wno-suggest-attribute=pure',
      '-Wno-switch-default',
      '-Wno-unused-const-variable',
    ]
  elif cpp.get_id() == 'msvc'
    cpp_suppressions += [
      '/wd4355',  # 'this' used in base member initializer list
      '/wd4571',  # structured exceptions are no longer caught
      '/wd4623',  # default constructor implicitly deleted
      '/wd4625',  # copy constructor implicitly deleted
      '/wd4626',  # assignment operator implicitly deleted
      '/wd4710',  # function not inlined
      '/wd4868',  # may not enforce left-to-right evaluation order
      '/wd5026',  # move constructor implicitly deleted
      '/wd5027',  # move assignment operator implicitly deleted
    ]
  endif
else
  if cpp.get_id() == 'clang'
    cpp_suppressions += [
      '-Wno-nullability-extension',
    ]
  endif
endif

exess_cpp_args = cpp.get_supported_arguments(cpp_suppressions)

cpp_headers = [
  'include/sratom/sratom.hpp',
]

serdpp_dep = dependency('serdxx-1',
                        version: '>= 1.0.0')

sratompp_dep = declare_dependency(
  include_directories: include_directories(['include']),
  link_with: libsratom)

pkg.generate(
  description: 'C++ bindings for sratom',
  filebase: versioned_cpp_name,
  name: 'Sratompp',
  subdirs: [versioned_cpp_name],
  version: meson.project_version())

# Install headers to a versioned include directory
install_headers(cpp_headers, subdir: versioned_cpp_name / 'sratom')

cpp_test_args = cpp.get_supported_arguments(['-Wno-float-equal'])

test(
  'bindings',
  executable(
    'test_sratom_hpp',
    'test/test_sratom_hpp.cpp',
    include_directories: include_directories(['include', '../../test']),
    cpp_args: exess_cpp_args + cpp_test_args,
    dependencies: [sratom_dep, serdpp_dep, sratompp_dep]),
  suite: 'cpp')
