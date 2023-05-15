# Stills Camera Controller for Hadal Landers

This project implements a controller for a stills camera deployed on the Inkfish landers. The objective is to capture an image at a specific interval.

Currently, the project has been tested with a [MindVision GE502C][] [GigE Vision][] industrial camera as a proxy for the camera to be installed on the lander system.

  [MindVision GE502C]: https://www.mindvision.com.cn/index_en.aspx
  [GigE Vision]: https://en.wikipedia.org/wiki/GigE_Vision


## Build and Usage

    make
    ./capture --gst-plugin-path=/usr/local/lib/aarch64-linux-gnu/gstreamer-1.0


## Theory of Operation

The controller software uses the [GStreamer][] multimedia framework to implement a video processing pipeline. This pipeline receives Bayer pattern images from the camera, converts them to RGB, encodes them with a video codec, muxes them into a container format, and saves them to rotating files disk.

  [GStreamer]: https://gstreamer.freedesktop.org

Communication with the camera is done using [Aravis](https://github.com/AravisProject/aravis), rather than proprietary, vendor-specific libraries.

An [intervalometer][] is implemented based on the computer's wall clock time—i.e., if the interval is set to 20 seconds, the trigger will fire at :00, :20, :40. In testing on a Raspberry Pi 4, the timing is usually accurate to 0.002 seconds. The trigger is sent to the camera as a GenICam command, rather than an electrical pulse.

  [intervalometer]: https://en.wikipedia.org/wiki/Intervalometer


## Codec

Presently, the video codec used is [FFV1][], which offers lossless compression and is considered by organizations such as the U.S. Library of Congress suitable for archival purposes.

  [FFV1]: https://en.wikipedia.org/wiki/FFV1

The format can be played back in [VLC][], or converted with [FFmpeg][]. For example, to transcode to a QuickTime-compatible lossy H.264 using the MP4 container format:

    ffmpeg -i video.ffv1.mkv -c:v libx264 -pix_fmt yuv420p video.h264.mp4

  [FFmpeg]: http://www.ffmpeg.org/
  [VLC]: http://www.videolan.org/vlc/


### Limitations

Due to limitations in GStreamer, it is not possible to retain the original Bayer pattern image, nor to work with greater than 8-bit color depth even if the camera supports it. Thus we cannot truly save the raw camera output.

If this is important, we can consider ejecting from GStreamer and storing the raw data from Aravis in a container format like [MCAP][]. Additional tooling would be required to extract the frames into a viewable format.

[MCAP]: https://mcap.dev/


## Camera Configuration

This software makes minimal changes to the camera configuration. For example, the exposure and gain settings are not automatically adjusted.

Any settings that need to be adjusted on the camera should be done through Aravis command line interface, `arv-tool`. For example:

    arv-tool control Gain=4


## Time Coding

Currently, the precise time of each trigger is not recorded in the output file. This is a high priority feature but there were obstacles, such as an [GStreamer issue][gst-timecode-issue] related to timecodes and fractional framerates.

  [gst-timecode-issue]: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/2465


## Time Synchronization

GPS can be used to precisely synchronize the clocks across independently-running camera controllers. Because the intervalometer is based on the wall clock time, this should result in the cameras triggering at approximately the same instant. (This technique is inspired by the [Straightedge][] art installation, which uses GPS to synchronize lights over a 2.6 mile span.)

  [Straightedge]: https://www.ardentheavyindustries.com/straightedge/

[gpsd][] is a popular project for interfacing a Linux system with GPS receivers. Alone, gpsd does not set the system clock. A service such as [chrony][], ntpd, etc. is needed for this. See [this tutorial][gps-tutorial] as a starting point.

  [chrony]: https://chrony.tuxfamily.org/
  [gpsd]: https://gpsd.io/
  [gps-tutorial]: https://gpsd.gitlab.io/gpsd/gpsd-time-service-howto.html

Configuring a GPS-disciplined system clock can be thorny, especially with pulse-per-second (PPS) time sources. On Ubuntu 22.04, we encountered overzealous AppArmor profiles and strange behavior with how gpsd takes ownership of our pps0 device. Some helpful config files for the [Adafruit Ultimate GPS Hat for Raspberry Pi][gps-hat] are provided under `support/gps/`.

  [gps-hat]: https://www.adafruit.com/product/2324


## Strobes

Strobes have not yet been integrated. The camera provides a strobe output pin and delay settings that can be configrued.


## Considerations for MindVision Camera

The MindVision camera provides a similar interface to the eventual camera we hope to deploy on the landers, but is a low-cost unit that will inevitably differ from the final camera.

A patch to Aravis is provided in the `support/aravis/` directory to work around an issue with the camera failing to describe supported framerates. The issue is discussed in [this forum thread][mindvision-thread].

  [mindvision-thread]: https://aravis-project.discourse.group/t/camera-works-in-arv-viewer-but-cannot-be-read-by-gst-aravis-launch-aravissrc/439
