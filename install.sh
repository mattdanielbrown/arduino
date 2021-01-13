#!/bin/bash
set -x
mv ~/Documents/Arduino ~/Documents/Arduino-backup
ln -svf "$(pwd)" ~/Documents/Arduino
