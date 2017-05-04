#!/bin/sh
/usr/local/share/d3/cam0_1080_setup.sh
gst-launch-1.0 -v -e v4l2src ! glimagesink &
