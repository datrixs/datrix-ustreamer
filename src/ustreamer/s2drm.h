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
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/mman.h>
#include <drm_fourcc.h>

#include <rga/rga.h>
#include <rga/RgaApi.h>
#include <rga/RgaUtils.h>

#include "../libs/logging.h"
#include "../libs/tools.h"
#include "../libs/frame.h"


typedef struct {
    char *path;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t handle;
    uint32_t size;
    uint32_t fd;
    uint32_t fb_id;
    uint64_t last_id;
    uint8_t *vaddr;
    long double last_client_ts;

    drmModeConnector *conn;
    drmModeRes *res;
    drmModePlaneRes *plane_res;

    int cr_r_tab[256];
    int cb_b_tab[256];
    int cr_g_tab[256];
    int cb_g_tab[256];
} us_drm_s;

us_drm_s *us_drm_init(int width, int height);
void us_drm_destroy(us_drm_s *drm);
int drm_card_open(int *out);
int drm_create_fd(int fd, us_drm_s *drm);
int us_drm_draw(us_drm_s *drm, const us_frame_s *frame);