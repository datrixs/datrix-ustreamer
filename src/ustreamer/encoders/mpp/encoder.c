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

#include "encoder.h"

#define _RUN(x_next) enc->x_next

static MppCodingType enc_parse_type(unsigned format) {
    switch (format) {
        case V4L2_PIX_FMT_MJPEG:
            /**< Motion JPEG */
            return MPP_VIDEO_CodingMJPEG;
        case V4L2_PIX_FMT_H264:
            /**< H.264/AVC */
            return MPP_VIDEO_CodingAVC;
        default:
            /**< Value when coding is N/A */
            return MPP_VIDEO_CodingUnused;
    }
}

static MPP_RET enc_ctx_init(mpp_encode_data *p, mpp_encode_cfg *cfg) {
    if (!p || !cfg) {
        US_LOG_ERROR("Invalid input data %p ------ cfg %p", p, cfg);
        return MPP_ERR_NULL_PTR;
    }
    MPP_RET ret = MPP_OK;

    // get paramter from cfg
    p->width      = cfg->width;
    p->height     = cfg->height;
    p->hor_stride = (cfg->hor_stride) ? (cfg->hor_stride) : (MPP_ALIGN(cfg->width, 16));
    p->ver_stride = (cfg->ver_stride) ? (cfg->ver_stride) : (MPP_ALIGN(cfg->height, 16));
    p->fmt        = cfg->format;
    p->type       = cfg->type;
    p->bps        = cfg->bps_target;
    p->bps_min    = cfg->bps_min;
    p->bps_max    = cfg->bps_max;
    p->rc_mode    = cfg->rc_mode;
    p->num_frames = cfg->num_frames;
    if (cfg->type == MPP_VIDEO_CodingMJPEG && p->num_frames == 0) {
        US_LOG_INFO("jpege default encode only one frame. Use -n [num] for rc case");
        p->num_frames = 1;
    }
    
    p->gop_mode       = cfg->gop_mode;
    p->gop_len        = cfg->gop_len;
    p->vi_len         = cfg->vi_len;

    p->fps_in_flex    = cfg->fps_in_flex;
    p->fps_in_den     = cfg->fps_in_den;
    p->fps_in_num     = cfg->fps_in_num;
    p->fps_out_flex   = cfg->fps_out_flex;
    p->fps_out_den    = cfg->fps_out_den;
    p->fps_out_num    = cfg->fps_out_num;
    p->mdinfo_size    = (MPP_VIDEO_CodingHEVC == cfg->type) ? 
                        (MPP_ALIGN(p->hor_stride, 32) >> 5) * 
                        (MPP_ALIGN(p->ver_stride, 32) >> 5) * 16 : 
                        (MPP_ALIGN(p->hor_stride, 64) >> 6) * 
                        (MPP_ALIGN(p->ver_stride, 16) >> 4) * 16;

    if (cfg->file_output) {
        p->fp_output = fopen("/home/wucw/out.264", "w+b");
        if (NULL == p->fp_output) {
            US_LOG_PERROR("failed to open output file %s", cfg->file_output);
            ret = MPP_ERR_OPEN_FILE;
        }
    }
    
    // update resource parameter
    switch (p->fmt & MPP_FRAME_FMT_MASK) {
        case MPP_FMT_YUV420SP:
        case MPP_FMT_YUV420P: {
            p->frame_size = MPP_ALIGN(p->hor_stride, 64) * MPP_ALIGN(p->ver_stride, 64) * 3 / 2;
        } break;

        case MPP_FMT_YUV422_YUYV:
        case MPP_FMT_YUV422_YVYU:
        case MPP_FMT_YUV422_UYVY:
        case MPP_FMT_YUV422_VYUY:
        case MPP_FMT_YUV422P:
        case MPP_FMT_YUV422SP: {
            p->frame_size = MPP_ALIGN(p->hor_stride, 64) * MPP_ALIGN(p->ver_stride, 64) * 2;
        } break;

        case MPP_FMT_RGB444:
        case MPP_FMT_BGR444:
        case MPP_FMT_RGB555:
        case MPP_FMT_BGR555:
        case MPP_FMT_RGB565:
        case MPP_FMT_BGR565:
        case MPP_FMT_RGB888:
        case MPP_FMT_BGR888:
        case MPP_FMT_RGB101010:
        case MPP_FMT_BGR101010:
        case MPP_FMT_ARGB8888:
        case MPP_FMT_ABGR8888:
        case MPP_FMT_BGRA8888:
        case MPP_FMT_RGBA8888: {
            p->frame_size = MPP_ALIGN(p->hor_stride, 64) * MPP_ALIGN(p->ver_stride, 64);
        } break;

        default: {
            p->frame_size = MPP_ALIGN(p->hor_stride, 64) * MPP_ALIGN(p->ver_stride, 64) * 4;
        } break;
    }

    if (MPP_FRAME_FMT_IS_FBC(p->fmt)) {
        if ((p->fmt & MPP_FRAME_FBC_MASK) == MPP_FRAME_FBC_AFBC_V1){
            p->header_size = MPP_ALIGN(MPP_ALIGN(p->width, 16) * MPP_ALIGN(p->height, 16) / 16, SZ_4K);
        } else {
            p->header_size = MPP_ALIGN(p->width, 16) * MPP_ALIGN(p->height, 16) / 16;
        }
    } else {
        p->header_size = 0;
    }

    return ret;
}

