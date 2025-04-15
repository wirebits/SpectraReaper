# SpectraReaper
A tool that deauth 2.4GHz and 5GHz Wi-Fi networks via BW16 on serial console.

# Key Features
- Minimal Setup.
- Easily controlled by Serial Console.
- It deauthenticates 2.4GHz and 5GHz WiFi networks both.

# Hardware Requirements
- Ai-Thinker BW16 RTL8720DN Development Board

# Note
- It has two variants : Type-B and Type-C connectors.

# Setup
1. Download Arduino IDE from [here](https://www.arduino.cc/en/software) according to your Operating System.
2. Install it.
3. Go to `File` → `Preferences` → `Additional Boards Manager URLs`.
4. Paste the following link :
   
   ```
   https://github.com/ambiot/ambd_arduino/raw/master/Arduino_package/package_realtek_amebad_index.json
   ```
5. Click on `OK`.
6. Go to `Tools` → `Board` → `Board Manager`.
7. Wait for sometimes and search `Realtek Ameba Boards (32-Bits ARM Cortex-M33@200MHz)` by `Realtek`.
8. Simply install it.
9. Restart the Arduino IDE by closing and open again.
10. Done!

# Install
1. Download or Clone the Repository.
2. Open the folder and just double click on `SpectraReaper.ino` file.
   - It opens in Arduino IDE.
3. Compile the code.
4. Select the correct board from the `Tools` → `Board` → `AmebaD ARM (32-bits) Boards`.
   - It is `Ai-Thinker BW16 (RTL8720DN)`.
6. Select the correct port number of that board.
7. Go to `Tools` → `Board` → `Auto Flash Mode` and select `Enable`.
8. Upload the code.
9. Done!

# Using Serial Console
1. Open Serial Console by click [here](https://wirebits.github.io/SerialConsole/).
2. Select baud rate `115200`.
3. Click on `Connect` button.
4. When it shows `Connected! Go On!` then your BW16 Development Board is ready to use.
5. Type `help` to get available commands.

# Indicators
- 🔵 - The BW16 scanning for nearby wifi networks.
- 🟢 - The BW16 board is ready to deauth.
- 🔴 - The BW16 board is sending deauthentication frames.
