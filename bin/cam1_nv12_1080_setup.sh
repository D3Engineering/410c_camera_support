#!/bin/bash
set +x
# Connect CSI1 to ISP1 output
sudo media-ctl -d /dev/media1 -l '"msm_csiphy1":1->"msm_csid1":0[1],"msm_csid1":1->"msm_ispif1":0[1],"msm_ispif1":1->"msm_vfe0_pix":0[1]'
# Set resolution to 1920x1080
sudo media-ctl -d /dev/media1 -V '"ov5640 1-0074":0[fmt:UYVY8_2X8/1920x1080 field:none],"msm_csiphy1":0[fmt:UYVY8_2X8/1920x1080 field:none],"msm_csid1":0[fmt:UYVY8_2X8/1920x1080 field:none],"msm_ispif1":0[fmt:UYVY8_2X8/1920x1080 field:none],"msm_vfe0_pix":0[fmt:YUYV8_1_5X8/1920x1080 field:none field:none]'
