# glog via vcpkg (vcpkg#glog:glog, 0.7.1, linkage='auto'). glog references
# gflags (FlagRegisterer etc.), so depend on the gflags wrapper -- the vcpkg
# VcpkgLibrary targets carry no inter-port deps, so a static glog.a needs
# gflags.a explicitly on the link line (the old shared glog.dylib resolved it
# internally). //thirdparty/glog:glog unchanged for consumers.
cc_library(name='glog', hdrs=[], deps=['vcpkg#glog:glog',
                                       '//thirdparty/gflags:gflags'],
           visibility='PUBLIC')
