#! /usr/bin/env nix-shell
#! nix-shell -i bash -p zig
n=2
dx=2
c=0.5
k=1
t=0.5
s=0
dy=`echo "scale=0; ($dx * 2)/1" | bc`
c=`echo "scale=0; (($n*$n - $n) * $c)/1" | bc`
k=`echo "scale=0; ($c * $k)/1" | bc`
tx=`echo "scale=0; ($dx*$dx * $t)/1" | bc`
ty=`echo "scale=0; ($dy*$dy * $t)/1" | bc`
txy=`echo "scale=0; ($dx*$dy * $t)/1" | bc`

zig build run -- -n $n -N $n -d $dx -D $dy -c $c -C $c -k $k -K $k -t $tx -T $ty --num_constraints $c --num_constraint_types $k --num_no_goods $txy --seed $s
