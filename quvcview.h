#ifndef QUVCVIEW_H
#define QUVCVIEW_H

#include <QMainWindow>
#include <QTime>
#include <QTimer>
#include <QDebug>
#include <QImage>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>    /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include "v4l2.h"


#define DEFAULT_DEV "/dev/video1"

#define IMAGE_WIDTH	 320
#define IMAGE_HEIGHT 240


namespace Ui {
class quvcview;
}

class quvcview : public QMainWindow
{
    Q_OBJECT

public:
    explicit quvcview(QWidget *parent = 0);
    ~quvcview();

private:
    Ui::quvcview *ui;  
    QTimer *timer;
    QTimer *fpstimer;
    struct camera *cam;

    int lFPSsum;

    int InitCam(void);

private slots:
    void openCamera();
    void readCamera();
    void closeCamera();
    void reflashfps();

};

#endif // QUVCVIEW_H
