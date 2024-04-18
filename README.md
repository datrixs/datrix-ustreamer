# datrix-uStreamer
datrix-ustreamer is based on the ustreamer project and ported to Rockchip.

-----
# Installation

## Building
You need to download the µStreamer onto your system and build it from the sources.

### Preconditions
You'll need  ```make```, ```gcc```, ```libevent``` with ```pthreads``` support, ```libjpeg9```/```libjpeg-turbo``` and ```libbsd``` (only for Linux).

* Arch: `sudo pacman -S libevent libjpeg-turbo libutil-linux libbsd`.
* Raspbian: `sudo apt install libevent-dev libjpeg9-dev libbsd-dev`. Add `libgpiod-dev` for `WITH_GPIO=1` and `libsystemd-dev` for `WITH_SYSTEMD=1` and `libasound2-dev libspeex-dev libspeexdsp-dev libopus-dev` for `WITH_JANUS=1`.
* Debian/Ubuntu: `sudo apt install build-essential libevent-dev libjpeg-dev libbsd-dev`.
* Alpine: `sudo apk add libevent-dev libbsd-dev libjpeg-turbo-dev musl-dev`. Build with `WITH_PTHREAD_NP=0`.

To enable GPIO support install [libgpiod](https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/about) and pass option ```WITH_GPIO=1```. If the compiler reports about a missing function ```pthread_get_name_np()``` (or similar), add option ```WITH_PTHREAD_NP=0``` (it's enabled by default). For the similar error with ```setproctitle()``` add option ```WITH_SETPROCTITLE=0```.

> **Note**
> Raspian: In case your version of Raspian is too old for there to be a libjpeg9 package, use `libjpeg8-dev` instead: `E: Package 'libjpeg9-dev' has no installation candidate`.

### Make
The most convenient process is to clone the µStreamer Git repository onto your system. 

```
$ git clone --depth=1 https://github.com/datrixs/datrix-ustreamer
$ cd ustreamer
$ make
$ ./ustreamer --help
```

## Update
Assuming you have a µStreamer clone as discussed above you can update µStreamer as follows.

```
$ cd ustreamer
$ git pull
$ make clean
$ make
```

-----
# Usage
Without arguments, ```ustreamer``` will try to open ```/dev/video0``` with 640x480 resolution and start streaming on  ```http://127.0.0.1:8080```. You can override this behavior using parameters ```--device```, ```--host``` and ```--port```. For example, to stream to the world, run:
```
# ./ustreamer --device=/dev/video1 --host=0.0.0.0 --port=80
```

:exclamation: Please note that since µStreamer v2.0 cross-domain requests were disabled by default for [security reasons](https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS). To enable the old behavior, use the option `--allow-origin=\*`.

## EDID
Add `-e EDID=1` to set HDMI EDID before starting ustreamer. Use together with `-e EDID_HEX=xx` to specify custom EDID data.

-----
# Integrations

## Janus
µStreamer supports bandwidth-efficient streaming using [H.264 compression](https://en.wikipedia.org/wiki/Advanced_Video_Coding) and the Janus WebRTC server. See the [Janus integration guide](docs/h264.md) for full details.

## Nginx
When uStreamer is behind an Nginx proxy, it's buffering behavior introduces latency into the video stream. It's possible to disable Nginx's buffering to eliminate the additional latency:

```nginx
location /stream {
    postpone_output 0;
    proxy_buffering off;
    proxy_ignore_headers X-Accel-Buffering;
    proxy_pass http://ustreamer;
}
```

-----
# Tips & tricks for v4l2
v4l2 utilities provide the tools to manage USB webcam setting and information. Scripts can be use to make adjustments and run manually or with cron. Running in cron for example to change the exposure settings at certain times of day. The package is available in all Linux distributions and is usually called `v4l-utils`.

* List of available video devices: `v4l2-ctl --list-devices`.
* List available control settings: `v4l2-ctl -d /dev/video0 --list-ctrls`.
* List available video formats: `v4l2-ctl -d /dev/video0 --list-formats-ext`.
* Read the current setting: `v4l2-ctl -d /dev/video0 --get-ctrl=exposure_auto`.
* Change the setting value: `v4l2-ctl -d /dev/video0 --set-ctrl=exposure_auto=1`.

[Here](https://www.kurokesu.com/main/2016/01/16/manual-usb-camera-settings-in-linux/) you can find more examples. Documentation is available in [`man v4l2-ctl`](https://www.mankier.com/1/v4l2-ctl).

-----
# See also
* [Running uStreamer via systemd service](https://github.com/pikvm/ustreamer/issues/16).

# Special thanks
* [Pikvm](https://github.com/pikvm)
* [Pikvm UStreamer](https://github.com/pikvm/ustreamer)
