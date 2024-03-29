# ChCore

[![Build](https://github.com/SeanChao/chcore/actions/workflows/build.yml/badge.svg)](https://github.com/SeanChao/chcore/actions/workflows/build.yml)
[![Test](https://github.com/SeanChao/chcore/actions/workflows/test.yml/badge.svg)](https://github.com/SeanChao/chcore/actions/workflows/test.yml)

This is the repository of ChCore labs in SE315, 2020 Spring.

[DOCS](docs.md)

## build 
  - `make` or `make build`
  - The project will be built in `build` directory.

## Emulate
  - `make qemu`

  Emulate ChCore in QEMU

## Debug with GBD

  - `make qemu-gdb`

  Start a GDB server running ChCore
  
  - `make gdb`
  
  Start a GDB (gdb-multiarch) client

## Grade
  - `make grade`
  
  Show your grade of labs in the current branch

## Other
  - type `Ctrl+a x` to quit QEMU
  - type `Ctrl+d` to quit GDB
  