#!/usr/bin/env python

from waflib import Build, Logs, Options
from waflib.extras import autowaf

# Library and package version (UNIX style major, minor, micro)
# major increment <=> incompatible changes
# minor increment <=> compatible changes (additions)
# micro increment <=> no interface changes
SRATOM_VERSION       = '0.6.8'
SRATOM_MAJOR_VERSION = '0'

# Mandatory waf variables
APPNAME = 'sratom'        # Package name for waf dist
VERSION = SRATOM_VERSION  # Package version for waf dist
top     = '.'             # Source directory
out     = 'build'         # Build directory

# Release variables
uri          = 'http://drobilla.net/sw/sratom'
dist_pattern = 'http://download.drobilla.net/sratom-%d.%d.%d.tar.bz2'
post_tags    = ['Hacking', 'LAD', 'LV2', 'RDF', 'Sratom']


def options(ctx):
    ctx.load('compiler_c')
    ctx.add_flags(
        ctx.configuration_options(),
        {'static':    'build static library',
         'no-shared': 'do not build shared library'})


def configure(conf):
    conf.load('compiler_c', cache=True)
    conf.load('autowaf', cache=True)
    autowaf.set_c_lang(conf, 'c99')

    conf.env.BUILD_SHARED = not Options.options.no_shared
    conf.env.BUILD_STATIC = Options.options.static

    if not conf.env.BUILD_SHARED and not conf.env.BUILD_STATIC:
        conf.fatal('Neither a shared nor a static build requested')

    if conf.env.DOCS:
        conf.load('sphinx')

    if Options.options.strict:
        # Check for programs used by lint target
        conf.find_program("flake8", var="FLAKE8", mandatory=False)
        conf.find_program("clang-tidy", var="CLANG_TIDY", mandatory=False)
        conf.find_program("iwyu_tool", var="IWYU_TOOL", mandatory=False)

    if Options.options.ultra_strict:
        autowaf.add_compiler_flags(conf.env, 'c', {
            'gcc': [
                '-Wno-cast-align',
                '-Wno-cast-qual',
                '-Wno-conversion',
                '-Wno-padded',
                '-Wno-suggest-attribute=pure',
            ],
            'clang': [
                '-Wno-cast-align',
                '-Wno-cast-qual',
                '-Wno-double-promotion',
                '-Wno-float-conversion',
                '-Wno-implicit-float-conversion',
                '-Wno-implicit-int-conversion',
                '-Wno-nullability-extension',
                '-Wno-nullable-to-nonnull-conversion',
                '-Wno-padded',
                '-Wno-shorten-64-to-32',
                '-Wno-sign-conversion',
            ],
            'msvc': [
                '/wd4242'  # conversion with possible loss of data
            ]
        })

    conf.check_pkg('lv2 >= 1.16.0', uselib_store='LV2')
    conf.check_pkg('serd-0 >= 0.30.0', uselib_store='SERD')
    conf.check_pkg('sord-0 >= 0.14.0', uselib_store='SORD')

    # Set up environment for building/using as a subproject
    autowaf.set_lib_env(conf, 'sratom', SRATOM_VERSION,
                        include_path=str(conf.path.find_node('include')))

    autowaf.display_summary(conf, {'Unit tests': bool(conf.env.BUILD_TESTS)})


lib_source = ['src/sratom.c']


