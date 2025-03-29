{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = [
    pkgs.gcc
    pkgs.clang-tools
    pkgs.lldb
    pkgs.git
    pkgs.cmake
    pkgs.ninja
    pkgs.cmake-language-server
    pkgs.nixd
  ];

  BUILD_DIR = "build";
}
