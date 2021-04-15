#!/bin/zsh
tmux new-window "make qemu-gdb"
tmux split-window -h "make gdb"
