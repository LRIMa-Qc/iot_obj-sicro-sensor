{
  description = "nRF firmware project";

  # Single project-owned input: nix-nrf-dev. The generated flake intentionally
  # avoids a second Nixpkgs/flake-utils input; nix-nrf-dev pins its own and
  # consumers can follow it if they want to reuse their own Nixpkgs revision.
  inputs.nix-nrf-dev.url = "github:qarnet/nix-nrf-dev";

  outputs =
    { nix-nrf-dev, ... }:
    let
      # nix-nrf-dev pins its own nixpkgs; reuse it for host tool packages.
      # pkgs = nix-nrf-dev.inputs.nixpkgs.legacyPackages.x86_64-linux;
    in
    {
      devShells.x86_64-linux.default = nix-nrf-dev.lib.x86_64-linux.mkNrfShell {
        # west backend: Nix provides python/cmake/ninja/Zephyr SDK (NixOS-safe).
        # nrfutil backend's Nordic toolchain binaries are dynamically linked and
        # cannot run on this NixOS host (stub-ld rejects them).
        backend = "west";
        ncsVersion = "v3.3.0";
        # list_boards.py imports jsonschema, but no NCS requirements file ships
        # it. Provide a Nix-packaged python (3.12, matching the venv) with
        # jsonschema on the shell PATH.
        # packages = [(pkgs.python312.withPackages (ps: [ps.jsonschema]))];
        # autoBootstrap = true;
        # extraShellHook = "";
      };
    };
}
