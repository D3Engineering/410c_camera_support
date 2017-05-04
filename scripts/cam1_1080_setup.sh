#!/bin/sh
set +x
# Connect CSI1 to ISP1 output
sudo media-ctl -v -d /dev/media0 -l '"msm_csiphy1":1->"msm_csid1":0[1],"msm_csid1":1->"msm_ispif1":0[1],"msm_ispif1":1->"msm_vfe0_rdi1":0[1]'
# Set resolution to 1920x1080
sudo media-ctl -v -d /dev/media0 -V '"ov5640 1-0074":0[fmt:UYVY2X8/1920x1080],"msm_csiphy1":0[fmt:UYVY2X8/1920x1080],"msm_csid1":0[fmt:UYVY2X8/1920x1080],"msm_ispif1":0[fmt:UYVY2X8/1920x1080],"msm_vfe0_rdi1":0[fmt:UYVY2X8/1920x1080]'
