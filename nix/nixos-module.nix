self: {
  config,
  lib,
  pkgs,
  ...
}: let
  cfg = config.programs.cwcwm;
in {
  options = {
    programs.cwc = {
      enable = lib.mkEnableOption "Enable cwc wayland compositor";
      package = lib.mkOption {
        type = lib.types.package;
        default = self.packages.${pkgs.system}.cwc;
        description = "The cwc package to use";
      };
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [
      cfg.package
    ];

    xdg.portal = {
      enable = lib.mkDefault true;
      wlr.enable = lib.mkDefault true;
      configPackages = [cfg.package];
    };

    security.polkit.enable = true;

    programs.xwayland.enable = true;

    services = {
      displayManager.sessionPackages = [cfg.package];
      graphical-desktop.enable = lib.mkDefault true;
    };
  };
}
