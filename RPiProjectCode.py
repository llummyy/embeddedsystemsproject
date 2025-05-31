
import tkinter as tk
from tkinter import scrolledtext
import paho.mqtt.client as mqtt

# MQTT settings
broker = "49dcc968ea4a46e28a0488449edcbb8d.s1.eu.hivemq.cloud"
port = 8883
username = "hivemq.webclient.1747723393121"
password = "5l&$gs0IF3qySRZ?9;aC"
topic = "DoorLockAccess"

# Send message to broker
def send_command(command):
    client.publish(topic, command)
    log_message(f"Sent: {command}")

# Add text to log
def log_message(message):
    log_text.config(state=tk.NORMAL)
    log_text.insert(tk.END, message + "\n")
    log_text.yview(tk.END)
    log_text.config(state=tk.DISABLED)

# When connected
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        status_label.config(text="Connected to MQTT", fg="green")
        client.subscribe(topic)
    else:
        status_label.config(text="Connection failed", fg="red")

# When a message is received
def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    log_message(f"Received: {payload}")

# Setup GUI
root = tk.Tk()
root.title("MQTT Door Controller")

# Frames for layout
left_frame = tk.Frame(root)
left_frame.pack(side=tk.LEFT, padx=10, pady=10)

right_frame = tk.Frame(root)
right_frame.pack(side=tk.RIGHT, padx=10, pady=10)

# Status label
status_label = tk.Label(left_frame, text="Connecting to MQTT...", fg="orange")
status_label.pack(pady=5)

# Buttons
unlock_button = tk.Button(left_frame, text="Unlock", width=15, command=lambda: send_command("unlock"))
unlock_button.pack(pady=5)

lock_button = tk.Button(left_frame, text="Lock", width=15, command=lambda: send_command("lock"))
lock_button.pack(pady=5)

# Log box
log_text = scrolledtext.ScrolledText(right_frame, width=50, height=20, wrap=tk.WORD, font=("Courier", 10))
log_text.pack()
log_text.insert(tk.END, "Waiting for access logs...\n")
log_text.config(state=tk.DISABLED)

# MQTT setup
client = mqtt.Client()
client.username_pw_set(username, password)
client.tls_set()
client.on_connect = on_connect
client.on_message = on_message

# Start MQTT
client.connect(broker, port)
client.loop_start()

# Start GUI
root.mainloop()




