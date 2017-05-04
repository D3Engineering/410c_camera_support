#!/bin/sh
set +x
# Connect CSI0 to ISP0 output
sudo media-ctl -v -d /dev/media0 -l '"msm_csiphy0":1->"msm_csid0":0[1],"msm_csid0":1->"msm_ispif0":0[1],"msm_ispif0":1->"msm_vfe0_rdi0":0[1]'
# Set resolution to 1920x1080
sudo media-ctl -v -d /dev/media0 -V '"ov5640 1-0078":0[fmt:UYVY2X8/1920x1080],"msm_csiphy0":0[fmt:UYVY2X8/1920x1080],"msm_csid0":0[fmt:UYVY2X8/1920x1080],"msm_ispif0":0[fmt:UYVY2X8/1920x1080],"msm_vfe0_rdi0":0[fmt:UYVY2X8/1920x1080]'
