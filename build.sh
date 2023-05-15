#!/bin/sh -eu
cc -g -o cap stream.c WallClockIntervalometerSource.c `pkg-config --cflags --libs aravis-0.8 gstreamer-1.0 gstreamer-app-1.0` -fsanitize=address
