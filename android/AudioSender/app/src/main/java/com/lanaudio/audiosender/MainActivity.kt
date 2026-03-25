package com.lanaudio.audiosender

import android.Manifest
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
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
    private var targetIp = ""
    private val targetPort = 9000

    private lateinit var btnToggle: Button
    private lateinit var ipInput: EditText
    private lateinit var tvStatus: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        btnToggle = findViewById(R.id.btnToggle)
        ipInput = findViewById(R.id.ipInput)
        tvStatus = findViewById(R.id.tvStatus)

        btnToggle.setOnClickListener {
            if (isStreaming) {
                stopStreaming()
            } else {
                targetIp = ipInput.text.toString()
                if (targetIp.isBlank()) {
                    Toast.makeText(this, "Please enter an IP address", Toast.LENGTH_SHORT).show()
                    return@setOnClickListener
                }
                
                if (checkPermissions()) {
                    startStreaming()
                } else {
                    requestPermissions()
                }
            }
        }
    }

    private fun checkPermissions(): Boolean {
        return ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED
    }

    private fun requestPermissions() {
        ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.RECORD_AUDIO), 1)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 1 && grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            startStreaming()
        } else {
            Toast.makeText(this, "Microphone permission is required", Toast.LENGTH_LONG).show()
        }
    }

    private fun startStreaming() {
        try {
            val minBufSize = AudioRecord.getMinBufferSize(
                44100,
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT
            )

            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
                return
            }

            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.MIC,
                44100,
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT,
                minBufSize.coerceAtLeast(1024)
            )

            audioRecord?.startRecording()
            isStreaming = true
            
            btnToggle.text = "Stop Streaming"
            btnToggle.setBackgroundColor(0xFFE53935.toInt()) // Red
            tvStatus.text = "Status: Streaming to $targetIp..."

            thread {
                try {
                    val socket = DatagramSocket()
                    val address = InetAddress.getByName(targetIp)
                    val buf = ShortArray(256) // 256 samples per chunk to match C++ CHUNK = 256
                    
                    // Max UDP payload: 16 byte header + 256 * 2 bytes = 528 bytes
                    val byteBuf = ByteBuffer.allocate(16 + 256 * 2)
                    byteBuf.order(ByteOrder.BIG_ENDIAN)

                    android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO)

                    while (isStreaming) {
                        val read = audioRecord?.read(buf, 0, 256) ?: 0
                        if (read > 0) {
                            byteBuf.clear()
                            // Header
                            byteBuf.putInt(0xABCD1234.toInt()) // magic
                            byteBuf.putInt(read * 2)           // payload bytes length
                            byteBuf.putLong(System.currentTimeMillis())
                            
                            // Payload
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
        
        btnToggle.text = "Start Streaming"
        btnToggle.setBackgroundColor(0xFF4CAF50.toInt()) // Green
        tvStatus.text = "Status: Stopped"
    }

    override fun onDestroy() {
        super.onDestroy()
        stopStreaming()
    }
}
