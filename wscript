#!/usr/bin/env python
import glob
import os
import shutil
import subprocess
import sys

from waflib.extras import autowaf as autowaf
import waflib.Logs as Logs, waflib.Options as Options

# Version of this package (even if built as a child)
SERIATOM_VERSION       = '0.0.0'
SERIATOM_MAJOR_VERSION = '0'

# Library version (UNIX style major, minor, micro)
# major increment <=> incompatible changes
# minor increment <=> compatible changes (additions)
# micro increment <=> no interface changes
# Seriatom uses the same version number for both library and package
SERIATOM_LIB_VERSION = SERIATOM_VERSION

# Variables for 'waf dist'
APPNAME = 'seriatom'
VERSION = SERIATOM_VERSION

# Mandatory variables
top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')
    autowaf.set_options(opt)
    opt.add_option('--test', action='store_true', default=False, dest='build_tests',
                   help="Build unit tests")
    opt.add_option('--static', action='store_true', default=False, dest='static',
                   help="Build static library")

def configure(conf):
    conf.load('compiler_c')
    autowaf.configure(conf)
    autowaf.display_header('Seriatom Configuration')

    if conf.env['MSVC_COMPILER']:
        conf.env.append_unique('CFLAGS', ['-TP', '-MD'])
    else:
        conf.env.append_unique('CFLAGS', '-std=c99')

    conf.env['BUILD_TESTS']  = Options.options.build_tests
    conf.env['BUILD_STATIC'] = Options.options.static

    # Check for gcov library (for test coverage)
    if conf.env['BUILD_TESTS']:
        conf.check_cc(lib='gcov',
                      define_name='HAVE_GCOV',
                      mandatory=False)

    autowaf.check_pkg(conf, 'serd-0', uselib_store='SERD',
                      atleast_version='0.8.0', mandatory=True)

    autowaf.define(conf, 'SERIATOM_VERSION', SERIATOM_VERSION)
    conf.write_config_header('seriatom_config.h', remove=False)

    autowaf.display_msg(conf, "Unit tests", str(conf.env['BUILD_TESTS']))
    print('')

lib_source = [
    'src/atom_to_rdf.c'
]

def build(bld):
    # C Headers
    includedir = '${INCLUDEDIR}/seriatom-%s/seriatom' % SERIATOM_MAJOR_VERSION
    bld.install_files(includedir, bld.path.ant_glob('seriatom/*.h'))

    # Pkgconfig file
    autowaf.build_pc(bld, 'SERIATOM', SERIATOM_VERSION, SERIATOM_MAJOR_VERSION,
                     'SERD',
                     {'SERIATOM_MAJOR_VERSION' : SERIATOM_MAJOR_VERSION})

    libflags = [ '-fvisibility=hidden' ]
    libs     = [ 'm' ]
    if bld.env['MSVC_COMPILER']:
        libflags = []
        libs     = []

    # Shared Library
    obj = bld(features        = 'c cshlib',
              export_includes = ['.'],
              source          = lib_source,
              includes        = ['.', './src'],
              lib             = libs,
              name            = 'libseriatom',
              target          = 'seriatom-%s' % SERIATOM_MAJOR_VERSION,
              vnum            = SERIATOM_LIB_VERSION,
              install_path    = '${LIBDIR}',
              cflags          = libflags + [ '-DSERIATOM_SHARED',
                                             '-DSERIATOM_INTERNAL' ])
    autowaf.use_lib(bld, obj, 'SERD')

    # Static library
    if bld.env['BUILD_STATIC']:
        obj = bld(features        = 'c cstlib',
                  export_includes = ['.'],
                  source          = lib_source,
                  includes        = ['.', './src'],
                  lib             = libs,
                  name            = 'libseriatom_static',
                  target          = 'seriatom-%s' % SERIATOM_MAJOR_VERSION,
                  vnum            = SERIATOM_LIB_VERSION,
                  install_path    = '${LIBDIR}',
                  cflags          = ['-DSERIATOM_INTERNAL'])
        autowaf.use_lib(bld, obj, 'SERD')

    if bld.env['BUILD_TESTS']:
        test_libs   = libs
        test_cflags = ['']
        if bld.is_defined('HAVE_GCOV'):
            test_libs   += ['gcov']
            test_cflags += ['-fprofile-arcs', '-ftest-coverage']

        # Static library (for unit test code coverage)
        obj = bld(features     = 'c cstlib',
                  source       = lib_source,
                  includes     = ['.', './src'],
                  lib          = test_libs,
                  name         = 'libseriatom_profiled',
                  target       = 'seriatom_profiled',
                  install_path = '',
                  cflags       = test_cflags + ['-DSERIATOM_INTERNAL'])
        autowaf.use_lib(bld, obj, 'SERD')

        # Unit test program
        obj = bld(features     = 'c cprogram',
                  source       = 'tests/seriatom_test.c',
                  includes     = ['.', './src'],
                  use          = 'libseriatom_profiled',
                  lib          = test_libs,
                  target       = 'seriatom_test',
                  install_path = '',
                  cflags       = test_cflags)

    # Documentation
    autowaf.build_dox(bld, 'SERIATOM', SERIATOM_VERSION, top, out)

    bld.add_post_fun(autowaf.run_ldconfig)
    if bld.env['DOCS']:
        bld.add_post_fun(fix_docs)

def test(ctx):
    autowaf.pre_test(ctx, APPNAME)
    os.environ['PATH'] = '.' + os.pathsep + os.getenv('PATH')
    autowaf.run_tests(ctx, APPNAME, ['seriatom_test'], dirs=['./src','./tests'])
    autowaf.post_test(ctx, APPNAME)

def lint(ctx):
    subprocess.call('cpplint.py --filter=+whitespace/comments,-whitespace/tab,-whitespace/braces,-whitespace/labels,-build/header_guard,-readability/casting,-readability/todo,-build/include src/* seriatom/*', shell=True)

def build_dir(ctx, subdir):
    if autowaf.is_child():
        return os.path.join('build', APPNAME, subdir)
    else:
        return os.path.join('build', subdir)

def fix_docs(ctx):
    try:
        top = os.getcwd()
        os.chdir(build_dir(ctx, 'doc/html'))
        os.system("sed -i 's/SERIATOM_API //' group__seriatom.html")
        os.system("sed -i 's/SERIATOM_DEPRECATED //' group__seriatom.html")
        os.remove('index.html')
        os.symlink('group__seriatom.html',
                   'index.html')
        os.chdir(top)
        os.chdir(build_dir(ctx, 'doc/man/man3'))
        os.system("sed -i 's/SERIATOM_API //' seriatom.3")
        os.chdir(top)
    except:
        Logs.error("Failed to fix up %s documentation" % APPNAME)

def upload_docs(ctx):
    os.system("rsync -ravz --delete -e ssh build/doc/html/ drobilla@drobilla.net:~/drobilla.net/docs/seriatom/")
