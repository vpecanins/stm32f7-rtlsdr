#!/bin/bash

# Script to tidy up the mess in the Components directory
# Basically put all the files inside Inc/ and Src/ hierarchy inside
# Components.
# Also delete HTML files (release notes)

# However: DO NOT USE THIS SCRIPT!
# Because individual C files refer to Components with the full hierarcy
# path. So this is not necessary at the moment.

rm $(find Components/ | grep \\.html)

mkdir Components/Src
mkdir Components/Inc
mv $(find Components/ | grep \\.c) -t Components/Src
mv$(find Components/ | grep \\.h) -t Components/Inc