static MPP_RET mpp_enc_cfg_setup(mpp_encode_data *p) {
    MppApi *mpi = p->mpi;
    MppCtx ctx = p->ctx;
    MppEncCfg cfg = p->cfg;
    MPP_RET ret;
    MppEncRcMode rc_mode = MPP_ENC_RC_MODE_CBR;
    RK_U32 rotation;
    RK_U32 mirroring;
    RK_U32 flip;

    /* setup default parameter */
	if (p->fps_in_den == 0) p->fps_in_den = 1;
	if (p->fps_in_num == 0) p->fps_in_num = 30;
	if (p->fps_out_den == 0) p->fps_out_den = 1;
	if (p->fps_out_num == 0) p->fps_out_num = 30;
    if (!p->bps) {
        p->bps = p->width * p->height / 8 * (p->fps_out_num / p->fps_out_den);
    }

    mpp_enc_cfg_set_s32(cfg, "prep:width", p->width);
    mpp_enc_cfg_set_s32(cfg, "prep:height", p->height);
    mpp_enc_cfg_set_s32(cfg, "prep:hor_stride", p->hor_stride);
    mpp_enc_cfg_set_s32(cfg, "prep:ver_stride", p->ver_stride);
    mpp_enc_cfg_set_s32(cfg, "prep:format", p->fmt);

    /* setup rc */
    mpp_enc_cfg_set_s32(cfg, "rc:mode", rc_mode);
    mpp_enc_cfg_set_u32(cfg, "rc:max_reenc_times", 1);

    /* fix input / output frame rate */
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_flex", p->fps_in_flex);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_num", p->fps_in_num);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_denorm", p->fps_in_den);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_flex", p->fps_out_flex);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_num", p->fps_out_num);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_denorm", p->fps_out_den);
    // mpp_enc_cfg_set_s32(cfg, "rc:gop", 45);
    mpp_enc_cfg_set_s32(cfg, "rc:gop", p->gop_len);

    // if (p->gop_len) {
    //     mpp_enc_cfg_set_s32(cfg, "rc:gop", p->gop_len);
    // } else {
    //     mpp_enc_cfg_set_s32(cfg,"rc:gop", p->fps_out_num * 2);
    // }

    /* drop frame or not when bitrate overflow */
    mpp_enc_cfg_set_u32(cfg, "rc:drop_mode", MPP_ENC_RC_DROP_FRM_DISABLED);
    mpp_enc_cfg_set_u32(cfg, "rc:drop_thd", 20);        /* 20% of max bps */
    mpp_enc_cfg_set_u32(cfg, "rc:drop_gap", 1);         /* Do not continuous drop frame */

    /* setup bitrate for different rc_mode */
    mpp_enc_cfg_set_s32(cfg, "rc:bps_target", p->bps);
    switch (p->rc_mode) {
        case MPP_ENC_RC_MODE_FIXQP : {
            /* do not setup bitrate on FIXQP mode */
        } break;

        case MPP_ENC_RC_MODE_CBR : {
            /* CBR mode has narrow bound */
            mpp_enc_cfg_set_s32(cfg, "rc:bps_max", p->bps_max ? p->bps_max : p->bps * 17 / 16);
            mpp_enc_cfg_set_s32(cfg, "rc:bps_min", p->bps_min ? p->bps_min : p->bps * 15 / 16);
        } break;

        case MPP_ENC_RC_MODE_VBR :
        case MPP_ENC_RC_MODE_AVBR : {
            /* VBR mode has wide bound */
            mpp_enc_cfg_set_s32(cfg, "rc:bps_max", p->bps_max ? p->bps_max : p->bps * 17 / 16);
            mpp_enc_cfg_set_s32(cfg, "rc:bps_min", p->bps_min ? p->bps_min : p->bps * 1 / 16);
        } break;

        default : {
            /* default use CBR mode */
            mpp_enc_cfg_set_s32(cfg, "rc:bps_max", p->bps_max ? p->bps_max : p->bps * 17 / 16);
            mpp_enc_cfg_set_s32(cfg, "rc:bps_min", p->bps_min ? p->bps_min : p->bps * 15 / 16);
        } break;
    }

    /* setup qp for different codec and rc_mode */
    switch (p->type) {
        case MPP_VIDEO_CodingAVC :
        case MPP_VIDEO_CodingHEVC : {
            switch (p->rc_mode) {
                case MPP_ENC_RC_MODE_FIXQP : {
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_init", 20);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_max", 20);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_min", 20);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", 20);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", 20);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 2);
                } break;

                case MPP_ENC_RC_MODE_CBR :
                case MPP_ENC_RC_MODE_VBR :
                case MPP_ENC_RC_MODE_AVBR : {
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_init", 26);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_max", 51);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_min", 10);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", 51);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", 10);
                    mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 2);
                } break;

                default : {
                    US_LOG_ERROR("unsupport encoder rc mode %d", p->rc_mode);
                } break;
            }
        } break;

        case MPP_VIDEO_CodingVP8 : {
            /* vp8 only setup base qp range */
            mpp_enc_cfg_set_s32(cfg, "rc:qp_init", 40);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_max",  127);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_min",  0);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", 127);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", 0);
            mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 6);
        } break;

        case MPP_VIDEO_CodingMJPEG : {
            /* jpeg use special codec config to control qtable */
            // mpp_enc_cfg_set_s32(cfg, "jpeg:q_factor", 80);
            // mpp_enc_cfg_set_s32(cfg, "jpeg:qf_max", 99);
            // mpp_enc_cfg_set_s32(cfg, "jpeg:qf_min", 1);
            mpp_enc_cfg_set_s32(cfg, "jpeg:quant", 8);
        } break;

        default : {
        } break;
    }

    /* setup codec  */
    mpp_enc_cfg_set_s32(cfg, "codec:type", p->type);
    switch (p->type) {
        case MPP_VIDEO_CodingAVC: {
            RK_U32 constraint_set;

            /*
            * H.264 profile_idc parameter
            * 66  - Baseline profile
            * 77  - Main profile
            * 100 - High profile
            */
            mpp_enc_cfg_set_s32(cfg, "h264:profile", 100);
            /*
            * H.264 level_idc parameter
            * 10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
            * 20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
            * 30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
            * 40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
            * 50 / 51 / 52         - 4K@30fps
            */
            mpp_enc_cfg_set_s32(cfg, "h264:level", 40);
            mpp_enc_cfg_set_s32(cfg, "h264:cabac_en", 1);
            mpp_enc_cfg_set_s32(cfg, "h264:cabac_idc", 0);
            mpp_enc_cfg_set_s32(cfg, "h264:trans8x8", 1);

            mpp_env_get_u32("constraint_set", &constraint_set, 0);
            if (constraint_set & 0x3f0000) {
                mpp_enc_cfg_set_s32(cfg, "h264:constraint_set", constraint_set);
            }
        } break;

        case MPP_VIDEO_CodingHEVC:
        case MPP_VIDEO_CodingMJPEG:
        case MPP_VIDEO_CodingVP8: {
        } break;

        default: {
            US_LOG_ERROR("unsupport encoder coding type %d", p->type);
        } break;
    }

    p->split_mode = 0;
    p->split_arg = 0;

    mpp_env_get_u32("split_mode", &p->split_mode, MPP_ENC_SPLIT_NONE);
    mpp_env_get_u32("split_arg", &p->split_arg, 0);

    if (p->split_mode) {
        US_LOG_INFO("%p split mode %d arg %d", ctx, p->split_mode, p->split_arg);
        mpp_enc_cfg_set_s32(cfg, "split:mode", p->split_mode);
        mpp_enc_cfg_set_s32(cfg, "split:arg", p->split_arg);
    }

    mpp_env_get_u32("mirroring", &mirroring, 0);
    mpp_env_get_u32("rotation", &rotation, 0);
    mpp_env_get_u32("flip", &flip, 0);

    mpp_enc_cfg_set_s32(cfg, "prep:mirroring", mirroring);
    mpp_enc_cfg_set_s32(cfg, "prep:rotation", rotation);
    // mpp_enc_cfg_set_s32(cfg, "prep:flip", flip);

    // create mpp
    ret = mpi->control(ctx, MPP_ENC_SET_CFG, cfg);
    if (ret) {
        US_LOG_PERROR("mpi control enc set cfg failed ret %d", ret);
        return ret;
    }

    if (p->type == MPP_VIDEO_CodingAVC || p->type == MPP_VIDEO_CodingHEVC) {
        /* optional set mpp mode*/
        {
            RK_U32 sei_mode;

            mpp_env_get_u32("sei_mode", &sei_mode, MPP_ENC_SEI_MODE_ONE_FRAME);
            p->sei_mode = sei_mode;
            ret = mpi->control(ctx, MPP_ENC_SET_SEI_CFG, &p->sei_mode);
            if (ret) {
                US_LOG_PERROR("mpi control enc set sei cfg failed ret %d", ret);
                return ret;
            }
        }

        p->header_mode = MPP_ENC_HEADER_MODE_EACH_IDR;
        ret = mpi->control(ctx, MPP_ENC_SET_HEADER_MODE, &p->header_mode);
        if (ret) {
            US_LOG_PERROR("mpi control enc set header mode failed ret %d", ret);
            return ret;
        }
    }

    if (p->type == MPP_VIDEO_CodingMJPEG) {
        p->header_mode = MPP_ENC_HEADER_MODE_DEFAULT;
        ret = mpi->control(ctx, MPP_ENC_SET_HEADER_MODE, &p->header_mode);
        if (ret) {
            US_LOG_PERROR("mpi control enc set header mode failed ret %d", ret);
            return ret;
        }
    }

    RK_U32 gop_mode = p->gop_mode;

    mpp_env_get_u32("gop_mode", &gop_mode, gop_mode);
    if (gop_mode) {
        MppEncRefCfg ref;

        mpp_enc_ref_cfg_init(&ref);

        if (p->gop_mode < 4) {
            mpi_enc_gen_ref_cfg(ref, gop_mode);
        } 
        else {
            mpi_enc_gen_smart_gop_ref_cfg(ref, p->gop_len, p->vi_len);
        }

        ret = mpi->control(ctx, MPP_ENC_SET_REF_CFG, ref);
        if (ret) {
            US_LOG_PERROR("mpi control enc set ref cfg failed ret %d", ret);
            return ret;
        }
        mpp_enc_ref_cfg_deinit(&ref);
    }

    /* setup test mode by env */
    mpp_env_get_u32("osd_enable", &p->osd_enable, 0);
    mpp_env_get_u32("osd_mode", &p->osd_mode, MPP_ENC_OSD_PLT_TYPE_DEFAULT);
    mpp_env_get_u32("roi_enable", &p->roi_enable, 0);
    mpp_env_get_u32("user_data_enable", &p->user_data_enable, 0);

    return ret;
}

