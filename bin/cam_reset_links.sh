#!/bin/bash
set +x
# Connect reset camera links.
# reset request is not releasing link attachments.
sudo media-ctl -d /dev/media1 -l '"msm_csiphy0":1->"msm_csid0":0[0],"msm_csid0":1->"msm_ispif0":0[0],"msm_ispif0":1->"msm_vfe0_rdi0":0[0],"msm_ispif0":1->"msm_vfe0_pix":0[0]'
sudo media-ctl -d /dev/media1 -l '"msm_csiphy1":1->"msm_csid1":0[0],"msm_csid1":1->"msm_ispif1":0[0],"msm_ispif1":1->"msm_vfe0_rdi1":0[0],"msm_ispif1":1->"msm_vfe0_pix":0[0]'
