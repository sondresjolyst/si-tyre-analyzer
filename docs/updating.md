# Updating

The **desktop app** and the **car's firmware** update independently.

## The desktop app

The app checks for a newer release on launch and offers to update; you can also
click **Check for updates** in the header. Accept the prompt and it downloads the
new version, installs it, and restarts.

## The car's firmware

Firmware updates over Wi-Fi from the app.

1. Connect your computer to the master's Wi-Fi.
2. In the app, open **Sessions** and check **Host** is the master
   (`192.168.4.1`).
3. Click **Update firmware…**. The app compares the latest firmware with the
   device; if the car is current it says so and stops.
4. Confirm. The master flashes every paired wheel over the air.
5. When the wheels are done, the app asks whether to flash the **master** itself.
   Choose **Yes** to finish. The master reboots and its Wi-Fi drops briefly;
   reconnect once it returns.

### From a browser

The same actions, including a manual firmware-file upload, are available from the
master's own web page in Wi-Fi config mode.
