#!/usr/bin/env bash
for i in `grep url ../.gitmodules | awk  -F= '{print $2}'`; do git clone $i;done
