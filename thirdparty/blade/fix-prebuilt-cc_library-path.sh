#!/bin/bash
# Fix all prebuilt cc_library library paths.
#
# Rules:
#   cp lib32_release lib32
#   cp lib64_release lib64
#   if lib64_release links to lib64, consider it as already correct.

function copy_lib() {
  local bits="$1"
  local real_path
  for d in $(find -name "lib${bits}_release"); do
    target_path=${d%_release}
    if [[ -L $d ]]; then
      real_path=$(dirname $d)/$(readlink $d)
      real_path=${real_path%/}
      if [[ "$real_path" == "$target_path" ]]; then
        echo "$real_path already exists, skip"
        continue
      fi
      echo "find real_path $real_path"
    else
      real_path=$d
    fi

    if [[ -e $target_path ]]; then
      echo "$target_path already exists but maybe incorrect, remove it"
      svn rm $target_path
    fi

    svn cp "$real_path" "$target_path"
  done
}

copy_lib 32
copy_lib 64
