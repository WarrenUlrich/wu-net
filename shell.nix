with import <nixpkgs> {};

pkgs.mkShell rec {
  nativeBuildInputs = [ cmake pkg-config ];

  buildInputs = [
    openssl
  ];
}
