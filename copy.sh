#!/bin/bash
scp -r $(find . -maxdepth 1 ! -name copy.sh ! -name . | sed 's|^\./||' | tr '\n' ' ') plainprince@raspzero:/home/plainprince/fbgame