us_mpp_encoder_s *us_mpp_h264_encoder_init(unsigned width, unsigned height,  MppFrameFormat input_format, unsigned output_format, unsigned gop)
// us_mpp_encoder_s *us_mpp_encoder_init(unsigned height, unsigned width, int fps) 
{
    // if (output_format != V4L2_PIX_FMT_MJPEG || output_format != V4L2_PIX_FMT_H264) {
    //     US_LOG_ERROR("The encoder not support the output format, %d", output_format);
    //     return NULL;
    // }
    
    RK_S32 ret = MPP_NOK;
    MppPollType timeout = MPP_POLL_BLOCK;

    us_mpp_encoder_s *enc = NULL;
    US_CALLOC(enc, 1);
    if (NULL == enc) {
        US_LOG_PERROR("Failed to alloc encode for instance");
        return NULL;
    }
    enc->width = width;
    enc->height = height;
    enc->output_format = (output_format == V4L2_PIX_FMT_MJPEG ? V4L2_PIX_FMT_JPEG : V4L2_PIX_FMT_H264);

    mpp_encode_cfg *cfg = NULL;
    US_CALLOC(cfg, 1);
    if (NULL == cfg) {
        if (NULL != enc) {
            free(enc);
            enc = NULL;
        }
        US_LOG_PERROR("Failed to calloc mpp_encode_cfg for instance");
        return NULL;
    }

    mpp_encode_data *p = NULL;
    US_CALLOC(p, 1);
    if (NULL == p) {
        if (NULL != cfg) {
            free(cfg);
            cfg = NULL;
        }
        if (NULL != enc) {
            free(enc);
            enc = NULL;
        }
        US_LOG_PERROR("Failed to calloc mpp_encode_data for instance");
        return NULL;
    }

    // 设置的输入帧的格式信息
    cfg->format = input_format;
    // cfg->format = format;
    // 设置264编码格式
    cfg->type = enc_parse_type(output_format);
    cfg->width = width;
    cfg->height = height;
    cfg->gop_len = gop;
    cfg->fps_in_num = gop;
    cfg->fps_out_num = gop;
    cfg->hor_stride = mpi_enc_width_default_stride(cfg->width, cfg->format);
	cfg->ver_stride = cfg->height;

    ret = enc_ctx_init(p, cfg);
    if (ret) {
        US_LOG_PERROR("test data init failed ret %d", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    ret = mpp_buffer_group_get_internal(&p->buf_grp, MPP_BUFFER_TYPE_DRM);
    if (ret) {
        US_LOG_PERROR("failed to get mpp buffer group ret %d", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    ret = mpp_buffer_get(p->buf_grp, &p->frm_buf, p->frame_size + p->header_size);
    if (ret) {
        US_LOG_PERROR("failed to get buffer for input frame ret %d", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    ret = mpp_buffer_get(p->buf_grp, &p->pkt_buf, p->frame_size);
    if (ret) {
        US_LOG_PERROR("failed to get buffer for output packet ret %d", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    US_LOG_INFO("%p encoder start w %d h %d type %d", p->ctx, p->width, p->height, p->type);
    // create mpp context and mpp api
    ret = mpp_create(&p->ctx, &p->mpi);
    if (ret) {
        US_LOG_PERROR("mpp_create failed ret %d", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    ret = p->mpi->control(p->ctx, MPP_SET_OUTPUT_TIMEOUT, &timeout);
    if (MPP_OK != ret) {
        US_LOG_PERROR("mpi control set output timeout %d ret %d", timeout, ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    // init mpp
    ret = mpp_init(p->ctx, MPP_CTX_ENC, p->type);
    if (ret) {
        US_LOG_PERROR("mpp_init failed ret %d", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    ret = mpp_enc_cfg_init(&p->cfg);
    if (ret) {
        US_LOG_PERROR("mpp_enc_cfg_init failed ret %dn", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    ret = p->mpi->control(p->ctx, MPP_ENC_GET_CFG, p->cfg);
    if (ret) {
        US_LOG_PERROR("get enc cfg failed ret %d", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    ret = mpp_enc_cfg_setup(p);
    if (ret) {
        US_LOG_PERROR("test mpp setup failed ret %d", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }
    enc->gop = 30;
    enc->cfg = cfg;
    enc->p = p;
    return enc;
}

int us_mpp_h264_encoder_compress(us_mpp_encoder_s *enc, const us_frame_s *src, us_frame_s *dest, bool force_key) {
    if (enc == NULL && src == NULL) {
        return -1;
    }

    us_frame_encoding_begin(src, dest, (enc->output_format == V4L2_PIX_FMT_MJPEG ? V4L2_PIX_FMT_JPEG : enc->output_format));
    force_key = (enc->output_format == V4L2_PIX_FMT_H264 && (force_key || _RUN(last_online) != src->online));
    US_LOG_DEBUG("Compressing new frame; force_key=%d ...", force_key);

    mpp_encode_data *p = enc->p;
    MppApi *mpi = p->mpi;
    MppCtx ctx = p->ctx;
    MPP_RET ret = MPP_OK;

    MppMeta meta = NULL;
    MppFrame frame = NULL;
    MppPacket packet = NULL;
    RK_U32 eoi = 1;
    void *buf = mpp_buffer_get_ptr(p->frm_buf);
    memcpy(buf, src->data, src->used);
    ret = mpp_frame_init(&frame);
    if (ret) {
        US_LOG_PERROR("MPP Frame init failed");
        return ret;
    }

    mpp_frame_set_width(frame, p->width);
    mpp_frame_set_height(frame, p->height);
    mpp_frame_set_hor_stride(frame, p->hor_stride);
    mpp_frame_set_ver_stride(frame, p->ver_stride);
    mpp_frame_set_fmt(frame, p->fmt);
    mpp_frame_set_eos(frame, p->frm_eos);

    mpp_frame_set_buffer(frame, p->frm_buf);
    meta = mpp_frame_get_meta(frame);
    mpp_packet_init_with_buffer(&packet, p->pkt_buf);

    /* NOTE: It is important to clear output packet lenght!! */
    mpp_packet_set_length(packet, 0);
    mpp_meta_set_packet(meta, KEY_OUTPUT_PACKET, packet);

    ret = mpi->encode_put_frame(ctx, frame);
    if (ret) {
        US_LOG_PERROR("encode put frame failed!, %d", ret);
        mpp_frame_deinit(&frame);
        return ret;
    }

    mpp_frame_deinit(&frame);

    do {
        ret = mpi->encode_get_packet(ctx, &packet);
        if (ret) {
            US_LOG_PERROR("Encode get packet failed!");
            return ret;
        }

        assert(packet);

        if (packet) {
            // write packet to sink here
            void *packet_data_ptr = mpp_packet_get_pos(packet);
            size_t byteused = mpp_packet_get_length(packet);
            p->pkt_eos = mpp_packet_get_eos(packet);

            memcpy(dest->data, packet_data_ptr, byteused);
            dest->used = byteused;
            dest->gop = enc->gop;

            /* for low delay partition encoding */
            if (mpp_packet_is_partition(packet)) {
                eoi = mpp_packet_is_eoi(packet);
                p->frm_pkt_cnt = (eoi) ? (0) : (p->frm_pkt_cnt + 1);
            }

            if (p->fp_output) {
                fwrite(packet_data_ptr, 1, byteused, p->fp_output);
            }

            if (mpp_packet_has_meta(packet)) {
                meta = mpp_packet_get_meta(packet);
                RK_S32 temporal_id = 0;
                RK_S32 lt_idx = -1;
                RK_S32 avg_qp = -1;
                if (MPP_OK == mpp_meta_get_s32(meta, KEY_TEMPORAL_ID, &temporal_id)) {
                    US_LOG_DEBUG("mpp get meta tid %d", temporal_id);
                }
                if (MPP_OK == mpp_meta_get_s32(meta, KEY_LONG_REF_IDX, &lt_idx)) {
                    US_LOG_DEBUG("mpp get meta lt %d", lt_idx);
                }
                if (MPP_OK == mpp_meta_get_s32(meta, KEY_ENC_AVERAGE_QP, &avg_qp)) {
                    US_LOG_DEBUG("mpp get meta qp %d", avg_qp);
                }
            }
            
            mpp_packet_deinit(&packet);
            p->stream_size += byteused;
            p->frame_count += eoi;
            if (p->pkt_eos) {
                US_LOG_INFO("%p found last packet", ctx);
            }
        }
    } while (!eoi);

    us_frame_encoding_end(dest);

    _RUN(last_online) = src->online;

    return ret;
}

us_mpp_encoder_s *us_mpp_jpeg_encoder_init(unsigned width, unsigned height, MppFrameFormat input_format, unsigned gop, unsigned quality) {
    RK_S32 ret = MPP_NOK;
	us_mpp_encoder_s *enc = NULL;
    US_CALLOC(enc, 1);
    if (NULL == enc) {
        US_LOG_PERROR("us_mpp_jpeg_encoder_open Failed to alloc encode for instance");
        return NULL;
    }
    enc->width = width;
    enc->height = height;
    enc->output_format = V4L2_PIX_FMT_JPEG ;

    mpp_encode_data *p = NULL;
    US_CALLOC(p, 1);
    if (NULL == p) {
        if (NULL != enc) {
            free(enc);
            enc = NULL;
        }
        US_LOG_PERROR("us_mpp_jpeg_encoder_open Failed to calloc mpp_encode_data for instance");
        return NULL;
    }

    p->hor_stride = MPP_ALIGN(mpi_enc_width_default_stride(width, input_format),16);
    p->ver_stride = MPP_ALIGN(height, 16);
    p->fps_in_num =30;
    p->fps_in_den = 1;
	p->fps_out_den = 1;
	p->fps_out_num = 30;
    p->bps = width * height / 8 *  p->fps_in_num;
    p->frame_size = MPP_ALIGN(p->hor_stride, 64) * MPP_ALIGN(p->ver_stride, 64) * 2;
    p->header_size = 0;

    ret = mpp_buffer_group_get_internal(&p->buf_grp, MPP_BUFFER_TYPE_DRM);
    if (ret) {
        US_LOG_PERROR("us_mpp_jpeg_encoder_open failed to get mpp buffer group ret %d", ret);
       us_mpp_encoder_destory(enc);
        return NULL;
    }

    ret = mpp_buffer_get(p->buf_grp, &p->frm_buf, p->frame_size + p->header_size);
    if (ret) {
        US_LOG_PERROR("us_mpp_jpeg_encoder_open failed to get buffer for input frame ret %d", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    ret = mpp_buffer_get(p->buf_grp, &p->pkt_buf, p->frame_size);
    if (ret) {
        US_LOG_PERROR("us_mpp_jpeg_encoder_open failed to get buffer for output packet ret %d", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }
    

    ret = mpp_create(&p->ctx, &p->mpi);
    if (ret) {
        US_LOG_PERROR("us_mpp_jpeg_encoder_open mpp_create failed ret %d", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    ret = mpp_init(p->ctx, MPP_CTX_ENC, MPP_VIDEO_CodingMJPEG);
    if (ret) {
        US_LOG_PERROR("us_mpp_jpeg_encoder_open mpp_init failed ret %d", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    // set format 
    MppEncCfg cfg;

    ret = mpp_enc_cfg_init (&cfg);
    if (ret) {
        US_LOG_PERROR("us_mpp_jpeg_encoder_open mpp_enc_cfg_init failed ret %dn", ret);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    ret = p->mpi->control(p->ctx, MPP_ENC_GET_CFG, cfg);
    if (ret) {
        US_LOG_PERROR("us_mpp_jpeg_encoder_open Get enc cfg failed ret %dn", ret);
        mpp_enc_cfg_deinit (cfg);
        us_mpp_encoder_destory(enc);
        return NULL;
    }

    mpp_enc_cfg_set_s32 (cfg, "codec:type", MPP_VIDEO_CodingMJPEG);

    int quant = (int)((quality / 10) + 0.5);
    if (quant >= 10) {
        quant = 10;
    } else if(quant <= 0) {
        quant = 0;
    }

    mpp_enc_cfg_set_s32 (cfg, "jpeg:quant", quant);

    mpp_enc_cfg_set_s32 (cfg, "prep:format",input_format);
    mpp_enc_cfg_set_s32 (cfg, "prep:width", width);
    mpp_enc_cfg_set_s32 (cfg, "prep:height",height);
    mpp_enc_cfg_set_s32 (cfg, "prep:hor_stride", p->hor_stride);
    mpp_enc_cfg_set_s32 (cfg, "prep:ver_stride",p->ver_stride);
    mpp_enc_cfg_set_s32 (cfg, "rc:fps_in_flex", 0);
    mpp_enc_cfg_set_s32 (cfg, "rc:fps_in_num",p->fps_in_num);
    mpp_enc_cfg_set_s32 (cfg, "rc:fps_in_denorm",p->fps_in_den);
    mpp_enc_cfg_set_s32 (cfg, "rc:fps_out_num",p->fps_out_num);
    mpp_enc_cfg_set_s32 (cfg, "rc:fps_out_denorm",p->fps_out_den);

    //mpp_enc_cfg_set_s32 (cfg, "prep:rotation", self->rotation);
    mpp_enc_cfg_set_s32 (cfg, "rc:gop", gop);
    mpp_enc_cfg_set_u32 (cfg, "rc:max_reenc_times", 1);
    mpp_enc_cfg_set_s32 (cfg, "rc:mode", MPP_ENC_RC_MODE_CBR);

    mpp_enc_cfg_set_s32 (cfg, "rc:bps_target", p->bps);
    mpp_enc_cfg_set_s32 (cfg, "rc:bps_max",p->bps * 17 / 16);
    mpp_enc_cfg_set_s32 (cfg, "rc:bps_min",p->bps * 15 / 16);


    ret = p->mpi->control (p->ctx, MPP_ENC_SET_CFG, cfg);
    if (ret){
        US_LOG_PERROR ("us_mpp_jpeg_encoder_open Set enc cfg failed ret%dn", ret);
    }

    mpp_enc_cfg_deinit (cfg); 

    enc->gop = gop;
    enc->p = p;
    return enc;

}

int us_mpp_jpeg_encoder_compress(us_mpp_encoder_s *enc, const us_frame_s *src, us_frame_s *dest) {
    if (enc == NULL && src == NULL) {
        return -1;
    }

    //US_LOG_INFO("us_mpp_jpeg_encoder_compress -------> 1");

    us_frame_encoding_begin(src, dest, V4L2_PIX_FMT_JPEG);

    // US_LOG_INFO("us_mpp_jpeg_encoder_compress -------> 1.1");

    mpp_encode_data *p = enc->p;
    MppApi *mpi = p->mpi;
    MppCtx ctx = p->ctx;
    MPP_RET ret = MPP_OK;

    MppMeta meta = NULL;
    MppFrame frame = NULL;
    MppPacket packet = NULL;
    void *buf = mpp_buffer_get_ptr(p->frm_buf);
    //US_LOG_INFO("us_mpp_jpeg_encoder_compress -------> 1.2");
    memcpy(buf, src->data, src->used);
    //US_LOG_INFO("us_mpp_jpeg_encoder_compress -------> 1.3");
    ret = mpp_frame_init(&frame);
    if (ret) {
        US_LOG_PERROR("us_mpp_jpeg_encoder_compress MPP Frame init failed");
        return ret;
    }
    //US_LOG_INFO("us_mpp_jpeg_encoder_compress -------> 2");


    mpp_frame_set_width(frame, p->width);
    mpp_frame_set_height(frame, p->height);
    mpp_frame_set_hor_stride(frame, p->hor_stride);
    mpp_frame_set_ver_stride(frame, p->ver_stride);

    /*ret = mpi->poll(ctx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (ret){
      US_LOG_PERROR("us_mpp_jpeg_encoder_compress mpp input poll failed");
    }*/

    //US_LOG_INFO("us_mpp_jpeg_encoder_compress -------> 3");

    //mpp_frame_set_fmt(frame, p->fmt);
    mpp_frame_set_eos(frame, 0);

    mpp_frame_set_buffer(frame, p->frm_buf);
    meta = mpp_frame_get_meta(frame);
    mpp_packet_init_with_buffer(&packet, p->pkt_buf);

    /* NOTE: It is important to clear output packet lenght!! */
    mpp_packet_set_length(packet, 0);
    mpp_meta_set_packet(meta, KEY_OUTPUT_PACKET, packet);

    ret = mpi->encode_put_frame(ctx, frame);
    if (ret) {
        US_LOG_PERROR("us_mpp_jpeg_encoder_compress encode put frame failed!, %d", ret);
        mpp_frame_deinit(&frame);
        return ret;
    }

    //US_LOG_INFO("us_mpp_jpeg_encoder_compress -------> 4");


    mpp_frame_deinit(&frame);

 
        ret = mpi->encode_get_packet(ctx, &packet);
        if (ret) {
            US_LOG_PERROR("us_mpp_jpeg_encoder_compress Encode get packet failed!");
            return ret;
        }

        //US_LOG_INFO("us_mpp_jpeg_encoder_compress -------> 5");


        assert(packet);

        if (packet) {
            //US_LOG_INFO("us_mpp_jpeg_encoder_compress -------> 6");

            // write packet to sink here
            void *packet_data_ptr = mpp_packet_get_pos(packet);
            size_t byteused = mpp_packet_get_length(packet);
            p->pkt_eos = mpp_packet_get_eos(packet);

            //US_LOG_INFO("us_mpp_jpeg_encoder_compress (%p, %p, %ld)-------> 6.1",&dest->data,&packet_data_ptr,byteused);

            // memcpy(dest->data, packet_data_ptr, byteused);
            us_frame_set_data(dest, packet_data_ptr, byteused);
           // US_LOG_INFO("us_mpp_jpeg_encoder_compress -------> 6.2");
            //dest->used = byteused;
            //dest->gop = enc->gop;
            mpp_packet_deinit(&packet);
            p->stream_size += byteused;
            p->frame_count += 1;
            p->frm_pkt_cnt += 1;
        }

    //US_LOG_INFO("us_mpp_jpeg_encoder_compress -------> 7");

    us_frame_encoding_end(dest);

    _RUN(last_online) = src->online;

   // US_LOG_INFO("us_mpp_jpeg_encoder_compress -------> 8");

    return ret;

}

void us_mpp_encoder_destory(us_mpp_encoder_s *enc) {
    enc->p->mpi->reset(enc->p->ctx);
    if (enc->p->ctx) {
        mpp_destroy(enc->p->ctx);
        enc->p->ctx  = NULL;
    }
    
    if (enc->p->fp_output) {
        fclose(enc->p->fp_output);
        enc->p->fp_output = NULL;
    }

    if (enc->p->cfg) {
        mpp_enc_cfg_deinit(enc->p->cfg);
        enc->p->cfg = NULL;
    }

    if (enc->p->frm_buf) {
        mpp_buffer_put(enc->p->frm_buf);
        enc->p->frm_buf = NULL;
    }
    
    if (enc->cfg) {
        free(enc->cfg);
        enc->cfg = NULL;
    }

    free(enc);
}