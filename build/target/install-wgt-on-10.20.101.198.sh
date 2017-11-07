#!/bin/sh
#
# File:   install-wgt-on-target.sh
# Author: Sebastien Douheret @ IoT.bzh
# Object: install widget on target
# Created on 24-May-2017, 09:23:37
# Usage:

# Do not change manually use 'make remote-target-populate'
export RSYNC_TARGET=10.20.101.198
export WGT_FILE_L=/home/fulup/Workspace/Audio-4a/wsclient-audio4a/build/libclient-audio4a.wgt
export WGT_FILE_T=/tmp/libclient-audio4a.wgt

scp $WGT_FILE_L $RSYNC_TARGET:$WGT_FILE_T \
    && ssh -o "StrictHostKeyChecking no" -tt $RSYNC_TARGET -- \
        afm-util install $WGT_FILE_T

#    && rm -f $WGT_FILE_T

