#!/usr/bin/env bash
#
# Wrapper to reduce of command line args need to run cj-gen-bipartite.
# In addition c,k,t are fractions (0,1] of the max allowed values given the
# other arguments.

if [ $# -ne 6 ]; then
  >&2 echo "Usage: $0 n dx c-fraction k-fraction t-fraction seed."
  exit 1
fi

n=$1
dx=$2
c=$3
k=$4
t=$5
s=$6
dy=`echo "scale=0; ($dx * 2)/1" | bc`
c=`echo "scale=0; (($n*$n - $n) * $c)/1" | bc`
k=`echo "scale=0; ($c * $k)/1" | bc`
tx=`echo "scale=0; ($dx*$dx * $t)/1" | bc`
ty=`echo "scale=0; ($dy*$dy * $t)/1" | bc`
txy=`echo "scale=0; ($dx*$dy * $t)/1" | bc`

cj-gen-bipartite -n $n -N $n -d $dx -D $dy -c $c -C $c -k $k -K $k -t $tx -T $ty --num_constraints $c --num_constraint_types $k --num_no_goods $txy --seed $s
