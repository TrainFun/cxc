#!/bin/env -S bash -e
if [[ $* = *--verbose* ]]; then
	verbose=1
fi

if [[ $* = *--continue* ]]; then
	set +e
fi

i=1
code="test/$1_$i.c"
while [[ -e $code ]]; do
	printf "\033[32;1m


================================================
|					      |
|                  TEST %3d                   |
|					      |
================================================
\033[93;3m" $i
	if [[ $verbose -eq 1 ]]; then
		printf "\033[91m"
		./bin/main $code > /tmp/cxcode
		printf "\033[93m"
	else
		./bin/main $code > /tmp/cxcode 2>/dev/null
	fi
	input="test/$1_$i.in"
	if [[ -e $input ]] ; then
		lli /tmp/cxcode < $input > /tmp/cxout
	else
		lli /tmp/cxcode > /tmp/cxout
	fi
	printf "\033[0m"
	diff -y --suppress-common-lines --color=always /tmp/cxout "test/$1_$i.out"

	i=$((i+1))
	code="test/$1_$i.c"
done
