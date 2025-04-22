#!/bin/bash
# Author: Odin Bjerke, odin.bjerke@uit.no
#
# Use of this script is entirely optional, and the only advantage of using it is not having to type out arguments.
# Edit it in any way you would like.
#

# binary targets
DEBUG_BIN="build/debug/indexer"
RELEASE_BIN="build/release/indexer"

# edit the *_ARGS below to your personal preference

# run the debug executable with these arguments.
# include .txt / .md files from 'data/enwiki/', limit to n=100 files
DEBUG_ARGS="data/enwiki/ --type txt md --limit 100"

# include all .txt and .md files from 'data/enwiki/', limit to n=100000 files
RELEASE_ARGS="data/enwiki/ --type txt md --limit 100000"

# If you want to run with a debugger as default, you could set this to something like..
# COMMAND_DEBUG="lldb $DEBUG_BIN -- $DEBUG_ARGS"
# COMMAND_DEBUG="gdb $DEBUG_BIN --args $DEBUG_ARGS"
COMMAND_DEBUG="$DEBUG_BIN $DEBUG_ARGS"

COMMAND_RELEASE="$RELEASE_BIN $RELEASE_ARGS"

# name of this script; to make it distinct if the output is from the binary or this script
SELF_NAME="$(basename "$0")"

if [ -f $RELEASE_BIN ]; then
  if [ -f $DEBUG_BIN ]; then
    echo "$SELF_NAME: Found multiple binary targets. Defaulting to release build at $RELEASE_BIN."
  fi
  echo "Running command: \"$COMMAND_RELEASE\""
  exec $COMMAND_RELEASE
elif [ -f $DEBUG_BIN ]; then
  echo "Running command: \"$COMMAND_DEBUG\""
  exec $COMMAND_DEBUG
else
  echo "$SELF_NAME: Error - No binary target found. Compile with make before running this script."
  echo "Note: Set the 'DEBUG' variable in the Makefile to control whether to build for debugging or optimization."
  exit 1
fi
