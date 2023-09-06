package com.xiaocai.android_v4l2

import android.graphics.Bitmap
import android.os.Bundle
import android.os.SystemClock
import android.util.Log
import android.view.SurfaceHolder
import androidx.appcompat.app.AppCompatActivity
import com.xiaocai.android_v4l2.databinding.ActivityMainBinding
import kotlin.concurrent.thread

class MainActivity : AppCompatActivity() {

    private val TAG = "MainActivity"

    private lateinit var binding: ActivityMainBinding

    private val v4l2Camera = V4l2Camera()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.surface.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                val width = 1280
                val height = 720
                val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)

                /**
                 * 注意这里部分机器需要 chmod 777 /dev/video1
                 */

//                val cameraInfo = v4l2Camera.open("/dev/video1")
                val cameraInfo = v4l2Camera.open(0x0C45, 0x4653)
                Log.d(TAG, "camera name:${cameraInfo?.name}")
                val list = v4l2Camera.getSupportFrameSize()
                Log.d(TAG, "SupportFrameSize:${list}")
                v4l2Camera.setFrameSize(width, height)
                v4l2Camera.start()

                thread {
                    var start = SystemClock.elapsedRealtime()
                    var fps = 0

                    while (v4l2Camera.isStart()) {
                        v4l2Camera.frameToBitmap(bitmap)

                        try {
                            val surface = holder.surface
                            val canvas = surface.lockCanvas(null)
                            canvas?.drawBitmap(bitmap, 0f, 0f, null)
                            surface.unlockCanvasAndPost(canvas)
                        } catch (_: Exception) {
                        }

                        fps++
                        val end = SystemClock.elapsedRealtime()
                        if (end - start >= 1000) {
                            Log.d(TAG, "fps:$fps")
                            fps = 0
                            start = end
                        }
                    }
                }
            }

            override fun surfaceChanged(
                holder: SurfaceHolder,
                format: Int,
                width: Int,
                height: Int
            ) {

            }

            override fun surfaceDestroyed(holder: SurfaceHolder) {
                //记得close，释放资源
                v4l2Camera.close()
            }

        })
    }

}