/*****************************************************************************
#                                                                            #
#    uStreamer - Lightweight and fast MJPEG-HTTP streamer.                   #
#                                                                            #
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

#include "s2drm.h"

#ifndef US_DRM_MEMSINK_MAX_DATA
#define US_DRM_MEMSINK_MAX_DATA 33554432
#endif
#define DRM_MEMSINK_MAX_DATA ((size_t)(US_DRM_MEMSINK_MAX_DATA))
#define DRM_US_DELETE(x_dest) \
    {                         \
        if (x_dest)           \
        {                     \
            free(x_dest);     \
        }                     \
    }

#define CardPath "/dev/dri/card0"
#define YUV_R(_y, _v, _)	(((_y) + (454 * (_v))) >> 8)
#define YUV_G(_y, _u, _v)	(((_y) - (88 * (_u)) - (183 * (_v))) >> 8)
#define YUV_B(_y, _, _u)	(((_y) + (359 * (_u))) >> 8)
#define NORM_COMPONENT(_x)	(((_x) > 255) ? 255 : (((_x) < 0) ? 0 : (_x)))

rga_info_t src, dst;

static void uyvy_rga_copy(us_drm_s *drm, const us_frame_s *frame) {
    src.virAddr = frame->data;
    dst.virAddr = drm->vaddr;
    rga_set_rect(&src.rect, 0, 0, drm->width, drm->height, drm->width, drm->height, RK_FORMAT_UYVY_422);
    rga_set_rect(&dst.rect, 0, 0, drm->width, drm->height, drm->width, drm->height, RK_FORMAT_BGRX_8888);

    c_RkRgaBlit(&src, &dst, NULL);
}

void rgb_table_init(us_drm_s *drm) {
    for (int i = 0; i < 256; i++) {
        drm->cr_r_tab[i] = (454 * (i - 128));
        drm->cb_b_tab[i] = (359 * (i - 128));
        drm->cr_g_tab[i] = (183 * (i - 128));
        drm->cb_g_tab[i] = (88  * (i - 128));
    }
}

void uyvy_rgb_convert(us_drm_s *drm, const us_frame_s *frame) {
    register uint8_t *outptr = drm->vaddr;
    const uint8_t *data = frame->data;
    register int y1, y2, u, v;
    register int r1, g1, b1, r2, g2, b2;

    for (unsigned h = 0; h < frame->height; ++h) {
        for (unsigned x = 0; x < frame->width/2; ++x) {
            u  = *data++;
            y1 = *data++;
            v  = *data++;
            y2 = *data++;
            y1 = y1 << 8;
            y2 = y2 << 8;

            b1 = NORM_COMPONENT((y1 + drm->cr_r_tab[v]) >> 8);
            g1 = NORM_COMPONENT((y1 - drm->cr_g_tab[v] - drm->cb_g_tab[u]) >> 8);
            r1 = NORM_COMPONENT((y1 + drm->cb_b_tab[u]) >> 8);
            b2 = NORM_COMPONENT((y2 + drm->cr_r_tab[v]) >> 8);
            g2 = NORM_COMPONENT((y2 - drm->cr_g_tab[v] - drm->cb_g_tab[u]) >> 8);
            r2 = NORM_COMPONENT((y2 + drm->cb_b_tab[u]) >> 8);

            *outptr++ = r1;
            *outptr++ = g1;
            *outptr++ = b1;
            *outptr++ = 0;
            *outptr++ = r2;
            *outptr++ = g2;
            *outptr++ = b2;
            *outptr++ = 0;
        }
    }
}

int drm_card_open(int *out)
{
    int fd, ret;
    uint64_t has_dumb;
    fd = open(CardPath, O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        ret = -errno;
        US_LOG_PERROR("Cannot open card %s", CardPath);
        return ret;
    }

    if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || !has_dumb)
    {
        US_LOG_PERROR("DRM device %s does not support dumb buffers", CardPath);
        close(fd);

        return -EOPNOTSUPP;
    }

    *out = fd;
    return 0;
}

us_drm_s *us_drm_init(int width, int height)
{
    drmModeConnector *conn;
    drmModeRes *res;
    drmModePlaneRes *plane_res;
    uint32_t conn_id, crtc_id;

    int ret, fd;

    ret = drm_card_open(&fd);
    if (ret)
    {
        errno = -ret;
        US_LOG_PERROR("DRM init failed error %d", errno);
        exit(-1);
    }
    us_drm_s *drm;
    US_CALLOC(drm, 1);
    drm->path = CardPath;
    drm->fd = fd;
    drm->width = width;
    drm->height = height;
    rgb_table_init(drm);

    res = drmModeGetResources(fd);
    drm->res = res;
    crtc_id = res->crtcs[0];
    conn_id = res->connectors[0];

    ret = drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if (ret)
    {
        US_LOG_PERROR("Failed to set client cap");
        DRM_US_DELETE(drm);
        exit(-1);
    }

    plane_res = drmModeGetPlaneResources(fd);
    drm->plane_res = plane_res;

    conn = drmModeGetConnector(fd, conn_id);
    drm->conn = conn;

    ret = drm_create_fd(fd, drm);
    if (ret < 0)
    {
        US_LOG_PERROR("drmCreateFD err");
        DRM_US_DELETE(drm);
        exit(-1);
    }
    /*
    id      encoder status          name            size (mm)       modes   encoders
    150     149     connected       HDMI-A-1        890x500         24      149
    modes:
            name refresh (Hz) hdisp hss hse htot vdisp vss vse vtot)
    1280x720 60 1280 1390 1430 1650 720 725 730 750 74250 flags: phsync, pvsync; type: preferred, driver
    1920x1080 60 1920 2008 2052 2200 1080 1084 1089 1125 148500 flags: phsync, pvsync; type: driver
    1920x1080 60 1920 2008 2052 2200 1080 1084 1089 1125 148352 flags: phsync, pvsync; type: driver
    1920x1080i 60 1920 2008 2052 2200 1080 1084 1094 1125 74250 flags: phsync, pvsync, interlace; type: driver
    1920x1080i 60 1920 2008 2052 2200 1080 1084 1094 1125 74176 flags: phsync, pvsync, interlace; type: driver
    1920x1080 50 1920 2448 2492 2640 1080 1084 1089 1125 148500 flags: phsync, pvsync; type: driver
    1920x1080i 50 1920 2448 2492 2640 1080 1084 1094 1125 74250 flags: phsync, pvsync, interlace; type: driver
    1920x1080 30 1920 2008 2052 2200 1080 1084 1089 1125 74250 flags: phsync, pvsync; type: driver
    1920x1080 30 1920 2008 2052 2200 1080 1084 1089 1125 74176 flags: phsync, pvsync; type: driver
    1920x1080 24 1920 2558 2602 2750 1080 1084 1089 1125 74250 flags: phsync, pvsync; type: driver
    1920x1080 24 1920 2558 2602 2750 1080 1084 1089 1125 74176 flags: phsync, pvsync; type: driver
    1280x1024 60 1280 1328 1440 1688 1024 1025 1028 1066 108000 flags: phsync, pvsync; type: driver
    1280x720 60 1280 1390 1430 1650 720 725 730 750 74176 flags: phsync, pvsync; type: driver
    1280x720 50 1280 1720 1760 1980 720 725 730 750 74250 flags: phsync, pvsync; type: driver
    1024x768 60 1024 1048 1184 1344 768 771 777 806 65000 flags: nhsync, nvsync; type: driver
    800x600 60 800 840 968 1056 600 601 605 628 40000 flags: phsync, pvsync; type: driver
    720x576 50 720 732 796 864 576 581 586 625 27000 flags: nhsync, nvsync; type: driver
    720x576i 50 720 732 795 864 576 580 586 625 13500 flags: nhsync, nvsync, interlace, dblclk; type: driver
    720x480 60 720 736 798 858 480 489 495 525 27027 flags: nhsync, nvsync; type: driver
    720x480 60 720 736 798 858 480 489 495 525 27000 flags: nhsync, nvsync; type: driver
    720x480i 60 720 739 801 858 480 488 494 525 13514 flags: nhsync, nvsync, interlace, dblclk; type: driver
    720x480i 60 720 739 801 858 480 488 494 525 13500 flags: nhsync, nvsync, interlace, dblclk; type: driver
    640x480 60 640 656 752 800 480 490 492 525 25200 flags: nhsync, nvsync; type: driver
    640x480 60 640 656 752 800 480 490 492 525 25175 flags: nhsync, nvsync; type: driver
    */
    // conn->modes[1]  1920x1080 60 1920 2008 2052 2200 1080 1084 1089 1125 148500 flags: phsync, pvsync; type: driver

    for (int i = 0; i < conn->count_modes; i++) {
        if (drm->width == conn->modes[i].hdisplay && drm->height == conn->modes[i].vdisplay) {
            ret = drmModeSetCrtc(fd, crtc_id, drm->fb_id, 0, 0, &conn_id, 1, &conn->modes[i]);
            if (ret < 0)
            {
                US_LOG_PERROR("drmModeSetCrtc err");
                DRM_US_DELETE(drm);
                exit(-1);
            }
            break;
        }
        continue;        
    }
    // ret = drmModeSetCrtc(fd, crtc_id, drm->fb_id, 0, 0, &conn_id, 1, &conn->modes[1]);
    // if (ret < 0)
    // {
    //     US_LOG_PERROR("drmModeSetCrtc err");
    //     DRM_US_DELETE(drm);
    //     exit(-1);
    // }

    ret = drmModeSetPlane(fd, plane_res->planes[3], crtc_id, drm->fb_id, 0,
                          0, 0, drm->width, drm->height, 0 << 16, 0 << 16, (drm->width) << 16, (drm->height) << 16);
    if (ret < 0)
    {
        US_LOG_PERROR("drmModeSetPlane err");
        DRM_US_DELETE(drm);
        exit(-1);
    }

    return drm;
}

