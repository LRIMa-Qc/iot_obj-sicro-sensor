FROM nixos/nix
COPY . .
RUN nix-channel --update

RUN nix-build -A pythonFull '<nixpkgs>'
