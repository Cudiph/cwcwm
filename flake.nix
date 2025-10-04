{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs = {
    self,
    flake-parts,
    ...
  } @ inputs:
    flake-parts.lib.mkFlake {inherit inputs;} {
      imports = [
        inputs.flake-parts.flakeModules.easyOverlay
      ];

      flake = {
        nixosModules.cwc = import ./nix/nixos-module.nix self;
      };

      perSystem = {
        config,
        pkgs,
        ...
      }: let
        inherit
          (pkgs)
          callPackage
          ;
        cwc = callPackage ./nix/default.nix {};
        shellOverride = old: {
          nativeBuildInputs = old.nativeBuildInputs ++ [];
          buildInputs = old.buildInputs ++ [];
        };
      in {
        packages.default = cwc;
        overlayAttrs = {
          inherit (config.packages) cwc;
        };
        packages = {
          inherit cwc;
        };
        devShells.default = cwc.overrideAttrs shellOverride;
        formatter = pkgs.alejandra;
      };
      systems = ["x86_64-linux"];
    };
}