int drm_create_fd(int fd, us_drm_s *drm)
{
    struct drm_mode_create_dumb create = {};
    struct drm_mode_map_dumb dmap = {};
    uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
    int ret;

    create.width = drm->width;
    create.height = drm->height;
    create.bpp = 32; // 32 (DRM_FORMAT_XRGB8888), 16 (DRM_FORMAT_VYUY
    drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

    drm->pitch = create.pitch;
    drm->size = create.size;
    drm->handle = create.handle;
    dmap.handle = create.handle;
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &dmap);

    drm->vaddr = mmap(0, create.size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, dmap.offset);

    offsets[0] = 0;
    handles[0] = drm->handle;
    pitches[0] = drm->pitch;

    ret = drmModeAddFB2(fd, drm->width, drm->height,
                        DRM_FORMAT_XRGB8888, handles, pitches, offsets, &drm->fb_id, 0);
    if (ret)
    {
        US_LOG_PERROR("drmModeAddFB2 err %d\n", ret);
        return ret;
    }

    memset(drm->vaddr, 0xff, drm->size);

    return 0;
}

int us_drm_draw(us_drm_s *drm, const us_frame_s *frame)
{
    assert(drm->fd);
    const long double now = us_get_now_monotonic();

    if (frame->used > US_DRM_MEMSINK_MAX_DATA)
    {
        US_LOG_ERROR("DRM device: Can't put frame: is too big (%lu > %u)", frame->used, US_DRM_MEMSINK_MAX_DATA);
        return 0; // -2
    }

    if (us_flock_timedwait_monotonic(drm->fd, 1) == 0)
    {
        US_LOG_DEBUG("DRM: >>>>> Exposing new frame ...");
        uyvy_rga_copy(drm, frame);
        // uyvy_rgb_convert(drm, frame);
        drm->last_id = us_get_now_id();
        drm->last_client_ts = us_get_now_monotonic();
        if (flock(drm->fd, LOCK_UN) < 0)
        {
            US_LOG_PERROR("DRM: Can't unlock memory");
            return 0;
        }
        US_LOG_DEBUG("DRM: Exposed new frame; full exposition time = %.3Lf", us_get_now_monotonic() - now);
    }
    else if (errno == EWOULDBLOCK)
    {
        US_LOG_VERBOSE("DRM: ===== Shared memory is busy now; frame skipped");
        return -1;
    }
    else
    {
        US_LOG_PERROR("DRM: Can't lock memory");
        return -1;
    }
    return 0;
}

void us_drm_destroy(us_drm_s *drm)
{
    assert(drm->fd);

    struct drm_mode_destroy_dumb destroy = {};

    drmModeRmFB(drm->fd, drm->fb_id);
    munmap(drm->vaddr, drm->size);
    destroy.handle = drm->handle;
    drmIoctl(drm->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);

    drmModeFreeConnector(drm->conn);
    drmModeFreePlaneResources(drm->plane_res);
    drmModeFreeResources(drm->res);
}