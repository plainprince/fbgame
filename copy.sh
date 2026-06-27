#!/bin/bash
rsync -av --exclude='copy.sh' --exclude='.git' --exclude='.DS_Store' . plainprince@raspzero:/home/plainprince/fbgame
