#include <jni.h>
#include <string>
#include <android/log.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <android/bitmap.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <mutex>
#include <iostream>
#include <map>

#define DEF_VIDEO_WIDTH 640
#define DEF_VIDEO_HEIGHT 480

#define COUNT 4

struct buffer {
    void * start;
    unsigned int length;
};

struct video {
    buffer * buffers;
    struct v4l2_buffer buf;
    unsigned char n_buffers;
    unsigned char index;
    bool isStart = false;
    int width = DEF_VIDEO_WIDTH;
    int height = DEF_VIDEO_HEIGHT;
};

std::mutex mtx;
std::map<int, struct video *> fmap;

#define TAG "v4l2_camera"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

int getFd(JNIEnv *env, jobject thiz)
{
    jclass cls = env->GetObjectClass(thiz); // 获取Java对象对应的类对象
    jfieldID fieldId = env->GetFieldID(cls, "fd", "I"); // 获取字段ID，这里假设字段名为"fd"，类型为"I"（int）
    jint fd = env->GetIntField(thiz, fieldId); // 获取字段的值
    return fd;
}

void setFd(JNIEnv *env, jobject thiz, int fd)
{
    jclass cls = env->GetObjectClass(thiz); // 获取Java对象对应的类对象
    jfieldID fieldId = env->GetFieldID(cls, "fd", "I"); // 获取字段ID，这里假设字段名为"fd"，类型为"I"（int）
    env->SetIntField(thiz, fieldId, fd); // 设置字段的值
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_xiaocai_android_1v4l2_V4l2Camera_open(JNIEnv *env, jobject thiz, jstring device_name) {
    const char * path = env->GetStringUTFChars(device_name, JNI_FALSE);
    int fd = open(path, O_RDWR);
    if (fd == -1)
    {
        return NULL;
    }

    struct buffer *buffers = NULL;

    buffers = (struct buffer*) malloc(COUNT * sizeof(struct buffer));

    if (!buffers)
    {
        LOGD("oom");
    }

    struct video * _video = NULL;
    _video = (struct video*) malloc(sizeof(struct video));
    
    _video->buffers = buffers;
    fmap[fd] = _video;

    setFd(env, thiz, fd);

    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        LOGD("querycap fail");
    } else
    {
        LOGD("name:%s", cap.card);

        if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == V4L2_CAP_VIDEO_CAPTURE)
        {
            LOGD("support video capture");
        }

        char buffer[50];
        sprintf(buffer, "%s", cap.card);

        jstring name = env->NewStringUTF(buffer);
        jclass cls = env->FindClass("com/xiaocai/android_v4l2/CameraInfo");
        jmethodID constructor = env->GetMethodID(cls, "<init>", "(Ljava/lang/String;)V");
        jobject obj2 = env->NewObject(cls, constructor, name);

        return obj2;
    }
    return NULL;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_xiaocai_android_1v4l2_V4l2Camera_close(JNIEnv *env, jobject thiz) {
    mtx.lock();

    jint fd = getFd(env, thiz);
    if (fd == -1)
    {
        return;
    }

    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ioctl(fd, VIDIOC_STREAMOFF, &type);

    struct video *_video = fmap[fd];
    struct buffer *buffers = _video->buffers;
    _video->isStart = false;
    fmap.erase(fd);

    for (int i = 0; i < _video->n_buffers; i++)
    {
        munmap(buffers[i].start, buffers[i].length);
    }

    free(buffers);
    free(_video);

    close(fd);
    fd = -1;
    setFd(env, thiz, fd);

    LOGD("close");

    mtx.unlock();
}
extern "C"
JNIEXPORT void JNICALL
Java_com_xiaocai_android_1v4l2_V4l2Camera_setFrameSize(JNIEnv *env, jobject thiz, jint width,jint height) {
    int fd = getFd(env, thiz);
    if (fmap.count(fd) && !fmap[fd]->isStart)
    {
        fmap[fd]->width = width;
        fmap[fd]->height = height;
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_xiaocai_android_1v4l2_V4l2Camera_start(JNIEnv *env, jobject thiz) {
    int fd = getFd(env, thiz);
    if (fd == -1)
    {
        return;
    }

    struct video *_video = fmap[fd];
    struct buffer *buffers = _video->buffers;

    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_format fmt;
    struct v4l2_streamparm stream_para;

    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    LOGD("support format:\n");
    while(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != -1)
    {
        LOGD("%s\n", fmtdesc.description);
        fmtdesc.index++;
    }

    struct v4l2_format fmt_test;
    fmt_test.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt_test.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    if (ioctl(fd, VIDIOC_TRY_FMT, &fmt_test) == -1)
    {
        LOGD("not support YUYV");
    } else
    {
        LOGD("support YUYV");
    }

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.width = _video->width;
    fmt.fmt.pix.height = _video->height;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        LOGD("unable set to format");
    }

    //设置帧率30
    memset(&stream_para, 0, sizeof(struct v4l2_streamparm));
    stream_para.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    stream_para.parm.capture.timeperframe.denominator = 30;
    stream_para.parm.capture.timeperframe.numerator = 1;

    if (ioctl(fd, VIDIOC_S_PARM, &stream_para) == -1)
    {
        LOGD("unable set to frame");
    }

    struct v4l2_requestbuffers req;

    req.count = COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1)
    {
        LOGD("request for buffers error");
    }

    struct v4l2_buffer buf;
    for (_video->n_buffers = 0; _video->n_buffers < req.count; _video->n_buffers++)
    {
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = _video->n_buffers;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            LOGD("query buffer error");
        }

        buffers[_video->n_buffers].length = buf.length;
        buffers[_video->n_buffers].start = mmap(NULL, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[_video->n_buffers].start == MAP_FAILED)
        {
            LOGD("buffer map error");
        }

    }

    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &type);

    for (_video->index = 0; _video->index < _video->n_buffers; ++_video->index) {
        buf.index = _video->index;
        ioctl(fd, VIDIOC_QBUF, &buf);
    }

    _video->buf = buf;
    _video->isStart = true;
    _video->index = 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_xiaocai_android_1v4l2_V4l2Camera_frameToBitmap(JNIEnv *env, jobject thiz, jobject bitmap) {
    mtx.lock();

    int fd = getFd(env, thiz);

    if (fd != -1)
    {
        struct video *_video = fmap[fd];
        struct buffer *buffers = _video->buffers;

        int ret;
        _video->buf.index = _video->index;

        ret = ioctl(fd, VIDIOC_DQBUF, &_video->buf);
        if (ret == 0)
        {
            void * pixels;

            if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0)
            {
                LOGD("bitmap lockPixels error");
            }

            unsigned char * src = (unsigned char *) buffers[_video->buf.index].start;

            int frameSize = _video->width * _video->height * 2;

            int * colors = (int *) pixels;
            for (int i = 0; i < frameSize; i+=4)
            {
                unsigned char y1, y2, u, v;
                y1 = src[i];
                u = src[i+1];
                y2 = src[i+2];
                v = src[i+3];

                int r1 = y1 + ((360 * (v - 128)) >> 8);
                int g1 = y1 - (((88 * (u - 128) + 184 * (v - 128))) >> 8);
                int b1 = y1 + ((455 * (u - 128)) >> 8);

                int r2 = y2 + ((360 * (v - 128)) >> 8);
                int g2 = y2 - (((88 * (u - 128) + 184 * (v - 128))) >> 8);
                int b2 = y2 + ((455 * (u - 128)) >> 8);

                r1 = r1>255 ? 255 : r1<0 ? 0 : r1;
                g1 = g1>255 ? 255 : g1<0 ? 0 : g1;
                b1 = b1>255 ? 255 : b1<0 ? 0 : b1;
                r2 = r2>255 ? 255 : r2<0 ? 0 : r2;
                g2 = g2>255 ? 255 : g2<0 ? 0 : g2;
                b2 = b2>255 ? 255 : b2<0 ? 0 : b2;

                *colors++ = 0xff000000 | b1<<16 | g1<<8 | r1;
                *colors++ = 0xff000000 | b2<<16 | g2<<8 | r2;
            }

            AndroidBitmap_unlockPixels(env, bitmap);

            ioctl(fd, VIDIOC_QBUF, &_video->buf);
            _video->index++;
            if (_video->index >= _video->n_buffers)
            {
                _video->index = 0;
            }
        }
    }

    mtx.unlock();
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_xiaocai_android_1v4l2_V4l2Camera_isStart(JNIEnv *env, jobject thiz) {
    int fd = getFd(env, thiz);

    if (fmap.count(fd))
    {
        return fmap[fd]->isStart;
    }
    return false;
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_xiaocai_android_1v4l2_V4l2Camera_getSupportFrameSize(JNIEnv *env, jobject thiz) {
    jclass listClass = env->FindClass("java/util/ArrayList");
    jmethodID constructor = env->GetMethodID(listClass, "<init>", "()V");
    jobject list = env->NewObject(listClass, constructor);

    int fd = getFd(env, thiz);
    if (fd != -1)
    {
        struct v4l2_frmsizeenum frmsize;

        frmsize.index = 0;
        frmsize.pixel_format = V4L2_PIX_FMT_YUYV;

        while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0)
        {
            jclass sizeClass = env->FindClass("com/xiaocai/android_v4l2/Size");
            jmethodID _constructor = env->GetMethodID(sizeClass, "<init>", "(II)V");
            jobject size = env->NewObject(sizeClass,_constructor,
                                          (int) frmsize.discrete.width,
                                          (int) frmsize.discrete.height);

            jmethodID add = env->GetMethodID(listClass, "add", "(ILjava/lang/Object;)V");
            env->CallVoidMethod(list, add, (int) frmsize.index, size);

            frmsize.index++;
        }
    }

    return list;
}