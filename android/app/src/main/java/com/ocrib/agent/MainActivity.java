package com.ocrib.agent;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {

    private EditText hostInput;
    private EditText portInput;
    private TextView statusText;
    private TextView pinLabel;
    private TextView pinText;
    private Button toggleServiceBtn;

    private SharedPreferences prefs;
    private Handler updateHandler;
    private Runnable updateRunnable;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        prefs = getSharedPreferences("ocrib_settings", Context.MODE_PRIVATE);

        hostInput = findViewById(R.id.hostInput);
        portInput = findViewById(R.id.portInput);
        statusText = findViewById(R.id.statusText);
        pinLabel = findViewById(R.id.pinLabel);
        pinText = findViewById(R.id.pinText);
        toggleServiceBtn = findViewById(R.id.toggleServiceBtn);
        Button saveBtn = findViewById(R.id.saveBtn);

        // Load saved values
        hostInput.setText(prefs.getString("broker_host", "127.0.0.1"));
        portInput.setText(String.valueOf(prefs.getInt("broker_port", 8080)));

        saveBtn.setOnClickListener(v -> {
            String host = hostInput.getText().toString().trim();
            String portStr = portInput.getText().toString().trim();

            if (host.isEmpty()) {
                Toast.makeText(this, "Host cannot be empty", Toast.LENGTH_SHORT).show();
                return;
            }

            int port = 8080;
            if (!portStr.isEmpty()) {
                try {
                    port = Integer.parseInt(portStr);
                } catch (NumberFormatException e) {
                    Toast.makeText(this, "Invalid port number", Toast.LENGTH_SHORT).show();
                    return;
                }
            }

            prefs.edit()
                    .putString("broker_host", host)
                    .putInt("broker_port", port)
                    .apply();

            Toast.makeText(this, "Settings saved. Restarting service...", Toast.LENGTH_SHORT).show();

            if (OcribAgentService.isRunning()) {
                stopService(new Intent(this, OcribAgentService.class));
                startForegroundService(new Intent(this, OcribAgentService.class));
            }
        });

        toggleServiceBtn.setOnClickListener(v -> {
            if (OcribAgentService.isRunning()) {
                stopService(new Intent(this, OcribAgentService.class));
            } else {
                startForegroundService(new Intent(this, OcribAgentService.class));
            }
            updateUI();
        });

        updateHandler = new Handler(Looper.getMainLooper());
        updateRunnable = new Runnable() {
            @Override
            public void run() {
                updateUI();
                updateHandler.postDelayed(this, 1000);
            }
        };
    }

    @Override
    protected void onResume() {
        super.onResume();
        updateUI();
        updateHandler.post(updateRunnable);
    }

    @Override
    protected void onPause() {
        super.onPause();
        updateHandler.removeCallbacks(updateRunnable);
    }

    private void updateUI() {
        boolean running = OcribAgentService.isRunning();
        if (running) {
            statusText.setText("Status: Running");
            statusText.setTextColor(0xFF4CAF50); // Green
            toggleServiceBtn.setText("Stop Service");
            toggleServiceBtn.setBackgroundColor(0xFFEF4444); // Red

            boolean paired = prefs.getBoolean("is_paired", false);
            if (paired) {
                pinLabel.setVisibility(View.GONE);
                pinText.setVisibility(View.GONE);
                statusText.setText("Status: Connected (Paired)");
            } else {
                int pin = prefs.getInt("pair_pin", 0);
                if (pin != 0) {
                    pinLabel.setVisibility(View.VISIBLE);
                    pinText.setVisibility(View.VISIBLE);
                    pinText.setText(String.valueOf(pin));
                }
            }
        } else {
            statusText.setText("Status: Stopped");
            statusText.setTextColor(0xFFEF4444); // Red
            toggleServiceBtn.setText("Start Service");
            toggleServiceBtn.setBackgroundColor(0xFF4CAF50); // Green
            pinLabel.setVisibility(View.GONE);
            pinText.setVisibility(View.GONE);
        }
    }
}
