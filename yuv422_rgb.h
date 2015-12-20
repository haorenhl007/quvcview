#ifndef _YUV422_RGB_H
#define _YUV422_RGB_H
//#include <libv4l1-videodev.h>
//#include <linux/videodev2.h>
#include <linux/videodev.h>
#ifdef __cplusplus
extern "C" {
#endif

int  yuv422_rgb24(unsigned char *yuv_buf, unsigned char *rgb_buf, unsigned int width, unsigned int height);

#ifdef __cplusplus
}
#endif

#endif
