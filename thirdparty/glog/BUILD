include('../foreign_build.bld')

_GFLAGS_PATH = get_install_dir('gflags')
_is_linux = blade.cc_toolchain.target_os == 'linux'

cmake_build(
    name='glog_build',
    source_package='glog-0.7.1.tar.gz',
    package_name='glog',
    lib_names=['glog'],
    cmake_options=[
        '-DWITH_GFLAGS=ON',
        '-DWITH_GTEST=OFF',
        '-DWITH_UNWIND=none',
        '-DBUILD_TESTING=OFF',
        # Build shared, with a small patch (build_static_with_shared.patch) so
        # a libglog.a is also emitted alongside libglog.{so,dylib}. We need
        # both: tests/libs dynamically link the shared lib to keep glog a
        # single instance — otherwise glog gets statically embedded in
        # multiple DSOs and gflags reports duplicate flag registration
        # ("linked both statically and dynamically") at startup.
        '-DBUILD_SHARED_LIBS=ON',
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        # Point cmake directly at gflags' install-tree config dir, not the
        # parent of both install/ and gflags_build/. Without this, cmake may
        # pick gflags' BUILD-tree gflags-targets.cmake (which records absolute
        # paths into .../gflags/gflags_build/lib/libgflags.dylib). That would
        # make libglog reference the build-dir copy of libgflags.dylib while
        # consumers reference the install copy at .../gflags/lib/libgflags.dylib
        # — dyld then loads both paths as two separate libraries and gflags
        # aborts with "linked both statically and dynamically".
        '-Dgflags_DIR=' + _GFLAGS_PATH + '/lib/cmake/gflags',
    ],
    deps=[
        '//thirdparty/gflags:gflags',
    ],
    strip_include_prefix='glog',
    patches=['no_export_guard.patch', 'build_static_with_shared.patch'],
    generate_dynamic=True,
)

foreign_cc_library(
    name='glog',
    deps=[
        ':glog_build',
    ] + (['#pthread', '#rt'] if _is_linux else []),
    has_dynamic=True,
    visibility='PUBLIC',
)

cc_test(name='glog_test',
        srcs='glog_test.cpp',
        deps=[
            ':glog',
            '//thirdparty/gflags:gflags',
        ] + (['#pthread'] if _is_linux else []))

cc_binary(name='press',
          srcs='press.cpp',
          deps=[
              ':glog',
              '//thirdparty/gflags:gflags',
          ] + (['#pthread'] if _is_linux else []))
