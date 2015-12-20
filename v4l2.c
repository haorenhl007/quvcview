#include <asm/types.h>          /* for videodev2.h */
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "v4l2.h"
#include "yuv422_rgb.h"

#define CLEAR(x) memset (&(x), 0, sizeof (x))

static unsigned int     n_buffers       = 0;


static void process_image (unsigned char *p, struct camera *cam)
{
    yuv422_rgb24(p, cam->rgbbuf, cam->width, cam->height);
}

void errno_exit(const char *s)
{
    fprintf (stderr, "%s error %d, %s\n",
             s, errno, strerror (errno));

    exit (EXIT_FAILURE);
}
static int xioctl(int fd, int request, void *arg)
{
    int r;

    do
    {
        r = ioctl (fd, request, arg);
    }
    while (-1 == r && EINTR == errno);

    return r;
}
void open_camera(struct camera *cam)
{
    struct stat st;

    if (-1 == stat (cam->device_name, &st))
    {
        fprintf (stderr, "Cannot identify '%s': %d, %s\n",
                 cam->device_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }

    if (!S_ISCHR (st.st_mode))
    {
        fprintf (stderr, "%s is no device\n", cam->device_name);
        exit (EXIT_FAILURE);
    }

    cam->fd = open (cam->device_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == cam->fd)
    {
        fprintf (stderr, "Cannot open '%s': %d, %s\n",
                 cam->device_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }
}
void close_camera(struct camera *cam)
{
    if (-1 == close (cam->fd))
        errno_exit ("close");

    cam->fd = -1;
}
int read_frame(struct camera *cam)
{
    struct v4l2_buffer buf;

    CLEAR (buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl (cam->fd, VIDIOC_DQBUF, &buf))
    {
        switch (errno)
        {
        case EAGAIN:
            return 0;

        case EIO:
        /* Could ignore EIO, see spec. */

        /* fall through */

        default:
            errno_exit ("VIDIOC_DQBUF");
        }
    }

    assert (buf.index < n_buffers);

    process_image (cam->buffers[buf.index].start, cam);

    if (-1 == xioctl (cam->fd, VIDIOC_QBUF, &buf))
        errno_exit ("VIDIOC_QBUF");
    return 1;
}

void start_capturing(struct camera *cam)
{
    unsigned int i;
    enum v4l2_buf_type type;

    for (i = 0; i < n_buffers; ++i)
    {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if (-1 == xioctl (cam->fd, VIDIOC_QBUF, &buf))
            errno_exit ("VIDIOC_QBUF");
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl (cam->fd, VIDIOC_STREAMON, &type))
        errno_exit ("VIDIOC_STREAMON");

}

void stop_capturing(struct camera *cam)
{
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl (cam->fd, VIDIOC_STREAMOFF, &type))
        errno_exit ("VIDIOC_STREAMOFF");

}
void uninit_camera(struct camera *cam)
{
    unsigned int i;


    for (i = 0; i < n_buffers; ++i)
        if (-1 == munmap (cam->buffers[i].start, cam->buffers[i].length))
            errno_exit ("munmap");

    free (cam->buffers);
}


static void init_mmap(struct camera *cam)
{
    struct v4l2_requestbuffers req;


    CLEAR (req);

    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    if (-1 == xioctl (cam->fd, VIDIOC_REQBUFS, &req))
    {
        if (EINVAL == errno)
        {
            fprintf (stderr, "%s does not support "
                     "memory mapping\n", cam->device_name);
            exit (EXIT_FAILURE);
        }
        else
        {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2)
    {
        fprintf (stderr, "Insufficient buffer memory on %s\n",
                 cam->device_name);
        exit (EXIT_FAILURE);
    }

    cam->buffers = calloc (req.count, sizeof (*(cam->buffers)));

    if (!cam->buffers)
    {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++(n_buffers))
    {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        if (-1 == xioctl (cam->fd, VIDIOC_QUERYBUF, &buf))
            errno_exit ("VIDIOC_QUERYBUF");

        cam->buffers[n_buffers].length = buf.length;
        cam->buffers[n_buffers].start =
            mmap (NULL /* start anywhere */,
                  buf.length,
                  PROT_READ | PROT_WRITE /* required */,
                  MAP_SHARED /* recommended */,
                  cam->fd, buf.m.offset);

        if (MAP_FAILED == cam->buffers[n_buffers].start)
            errno_exit ("mmap");
    }
}

void init_camera(struct camera *cam)
{
    struct v4l2_capability *cap;
    struct v4l2_cropcap *cropcap;
    struct v4l2_crop crop;
    struct v4l2_format *fmt;
    unsigned int min;

    cap = &(cam->v4l2_cap);
    cropcap = &(cam->v4l2_cropcap);
    fmt = &(cam->v4l2_fmt);

    if (-1 == xioctl (cam->fd, VIDIOC_QUERYCAP, cap))
    {
        if (EINVAL == errno)
        {
            fprintf (stderr, "%s is no V4L2 device\n",
                     cam->device_name);
            exit (EXIT_FAILURE);
        }
        else
        {
            errno_exit ("VIDIOC_QUERYCAP");
        }
    }

    if (!(cap->capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fprintf (stderr, "%s is no video capture device\n",
                 cam->device_name);
        exit (EXIT_FAILURE);
    }


    if (!(cap->capabilities & V4L2_CAP_STREAMING))
    {
        fprintf (stderr, "%s does not support streaming i/o\n",
                 cam->device_name);
        exit (EXIT_FAILURE);
    }

#ifdef DEBUG_CAM
    printf("\nVIDOOC_QUERYCAP\n");
    printf("the camera driver is %s\n", cap->driver);
    printf("the camera card is %s\n", cap->card);
    printf("the camera bus info is %s\n", cap->bus_info);
    printf("the version is %d\n", cap->version);
#endif
    /* Select video input, video standard and tune here. */


    CLEAR (*cropcap);

    cropcap->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl (cam->fd, VIDIOC_CROPCAP, cropcap))
    {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap->defrect; /* reset to default */

        if (-1 == xioctl (cam->fd, VIDIOC_S_CROP, &crop))
        {
            switch (errno)
            {
            case EINVAL:
                /* Cropping not supported. */
                break;
            default:
                /* Errors ignored. */
                break;
            }
        }
    }
    else
    {
        /* Errors ignored. */
    }


    CLEAR (*fmt);

    fmt->type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt->fmt.pix.width       = cam->width;
    fmt->fmt.pix.height      = cam->height;
    fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt->fmt.pix.field       = V4L2_FIELD_INTERLACED;

    if (-1 == xioctl (cam->fd, VIDIOC_S_FMT, fmt))
        errno_exit ("VIDIOC_S_FMT");

    /* Note VIDIOC_S_FMT may change width and height. */

    /* Buggy driver paranoia. */
    min = fmt->fmt.pix.width * 2;
    if (fmt->fmt.pix.bytesperline < min)
        fmt->fmt.pix.bytesperline = min;
    min = fmt->fmt.pix.bytesperline * fmt->fmt.pix.height;
    if (fmt->fmt.pix.sizeimage < min)
        fmt->fmt.pix.sizeimage = min;

    init_mmap (cam);

}
void get_cam_cap(struct camera  *cam)
{
    if (xioctl(cam->fd, VIDIOCGCAP, &cam->video_cap) == -1)
    {
        fprintf(stderr, "VIDIOCGCAP  --  could not get camera capabilities, exiting.....\n");
        exit(0);
    }
    if (cam->width > 0 && cam->height > 0)
    {
        if (cam->video_cap.maxwidth < cam->width)
        {
            cam->width = cam->video_cap.maxwidth;
        }
        if (cam->video_cap.minwidth > cam->width)
        {
            cam->width = cam->video_cap.minwidth;
        }
        if (cam->video_cap.maxheight < cam->height)
        {
            cam->height = cam->video_cap.maxheight;
        }
        if(cam->video_cap.minheight > cam->height)
        {
            cam->height = cam->video_cap.minheight;
        }
    }
#ifdef DEBUG_CAM
    printf("\nVIDIOCGCAP\n");
    printf("device name = %s\n", cam->video_cap.name);
    printf("device type = %d\n", cam->video_cap.type);
    printf("# of channels = %d\n", cam->video_cap.channels);
    printf("# of audio devices = %d\n", cam->video_cap.audios);
    printf("max width = %d\n", cam->video_cap.maxwidth);
    printf("max height = %d\n", cam->video_cap.maxheight);
    printf("min width = %d\n", cam->video_cap.minwidth);
    printf("min height = %d\n", cam->video_cap.minheight);
#endif
}
#ifdef DEBUG_CAM
void print_palette(int p)
{
    printf("The palette format is: ");
    switch (p)
    {
    case VIDEO_PALETTE_HI240:
        printf("High 240 cube (BT848)\n");
        break;

    case VIDEO_PALETTE_RGB565:
        printf("565 16 bit RGB\n");
        break;

    case VIDEO_PALETTE_RGB24:
        printf("24bit RGB\n");
        break;

    case VIDEO_PALETTE_RGB32:
        printf("32bit RGB\n");
        break;

    case VIDEO_PALETTE_RGB555:
        printf("555 15bit RGB\n");
        break;

    case VIDEO_PALETTE_YUV422:
        printf("YUV422 capture");
        break;

    case VIDEO_PALETTE_YUYV:
        printf("YUYV\n");
        break;

    case VIDEO_PALETTE_UYVY:
        printf("UYVY\n");
        break;

    case VIDEO_PALETTE_YUV420:
        printf("YUV420\n");
        break;

    case VIDEO_PALETTE_YUV411:
        printf("YUV411 capture\n");
        break;

    case VIDEO_PALETTE_RAW:
        printf("RAW capture (BT848)\n");
        break;

    case VIDEO_PALETTE_YUV422P:
        printf("YUV 4:2:2 Planar");
        break;

    case VIDEO_PALETTE_YUV411P:
        printf("YUV 4:1:1 Planar\n");
        break;

    case VIDEO_PALETTE_YUV420P:
        printf("YUV 4:2:0 Planar\n");
        break;

    case VIDEO_PALETTE_YUV410P:
        printf("YUV 4:1:0 Planar\n");
        break;
    }
}
#endif
void get_cam_pic(struct camera *cam)
{
    if (ioctl(cam->fd, VIDIOCGPICT, &cam->video_pic) == -1)
    {
        fprintf(stderr, "VIDIOCGPICT  --  could not get picture info, exiting....\n");
        exit(0);
    }
#ifdef DEBUG_CAM
    printf("\nVIDIOCGPICT:\n");
    printf("bright = %d\n", cam->video_pic.brightness);
    printf("hue = %d\n", cam->video_pic.hue);
    printf("colour = %d\n", cam->video_pic.colour);
    printf("contrast = %d\n", cam->video_pic.contrast);
    printf("whiteness = %d\n", cam->video_pic.whiteness);
    printf("colour depth = %d\n", cam->video_pic.depth);
    print_palette(cam->video_pic.palette);
#endif
}
void set_cam_pic(struct camera *cam)
{
    if(ioctl(cam->fd, VIDIOCSPICT, &cam->video_pic) == -1)
    {
        fprintf(stderr, "VIDIOCSPICT  --  could not set picture info, exiting....\n");
        exit(0);
    }
}
void get_cam_win(struct camera *cam)
{
    if(ioctl(cam->fd, VIDIOCGWIN, &cam->video_win) == -1)
    {
        fprintf(stderr, "VIDIOCGWIN  --  could not get window info, exiting....\n");
        exit(0);
    }
    //	cam->width = cam->video_win.width;
    //	cam->height = cam->video_win.height;
#ifdef DEBUG_CAM
    printf("\nVIDIOCGWIN\n");
    printf("x = %d\n", cam->video_win.x);
    printf("y = %d\n", cam->video_win.y);
    printf("width = %d\n", cam->video_win.width);
    printf("height = %d\n", cam->video_win.height);
    printf("chromakey = %d\n", cam->video_win.chromakey);
    printf("flags = %d\n", cam->video_win.flags);
#endif
}
void set_cam_win(struct camera *cam)
{
    if (ioctl(cam->fd, VIDIOCSWIN, &cam->video_win) == -1)
    {
        fprintf(stderr, "VIDIOCSWIN  --  could not set window info, exiting....\nerrno = %d", errno);
        exit(0);
    }
}

