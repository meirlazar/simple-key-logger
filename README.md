SKeylogger
==========

SKeylogger is a simple keylogger. I had previously been using a few other open source keyloggers, but they stopped working when I upgraded my operating system. I tried to look through the code of those keyloggers, but it was undocumented, messy, and complex. I decided to make my own highly documented and very simple keylogger.

<img width="1845" height="588" alt="image" src="https://github.com/user-attachments/assets/69e6c4ca-6b35-4be9-96e1-a917e2a4bfc9" />

## BUILD

1. Clone the repo `git clone https://github.com/gsingh93/simple-key-logger.git`
2. `cd simple-key-logger`
3. `make`
   
------------------------------------------------------------------------
## RUNNING 
1. Get the keyboard device input `cat /proc/bus/input/devices`
2. Look for your keyboard input device, it should have a line in the config that looks like this `H: Handlers=sysrq kbd event## leds`
3. The event## will be event with 2 digits after it, that is what we need.
4. You can make it easy to find by making a variable with the keyword of your keyboard manufacturer in it like this;
   `event=$(cat /proc/bus/input/devices | grep -i -A 5 "MyKeyboardManufacturerStringHere" | grep -iE "sysrq.*kbd.*leds" | grep -iEow "event[0-9]+")`

   Note: Change the keyword in the first grep to match your setup
6. Then use the variable in the next line to run the keylogger (assuming you cloned the repo in your home directory
   `sudo ${HOME}/simple-key-logger/skeylogger -l ${HOME}/simple-key-logger/logfile.log -d /dev/input/${event}`

    Note: This will start the keylogger in the background, logging to the logfile.log file in the repo's dir, using the correct device input captures.
8. Change the permissions of the logfile so you can read it `setfacl -m u:${USER}:r ${HOME}/simple-key-logger/logfile.log`
9. Watch it in realtime `tail -f ${HOME}/simple-key-logger/logfile.log`
 
   If the log was not written to in the last 5 seconds, it will create a newline with the timestamp and active window prepended and the keystrokes captured.

## STOPPING
1. You have a few options here but I like the command `pkill -f skeylogger`  short and sweet and to the point.

### NOTE: THIS HAS BEEN TESTED ON UBUNTU 24.04 (NOT WAYLAND - PROB WILL NOT WORK ON THAT)


Start on boot in Ubuntu (NOT TESTED - ORIGINAL DOCUMENTATION)
-----------------------
While Ubuntu isn't my main operating system (I'm an [Arch Linux](https://www.archlinux.org/) user), I got a few questions about how to start the keylogger when Ubuntu boots up. Here are the steps for Ubuntu 14.04:

1. Edit `/etc/rc.local` and add `/path/to/skeylogger` above the line with `exit 0`. Replace `/path/to/skeylogger` with the full path to the keylogger binary.

2. Allow sudo access without a password (Note: this may be a security threat. Do at your own risk). To do this, add the following line to `/etc/sudoers` making sure to replace the path with the path you used above and the username with your username:
    ```
    username ALL = NOPASSWD: /path/to/skeylogger
    ```
3. Reboot ubuntu. Open a terminal and type `pgrep skeylogger`. You should find one `skeylogger` process running.
