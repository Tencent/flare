include('//thirdparty/foreign_build.bld')

_is_linux = blade.cc_toolchain.target_os == 'linux'

autotools_build(
    name = 'openssl_build',
    source_package = 'openssl-1.1.1w.tar.gz',
    package_name = 'openssl',
    lib_names = ['crypto', 'ssl'],
    configure_options = '-fPIC -g -shared ' +
      # Assembly implementation doesn't get along well with PIC code in AArch64.
      ('no-asm' if build_target.arch == 'aarch64' else ''),
    configure_file_name = 'config',
    strip_include_prefix = 'openssl',
    install_target = 'install_sw',
    generate_dynamic=True,
)

foreign_cc_library(
    name = 'crypto',
    deps = [
      ':openssl_build',
    ] + (['#dl', '#pthread'] if _is_linux else []),
    visibility = 'PUBLIC',
    has_dynamic=True,
)

foreign_cc_library(
    name = 'ssl',
    deps = [
      ':crypto',
      ':openssl_build',
    ],
    visibility = 'PUBLIC',
    has_dynamic=True,
)
