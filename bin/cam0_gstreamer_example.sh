#!/bin/bash
DIRNAME="$(readlink -f "$(dirname "${BASH_SOURCE[0]}")")"
cd "${DIRNAME}"
./cam_reset_links.sh
./cam0_uyvy_1080_setup.sh
gst-launch-1.0 -v -e v4l2src ! glimagesink &
