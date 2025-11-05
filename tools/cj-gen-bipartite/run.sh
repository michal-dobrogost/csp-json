#! /usr/bin/env nix-shell
#! nix-shell -i bash --pure -p zig
zig build run -- -n 2 -N 2 -d 4 -D 5 -c 1 -C 1 -k 1 -K 1 -t 8 -T 12 --num_constraints 2 --num_constraint_types 1 --num_no_goods 10
