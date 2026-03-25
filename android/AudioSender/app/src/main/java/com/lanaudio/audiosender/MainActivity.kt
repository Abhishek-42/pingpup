package com.lanaudio.audiosender

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioPlaybackCaptureConfiguration
import android.media.AudioRecord
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.concurrent.thread

class MainActivity : AppCompatActivity() {

    private var isStreaming = false
    private var audioRecord: AudioRecord? = null
    private var mediaProjection: MediaProjection? = null
    private var targetIp = ""
    private val targetPort = 9000

    private lateinit var btnToggle: Button
    private lateinit var ipInput: EditText
    private lateinit var tvStatus: TextView

    private lateinit var projectionManager: MediaProjectionManager

    companion object {
        private const val REQUEST_AUDIO_PERMISSION = 1
        private const val REQUEST_MEDIA_PROJECTION = 2
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        btnToggle = findViewById(R.id.btnToggle)
        ipInput = findViewById(R.id.ipInput)
        tvStatus = findViewById(R.id.tvStatus)

        projectionManager = getSystemService(Context.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager

        btnToggle.setOnClickListener {
            if (isStreaming) {
                stopStreaming()
            } else {
                targetIp = ipInput.text.toString()
                if (targetIp.isBlank()) {
                    Toast.makeText(this, "Please enter an IP address", Toast.LENGTH_SHORT).show()
                    return@setOnClickListener
                }

                // Check audio permission first
                if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
                    ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.RECORD_AUDIO), REQUEST_AUDIO_PERMISSION)
                    return@setOnClickListener
                }

                // Request screen/audio capture permission
                requestMediaProjection()
            }
        }
    }

    private fun requestMediaProjection() {
        val captureIntent = projectionManager.createScreenCaptureIntent()
        startActivityForResult(captureIntent, REQUEST_MEDIA_PROJECTION)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_AUDIO_PERMISSION && grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            requestMediaProjection()
        } else {
            Toast.makeText(this, "Audio permission is required", Toast.LENGTH_LONG).show()
        }
    }

    @Deprecated("Use Activity Result API")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_MEDIA_PROJECTION) {
            if (resultCode == Activity.RESULT_OK && data != null) {
                mediaProjection = projectionManager.getMediaProjection(resultCode, data)
                startStreaming()
            } else {
                Toast.makeText(this, "Screen capture permission denied", Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun startStreaming() {
        try {
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
                return
            }

            val sampleRate = 44100
            val channelConfig = AudioFormat.CHANNEL_IN_MONO
            val audioFormat = AudioFormat.ENCODING_PCM_16BIT
            val minBufSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat)

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && mediaProjection != null) {
                // Android 10+: Capture internal/system audio
                val playbackConfig = AudioPlaybackCaptureConfiguration.Builder(mediaProjection!!)
                    .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
                    .addMatchingUsage(AudioAttributes.USAGE_GAME)
                    .addMatchingUsage(AudioAttributes.USAGE_UNKNOWN)
                    .build()

                audioRecord = AudioRecord.Builder()
                    .setAudioFormat(
                        AudioFormat.Builder()
                            .setEncoding(audioFormat)
                            .setSampleRate(sampleRate)
                            .setChannelMask(channelConfig)
                            .build()
                    )
                    .setBufferSizeInBytes(minBufSize.coerceAtLeast(4096))
                    .setAudioPlaybackCaptureConfig(playbackConfig)
                    .build()

                tvStatus.text = "Status: Streaming SYSTEM AUDIO to $targetIp..."
            } else {
                // Fallback: use microphone
                audioRecord = AudioRecord(
                    android.media.MediaRecorder.AudioSource.MIC,
                    sampleRate, channelConfig, audioFormat,
                    minBufSize.coerceAtLeast(1024)
                )
                tvStatus.text = "Status: Streaming MIC to $targetIp..."
            }

            audioRecord?.startRecording()
            isStreaming = true

            btnToggle.text = "Stop Streaming"
            btnToggle.setBackgroundColor(0xFFE53935.toInt())

            thread {
                try {
                    val socket = DatagramSocket()
                    val address = InetAddress.getByName(targetIp)
                    val buf = ShortArray(256)

                    // Header(16) + payload(256 * 2) = 528 bytes
                    val byteBuf = ByteBuffer.allocate(16 + 256 * 2)
                    byteBuf.order(ByteOrder.BIG_ENDIAN)

                    android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO)

                    while (isStreaming) {
                        val read = audioRecord?.read(buf, 0, 256) ?: 0
                        if (read > 0) {
                            byteBuf.clear()
                            // Header (big-endian)
                            byteBuf.putInt(0xABCD1234.toInt())
                            byteBuf.putInt(read * 2)
                            byteBuf.putLong(System.currentTimeMillis())

                            // Payload (big-endian shorts)
                            for (i in 0 until read) {
                                byteBuf.putShort(buf[i])
                            }

                            val packet = DatagramPacket(byteBuf.array(), byteBuf.position(), address, targetPort)
                            socket.send(packet)
                        }
                    }
                    socket.close()
                } catch (e: Exception) {
                    e.printStackTrace()
                    runOnUiThread {
                        Toast.makeText(this@MainActivity, "Network Error: ${e.message}", Toast.LENGTH_LONG).show()
                        stopStreaming()
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
            Toast.makeText(this, "Audio Error: ${e.message}", Toast.LENGTH_LONG).show()
            stopStreaming()
        }
    }

    private fun stopStreaming() {
        isStreaming = false
        audioRecord?.stop()
        audioRecord?.release()
        audioRecord = null
        mediaProjection?.stop()
        mediaProjection = null

        btnToggle.text = "Start Streaming"
        btnToggle.setBackgroundColor(0xFF4CAF50.toInt())
        tvStatus.text = "Status: Stopped"
    }

    override fun onDestroy() {
        super.onDestroy()
        stopStreaming()
    }
}
