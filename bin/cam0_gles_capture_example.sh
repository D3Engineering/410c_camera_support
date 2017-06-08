#!/bin/bash
DIRNAME="$(readlink -f "$(dirname "${BASH_SOURCE[0]}")")"
cd "${DIRNAME}"
./cam_reset_links.sh
./cam0_nv12_1080_setup.sh
./capture -d /dev/video3 -s /dev/v4l-subdev10
