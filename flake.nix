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

      perSystem = {
        config,
        pkgs,
        ...
      }: let
        inherit
          (pkgs)
          callPackage
          ;
        cwcwm = callPackage ./default.nix {};
        shellOverride = old: {
          nativeBuildInputs = old.nativeBuildInputs ++ [];
          BuildInputs = old.BuildInputs ++ [];
        };
      in {
        packages.default = cwcwm;
        overlayAttrs = {
          inherit (config.packages) cwcwm;
        };
        packages = {
          inherit cwcwm;
        };
        devShells.default = cwcwm.overrideAttrs shellOverride;
        formatter = pkgs.alejandra;
      };
      systems = ["x86_64-linux"];
    };
}
