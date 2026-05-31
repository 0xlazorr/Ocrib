package com.ocrib.agent;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Environment;
import android.os.IBinder;
import android.util.Log;
import androidx.core.app.NotificationCompat;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.Socket;
import java.net.URL;
import java.util.Random;

public class OcribAgentService extends Service {

    private static final String TAG = "OcribAgentService";
    private static final String CHANNEL_ID = "OcribAgentServiceChannel";
    private static final int NOTIFICATION_ID = 101;

    private static boolean isRunning = false;
    private Thread connectionThread;
    private Socket tcpSocket;
    private volatile boolean shouldRun = true;

    private SharedPreferences prefs;

    public static boolean isRunning() {
        return isRunning;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        prefs = getSharedPreferences("ocrib_settings", Context.MODE_PRIVATE);
        createNotificationChannel();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        isRunning = true;
        shouldRun = true;

        Intent notificationIntent = new Intent(this, MainActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(this,
                0, notificationIntent, PendingIntent.FLAG_IMMUTABLE);

        Notification notification = new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle("Ocrib Agent Active")
                .setContentText("Listening for remote downloads...")
                .setSmallIcon(android.R.drawable.stat_sys_download)
                .setContentIntent(pendingIntent)
                .setOngoing(true)
                .build();

        startForeground(NOTIFICATION_ID, notification);

        // Start background connection thread
        connectionThread = new Thread(this::runConnectionLoop, "OcribConnectionThread");
        connectionThread.start();

        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        isRunning = false;
        shouldRun = false;
        disconnectSocket();
        if (connectionThread != null) {
            connectionThread.interrupt();
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void disconnectSocket() {
        try {
            if (tcpSocket != null && !tcpSocket.isClosed()) {
                tcpSocket.close();
            }
        } catch (Exception e) {
            Log.e(TAG, "Error closing socket: ", e);
        }
    }

    private void runConnectionLoop() {
        while (shouldRun) {
            String host = prefs.getString("broker_host", "127.0.0.1");
            int port = prefs.getInt("broker_port", 8080);
            boolean isPaired = prefs.getBoolean("is_paired", false);
            String token = prefs.getString("hardware_token", "");

            try {
                Log.d(TAG, "Connecting to " + host + ":" + port);
                tcpSocket = new Socket(host, port);
                tcpSocket.setSoTimeout(0); // Infinite read timeout

                InputStream in = tcpSocket.getInputStream();
                OutputStream out = tcpSocket.getOutputStream();

                if (!isPaired) {
                    // Generate 6-digit PIN
                    int pin = 100000 + new Random().nextInt(900000);
                    prefs.edit().putInt("pair_pin", pin).apply();

                    String deviceName = Build.MODEL;
                    String pinMsg = "PIN:" + pin + "|NAME:" + deviceName;
                    out.write(pinMsg.getBytes());
                    out.flush();

                    byte[] buffer = new byte[1024];
                    int bytes = in.read(buffer);
                    if (bytes > 0) {
                        String response = new String(buffer, 0, bytes).trim();
                        if (response.startsWith("VERIFIED:")) {
                            String newToken = response.substring(9);
                            prefs.edit()
                                    .putString("hardware_token", newToken)
                                    .putBoolean("is_paired", true)
                                    .putInt("pair_pin", 0)
                                    .apply();
                            Log.d(TAG, "Pairing successful! Token received.");
                        } else {
                            disconnectSocket();
                            Thread.sleep(5000);
                            continue;
                        }
                    } else {
                        disconnectSocket();
                        Thread.sleep(5000);
                        continue;
                    }
                } else {
                    String authMsg = "TOKEN:" + token;
                    out.write(authMsg.getBytes());
                    out.flush();
                    Log.d(TAG, "Authenticated with token.");
                }

                // Inner message routing loop
                byte[] buffer = new byte[2048];
                while (shouldRun) {
                    int bytes = in.read(buffer);
                    if (bytes <= 0) break; // Disconnected

                    String taskUrl = new String(buffer, 0, bytes).trim();
                    Log.d(TAG, "Incoming download task: " + taskUrl);

                    // Download in a separate task so it doesn't block the TCP read loop
                    new Thread(() -> performDownload(taskUrl), "DownloadWorker").start();
                }

            } catch (Exception e) {
                Log.e(TAG, "Connection error: ", e);
                disconnectSocket();
            }

            // Retry delay
            try {
                if (shouldRun) {
                    Thread.sleep(5000);
                }
            } catch (InterruptedException e) {
                break;
            }
        }
    }

    private void performDownload(String urlString) {
        try {
            // Extract filename
            String filename = "downloaded_file";
            int lastSlash = urlString.lastIndexOf('/');
            if (lastSlash != -1) {
                filename = urlString.substring(lastSlash + 1);
            }
            int queryMark = filename.indexOf('?');
            if (queryMark != -1) {
                filename = filename.substring(0, queryMark);
            }

            // Determine output directory (App's external downloads folder is writable without request permission popup)
            File outputDir = getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
            File outputFile = new File(outputDir, filename);

            Log.d(TAG, "Downloading " + urlString + " to " + outputFile.getAbsolutePath());

            URL url = new URL(urlString);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setInstanceFollowRedirects(true);
            conn.connect();

            if (conn.getResponseCode() == HttpURLConnection.HTTP_OK) {
                try (InputStream is = new BufferedInputStream(conn.getInputStream());
                     OutputStream os = new FileOutputStream(outputFile)) {

                    byte[] data = new byte[4096];
                    int count;
                    while ((count = is.read(data)) != -1) {
                        os.write(data, 0, count);
                    }
                    os.flush();
                    Log.d(TAG, "Download finished. Saved: " + outputFile.getName());
                    showDownloadNotification(outputFile.getName());
                }
            } else {
                Log.e(TAG, "Server returned response code: " + conn.getResponseCode());
            }
            conn.disconnect();

        } catch (Exception e) {
            Log.e(TAG, "Download error: ", e);
        }
    }

    private void showDownloadNotification(String filename) {
        NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager == null) return;

        Notification notification = new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle("Download Complete")
                .setContentText("Saved: " + filename)
                .setSmallIcon(android.R.drawable.stat_sys_download_done)
                .setAutoCancel(true)
                .build();

        manager.notify((int) System.currentTimeMillis(), notification);
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel serviceChannel = new NotificationChannel(
                    CHANNEL_ID,
                    "Ocrib Agent Service Channel",
                    NotificationManager.IMPORTANCE_LOW
            );
            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(serviceChannel);
            }
        }
    }
}
