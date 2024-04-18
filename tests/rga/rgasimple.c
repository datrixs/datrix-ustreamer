#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <rga/rga.h>
#include <rga/RgaApi.h>
#include <rga/RgaUtils.h>

rga_info_t src, dst;
int main() {
    uint8_t buf[1280*720*2], dbuf[1280*720*4];
    for (int i = 0; i<720; i++) {
        for (int j = 0; j < 1280; j++) {
            buf[i*1280*4+j*2] = 128;
            buf[i*1280*4+j*2+1] = 255;
        }
    }
    
    int width = 1280, height = 720;

    //开启采集
    do {
        src.virAddr = buf;
        dst.virAddr = dbuf;
        rga_set_rect(&src.rect, 0, 0, width, height, width, height, RK_FORMAT_UYVY_422);
        rga_set_rect(&dst.rect, 0, 0, width, height, width, height, RK_FORMAT_BGRX_8888);

        c_RkRgaBlit(&src, &dst, NULL);
        printf("dst format=%d\n", dst.format);
    } while (0);

    return 0;
}