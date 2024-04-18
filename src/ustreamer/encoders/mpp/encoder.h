/*****************************************************************************
#                                                                            #
#    uStreamer - Lightweight and fast MJPEG-HTTP streamer.                   #
#                                                                            #
#    This source file based on code of MJPG-Streamer.                        #
#                                                                            #
#    Copyright (C) 2005-2006  Laurent Pinchart & Michel Xhaard               #
#    Copyright (C) 2006  Gabriel A. Devenyi                                  #
#    Copyright (C) 2007  Tom Stöveken                                        #
#    Copyright (C) 2018-2022  Maxim Devaev <mdevaev@gmail.com>               #
#                                                                            #
#    This program is free software: you can redistribute it and/or modify    #
#    it under the terms of the GNU General Public License as published by    #
#    the Free Software Foundation, either version 3 of the License, or       #
#    (at your option) any later version.                                     #
#                                                                            #
#    This program is distributed in the hope that it will be useful,         #
#    but WITHOUT ANY WARRANTY; without even the implied warranty of          #
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           #
#    GNU General Public License for more details.                            #
#                                                                            #
#    You should have received a copy of the GNU General Public License       #
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.  #
#                                                                            #
*****************************************************************************/
#ifndef __ENCODER_H__
#define __ENCODER_H__

#if defined(_WIN32)
#include "vld.h"
#endif

#include <stdio.h>
#include <string.h>

#include "rockchip/rk_mpi.h"
#include "rockchip/rk_type.h"
#include "rockchip/rk_venc_ref.h"
#include "rockchip/mpp_meta.h"
#include "rockchip/rk_venc_cmd.h"
#include "rockchip/mpp_packet.h"
#include "rockchip/mpp_frame.h"

#include "mpp_mem.h"
#include "mpp_common.h"
#include "mpp_env.h"
#include "mpi_enc_utils.h"

#include "../../../libs/logging.h"
#include "../../../libs/frame.h"

typedef void *MppEncRefCfg;

typedef struct MpiEncData {
    // base flow context
    MppCtx ctx;
    MppApi *mpi;
    MppEncCfg cfg;
    MppEncPrepCfg prep_cfg;
    MppEncRcCfg rc_cfg;
    MppEncCodecCfg codec_cfg;
    MppEncSliceSplit split_cfg;
    MppEncOSDPltCfg osd_plt_cfg;
    MppEncOSDPlt    osd_plt;
    MppEncOSDData   osd_data;
    MppEncROIRegion roi_region[3];
    MppEncROICfg    roi_cfg;


    // global flow control flag
    RK_U32 frm_eos;
    RK_U32 pkt_eos;
    RK_S32 frame_count;
    RK_U32 frm_pkt_cnt;
    RK_U64 stream_size;

    // src and dst
    FILE *fp_input;
    FILE *fp_output;

    /* encoder config set */
    // MppEncCfg cfg;
    // MppEncPrepCfg prep_cfg;
    // MppEncRcCfg rc_cfg;
    // MppEncCodecCfg codec_cfg;
    // MppEncSliceSplit split_cfg;
    // MppEncOSDPltCfg osd_plt_cfg;
    // MppEncOSDPlt osd_plt;
    // MppEncOSDData osd_data;
    // RoiRegionCfg roi_region;
    // MppEncROICfg roi_cfg;

    // input / output
    MppBufferGroup buf_grp;
    MppBuffer frm_buf;
    MppBuffer pkt_buf;
    MppEncSeiMode sei_mode;
    MppEncHeaderMode header_mode;

    // paramter for resource malloc
    RK_U32 width;
    RK_U32 height;
    RK_U32 hor_stride;
    RK_U32 ver_stride;
    MppFrameFormat fmt;
    MppCodingType type;
    RK_S32 num_frames;
    RK_S32 loop_times;

    // CamSource *cam_ctx;
    // MppEncRoiCtx roi_ctx;

    // resources
    size_t header_size;
    size_t frame_size;
    size_t mdinfo_size;
    /* NOTE: packet buffer may overflow */
    size_t packet_size;

    RK_U32 osd_enable;
    RK_U32 osd_mode;
    RK_U32 split_mode;
    RK_U32 split_arg;
    // RK_U32 split_out;

    RK_U32 user_data_enable;
    RK_U32 roi_enable;

    // rate control runtime parameter
    RK_S32 fps_in_flex;
    RK_S32 fps_in_den;
    RK_S32 fps_in_num;
    RK_S32 fps_out_flex;
    RK_S32 fps_out_den;
    RK_S32 fps_out_num;
    RK_S32 bps;
    RK_S32 bps_max;
    RK_S32 bps_min;
    RK_S32 rc_mode;
    RK_S32 gop_mode;
    RK_S32 gop_len;
    RK_S32 vi_len;

    // RK_S64 first_frm;
    // RK_S64 first_pkt;
} mpp_encode_data;


typedef struct MpiEncArgs {
    char                *file_input;
    char                *file_output;
    char                *file_cfg;
    // dictionary          *cfg_ini;

    MppCodingType       type;
    MppFrameFormat      format;
    RK_S32              num_frames;
    RK_S32              loop_cnt;

    RK_S32              width;
    RK_S32              height;
    RK_S32              hor_stride;
    RK_S32              ver_stride;

    RK_S32              bps_target;
    RK_S32              bps_max;
    RK_S32              bps_min;
    RK_S32              rc_mode;

    RK_S32              fps_in_flex;
    RK_S32              fps_in_num;
    RK_S32              fps_in_den;
    RK_S32              fps_out_flex;
    RK_S32              fps_out_num;
    RK_S32              fps_out_den;

    RK_S32              gop_mode;
    RK_S32              gop_len;
    RK_S32              vi_len;

    MppEncHeaderMode    header_mode;

    MppEncSliceSplit    split;
} mpp_encode_cfg;


typedef struct MppEncoder {
	unsigned    output_format;
    int         last_online;
    unsigned    height;
    unsigned    width;
    unsigned	gop;

    mpp_encode_cfg *cfg; // pointer to global command line info
    mpp_encode_data *p; // context of encoder
} us_mpp_encoder_s;

us_mpp_encoder_s *us_mpp_h264_encoder_init(unsigned width, unsigned height, MppFrameFormat input_format, unsigned output_format, unsigned gop);
int us_mpp_h264_encoder_compress(us_mpp_encoder_s *enc, const us_frame_s *src, us_frame_s *dest, bool force_key);
us_mpp_encoder_s * us_mpp_jpeg_encoder_init(unsigned width, unsigned height, MppFrameFormat input_format, unsigned gop, unsigned quality);
int us_mpp_jpeg_encoder_compress(us_mpp_encoder_s *enc, const us_frame_s *src, us_frame_s *dest);
void us_mpp_encoder_destory(us_mpp_encoder_s *enc);
#endif