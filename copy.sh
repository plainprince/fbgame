#!/bin/bash
rsync -av --exclude='copy.sh' --exclude='.git' --exclude='.DS_Store' --exclude='*.o' --exclude=pongtrain . plainprince@raspzero:/home/plainprince/fbgame
