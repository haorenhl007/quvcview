#include "quvcview.h"
#include "ui_quvcview.h"

quvcview::quvcview(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::quvcview)
{
    ui->setupUi(this);

    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);

    timer = new QTimer(this);
    fpstimer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(readCamera()));
    connect(ui->open, SIGNAL(clicked()), this, SLOT(openCamera()));
    connect(ui->close, SIGNAL(clicked()), this, SLOT(closeCamera()));
    connect(fpstimer,SIGNAL(timeout()), this, SLOT(reflashfps()));

}

quvcview::~quvcview()
{
    delete ui;
}

int quvcview::InitCam(void)
{
    cam = (struct camera*)malloc(sizeof(struct camera));
    if (!cam)
    {
        qDebug("malloc camera failure!\n");
        return(EXIT_FAILURE);
    }

    cam->device_name = DEFAULT_DEV;
    cam->buffers = NULL;
    cam->width = IMAGE_WIDTH;
    cam->height = IMAGE_HEIGHT;

    cam->display_depth = 3;  /* RGB24 */
    cam->rgbbuf =(unsigned char *)malloc(cam->width * cam->height * cam->display_depth);
    if (!cam->rgbbuf)
    {
        qDebug("malloc rgbbuf failure!\n");
        return(EXIT_FAILURE);
    }

    open_camera(cam);
    get_cam_cap(cam);
    get_cam_pic(cam);
    get_cam_win(cam);
    cam->video_win.width = cam->width;
    cam->video_win.height = cam->height;
    set_cam_win(cam);
    close_camera(cam);
    open_camera(cam);
    get_cam_win(cam);
    init_camera(cam);
    start_capturing (cam);

    return EXIT_SUCCESS;
}

void quvcview::openCamera()
{
    ui->open->setEnabled(false);

    if(EXIT_FAILURE == InitCam())
    {
        qDebug("faild to open camera ! \n");
        exit(0);
    }

    timer->start(17); //60fps max

    lFPSsum = 0;

    fpstimer->start(1000); //60fps max
}

void quvcview::readCamera()
{
    if (!read_frame (cam))
    {
        qDebug("drop frame ! \n");
        return;
    }

    lFPSsum ++;

    QImage image = QImage((const uchar *)cam->rgbbuf, cam->width, cam->height, QImage::Format_RGB888).rgbSwapped();
    ui->label->setPixmap(QPixmap::fromImage(image));
}

void quvcview::closeCamera()
{
    close_camera(cam);

    ui->open->setEnabled(true);
}

void quvcview::reflashfps()
{
    ui->fpslabel->setNum(lFPSsum);
    lFPSsum = 0;
}
