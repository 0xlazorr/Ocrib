use std::env;
use std::fs::File;
use std::io::{self, Read, Write};
use std::net::TcpStream;
use std::path::Path;
use std::process::Command;
use std::thread;
use std::time::Duration;

const DEFAULT_PORT: u16 = 8080;
const DEFAULT_HOST: &str = "127.0.0.1";
const AUTH_KEY_FILE: &str = "auth.key";

// Helper function to stream downloads directly to disk (equivalent to C fwrite callback)
// Prevents buffering the entire file into RAM, suitable for embedded limits.
fn download_file(url: &str, filename: &str) -> Result<(), Box<dyn std::error::Error>> {
    // Note: To compile without external dependencies, we use std::process to run curl.
    // However, we avoid shell command injection by passing arguments as an array of strings 
    // to the kernel execve, rather than running system() which spawns a subshell.
    let status = Command::new("curl")
        .arg("-L")
        .arg("-o")
        .arg(filename)
        .arg(url)
        .status()?;

    if status.success() {
        Ok(())
    } else {
        Err("Failed to download file via curl".into())
    }
}

fn send_desktop_notification(filename: &str) {
    #[cfg(target_os = "macos")]
    {
        let applescript = format!("display notification \"Download Complete: {}\" with title \"Ocrib Agent\"", filename);
        let _ = Command::new("osascript")
            .arg("-e")
            .arg(&applescript)
            .status();
    }

    #[cfg(not(target_os = "macos"))]
    {
        // Safe execution: avoids Shell/Command Injection!
        // Arguments are passed as individual array items directly to execve.
        // Even if filename is "file; rm -rf /", notify-send will print it literally.
        if Command::new("which").arg("notify-send").status().map(|s| s.success()).unwrap_or(false) {
            let _ = Command::new("notify-send")
                .arg("Ocrib Agent")
                .arg(format!("Download Complete: {}", filename))
                .status();
        }
    }
}

fn get_device_name() -> String {
    // Rust standard API for hostname lookup
    hostname::get()
        .map(|h| h.to_string_lossy().into_owned())
        .unwrap_or_else(|_| "UnknownRustDevice".to_string())
}

fn main() -> io::Result<()> {
    let args: Vec<String> = env::args().collect();
    let server_host = args.get(1).map(|s| s.as_str()).unwrap_or(DEFAULT_HOST);
    let server_port = args
        .get(2)
        .and_then(|s| s.parse::<u16>().ok())
        .unwrap_or(DEFAULT_PORT);

    let mut token = String::new();
    let mut is_paired = false;

    // Read existing pairing token
    if Path::new(AUTH_KEY_FILE).exists() {
        if let Ok(mut file) = File::open(AUTH_KEY_FILE) {
            if file.read_to_string(&mut token).is_ok() {
                token = token.trim().to_string();
                if !token.is_empty() {
                    is_paired = true;
                }
            }
        }
    }

    println!("Ocrib Rust Agent Starting...");
    println!("Connecting to {}:{}", server_host, server_port);

    loop {
        // Reconnect Loop
        match TcpStream::connect((server_host, server_port)) {
            Ok(mut stream) => {
                let mut buffer = [0u8; 2048];

                if !is_paired {
                    // Safe PIN generation
                    let pin: u32 = 100000 + rand::random::<u32>() % 900000;
                    let hostname = get_device_name();

                    println!("\n========================================");
                    println!(" NEW DEVICE DETECTED (Device: {})", hostname);
                    println!(" Enter this PIN on your web dashboard:");
                    println!(" ---> {} <--- ", pin);
                    println!("========================================\n");

                    let pin_msg = format!("PIN:{}|NAME:{}", pin, hostname);
                    if stream.write_all(pin_msg.as_bytes()).is_err() {
                        thread::sleep(Duration::from_secs(5));
                        continue;
                    }

                    match stream.read(&mut buffer) {
                        Ok(bytes) if bytes > 0 => {
                            let resp = String::from_utf8_lossy(&buffer[..bytes]);
                            if resp.starts_with("VERIFIED:") {
                                token = resp[9..].trim().to_string();
                                
                                // Safe file writing (atomic creation)
                                if let Ok(mut file) = File::create(AUTH_KEY_FILE) {
                                    let _ = file.write_all(token.as_bytes());
                                }
                                is_paired = true;
                                println!("Pairing successful!");
                            } else {
                                thread::sleep(Duration::from_secs(5));
                                continue;
                            }
                        }
                        _ => {
                            thread::sleep(Duration::from_secs(5));
                            continue;
                        }
                    }
                } else {
                    let auth_msg = format!("TOKEN:{}", token);
                    if stream.write_all(auth_msg.as_bytes()).is_err() {
                        thread::sleep(Duration::from_secs(5));
                        continue;
                    }
                    println!("Ocrib Active (Rust Agent).");
                }

                // Inner message loop
                loop {
                    match stream.read(&mut buffer) {
                        Ok(bytes) if bytes > 0 => {
                            let task_url = String::from_utf8_lossy(&buffer[..bytes]).trim().to_string();
                            println!("\nIncoming task: {}", task_url);

                            // Extract filename safely using Rust Path API
                            let filename = Path::new(&task_url)
                                .file_name()
                                .and_then(|f| f.to_str())
                                .map(|f| f.split('?').next().unwrap_or(f))
                                .unwrap_or("downloaded_file");

                            println!("Downloading to: {}", filename);
                            match download_file(&task_url, filename) {
                                Ok(_) => {
                                    println!("Saved: {}", filename);
                                    send_desktop_notification(filename);
                                }
                                Err(e) => {
                                    eprintln!("Download failed: {}", e);
                                }
                            }
                        }
                        _ => {
                            println!("Connection lost. Retrying...");
                            break;
                        }
                    }
                }
            }
            Err(e) => {
                eprintln!("Failed to connect: {}. Retrying in 5 seconds...", e);
                thread::sleep(Duration::from_secs(5));
            }
        }
    }
}