def build(bld):
    # C Headers
    includedir = '${INCLUDEDIR}/sratom-%s/sratom' % SRATOM_MAJOR_VERSION
    bld.install_files(includedir, bld.path.ant_glob('include/sratom/*.h'))

    # Pkgconfig file
    autowaf.build_pc(bld, 'SRATOM', SRATOM_VERSION, SRATOM_MAJOR_VERSION, [],
                     {'SRATOM_MAJOR_VERSION': SRATOM_MAJOR_VERSION,
                      'SRATOM_PKG_DEPS': 'lv2 serd-0 sord-0'})

    libflags = ['-fvisibility=hidden']
    libs     = ['m']
    defines  = []
    if bld.env.MSVC_COMPILER:
        libflags = []
        libs     = []
        defines  = []

    # Shared Library
    if bld.env.BUILD_SHARED:
        bld(features        = 'c cshlib',
            export_includes = ['include'],
            source          = lib_source,
            includes        = ['include'],
            lib             = libs,
            uselib          = 'SERD SORD LV2',
            name            = 'libsratom',
            target          = 'sratom-%s' % SRATOM_MAJOR_VERSION,
            vnum            = SRATOM_VERSION,
            install_path    = '${LIBDIR}',
            defines         = defines + ['SRATOM_INTERNAL'],
            cflags          = libflags)

    # Static library
    if bld.env.BUILD_STATIC:
        bld(features        = 'c cstlib',
            export_includes = ['include'],
            source          = lib_source,
            includes        = ['include'],
            lib             = libs,
            uselib          = 'SERD SORD LV2',
            name            = 'libsratom_static',
            target          = 'sratom-%s' % SRATOM_MAJOR_VERSION,
            vnum            = SRATOM_VERSION,
            install_path    = '${LIBDIR}',
            defines         = defines + ['SRATOM_STATIC', 'SRATOM_INTERNAL'])

    if bld.env.BUILD_TESTS:
        test_libs   = libs
        test_cflags = ['']
        test_linkflags  = ['']
        if not bld.env.NO_COVERAGE:
            test_cflags    += ['--coverage']
            test_linkflags += ['--coverage']

        # Static library (for unit test code coverage)
        bld(features     = 'c cstlib',
            source       = lib_source,
            includes     = ['include'],
            lib          = test_libs,
            uselib       = 'SERD SORD LV2',
            name         = 'libsratom_profiled',
            target       = 'sratom_profiled',
            install_path = '',
            defines      = defines + ['SRATOM_STATIC', 'SRATOM_INTERNAL'],
            cflags       = test_cflags,
            linkflags    = test_linkflags)

        # Unit test program
        bld(features     = 'c cprogram',
            source       = 'test/test_sratom.c',
            includes     = ['include'],
            use          = 'libsratom_profiled',
            lib          = test_libs,
            uselib       = 'SERD SORD LV2',
            target       = 'test_sratom',
            install_path = '',
            defines      = defines + ['SRATOM_STATIC'],
            cflags       = test_cflags,
            linkflags    = test_linkflags)

    # Documentation
    if bld.env.DOCS:
        bld.recurse('doc/c')

    bld.add_post_fun(autowaf.run_ldconfig)


def test(tst):
    import sys

    with tst.group('Integration') as check:
        check(['./test_sratom'])


class LintContext(Build.BuildContext):
    fun = cmd = 'lint'


def lint(ctx):
    "checks code for style issues"
    import glob
    import os
    import subprocess
    import sys

    st = 0

    if "FLAKE8" in ctx.env:
        Logs.info("Running flake8")
        st = subprocess.call([ctx.env.FLAKE8[0],
                              "wscript",
                              "--ignore",
                              "E101,E129,W191,E221,W504,E251,E241,E741"])
    else:
        Logs.warn("Not running flake8")

    if "IWYU_TOOL" in ctx.env:
        Logs.info("Running include-what-you-use")
        cmd = [ctx.env.IWYU_TOOL[0], "-o", "clang", "-p", "build"]
        output = subprocess.check_output(cmd).decode('utf-8')
        if 'error: ' in output:
            sys.stdout.write(output)
            st += 1
    else:
        Logs.warn("Not running include-what-you-use")

    if "CLANG_TIDY" in ctx.env and "clang" in ctx.env.CC[0]:
        Logs.info("Running clang-tidy")
        sources = glob.glob('src/*.c') + glob.glob('test/*.c')
        sources = list(map(os.path.abspath, sources))
        procs = []
        for source in sources:
            cmd = [ctx.env.CLANG_TIDY[0], "--quiet", "-p=.", source]
            procs += [subprocess.Popen(cmd, cwd="build")]

        for proc in procs:
            stdout, stderr = proc.communicate()
            st += proc.returncode
    else:
        Logs.warn("Not running clang-tidy")

    if st != 0:
        sys.exit(st)
