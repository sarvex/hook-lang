#!/usr/bin/env bash

VERSION_FILENAME="src/version.h"
DEFAULT_BUILD_TYPE="Debug"

update_revision() {
  revision="$(git rev-parse --short HEAD)"
  echo "#ifndef VERSION_H" > $VERSION_FILENAME
  echo "#define VERSION_H" >> $VERSION_FILENAME
  echo "#define VERSION \"0.1.0\"" >> $VERSION_FILENAME
  echo "#define REVISION \"($revision)\"" >> $VERSION_FILENAME
  echo "#endif" >> $VERSION_FILENAME
}

discard_changes() {
  git checkout -- "$VERSION_FILENAME"
}

cmake_build() {
  build_type="$1"
  with_extensions="$2"
  install_prefix="$3"

  if [ -z "$build_type" ]; then
    build_type="$DEFAULT_BUILD_TYPE"
  fi
  if [ "$with_extensions" == "with-extensions" ]; then
    with_extensions="-DBUILD_EXTENSIONS=1"
  else
    with_extensions=""
  fi

  update_revision

  if [ -z "$install_prefix" ]; then
    cmake -B build -DCMAKE_BUILD_TYPE=$build_type $with_extensions
  else
    cmake -B build -DCMAKE_BUILD_TYPE=$build_type $with_extensions -DCMAKE_INSTALL_PREFIX=$install_prefix
  fi

  cmake --build build

  discard_changes "$FILENAME"
}

cmake_build_and_install() {
  build_type="$1"
  with_extensions="$2"
  install_prefix="$3"
  cmake_build $build_type $with_extensions $install_prefix
  cmake --install build
}

cmake_build_and_pack() {
  build_type="$1"
  with_extensions="$2"
  cmake_build $build_type $with_extensions
  cpack --config build/CPackConfig.cmake
}
