package com.xiaocai.android_v4l2

import android.graphics.Bitmap
import java.io.File

class V4l2Camera {

    private var fd: Int = -1

    external fun open(pathname: String?): CameraInfo?
    external fun setFrameSize(width: Int, height: Int)
    external fun start()
    external fun frameToBitmap(bitmap: Bitmap?)
    external fun isStart(): Boolean
    external fun getSupportFrameSize(): List<Size>

    external fun close()

    fun open(vid: Int, pid: Int): CameraInfo? {
        val file = File("/sys/class/video4linux/")
        file.listFiles()?.forEach {
            val text = File(it, "/device/modalias").readText()
            println(text)

            val v = text.substring(5, 9).toInt(16)
            val p = text.substring(10, 14).toInt(16)
            if (v == vid && p == pid) {
                return open("/dev/${it.name}")
            }
        }
        return null
    }

    companion object {
        init {
            System.loadLibrary("android_v4l2")
        }
    }
}