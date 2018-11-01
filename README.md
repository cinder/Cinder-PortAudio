## Cinder-PortAudio

PortAudio audio backend for cinder.

## Supported Platforms

Currently this has only been tested on Windows Desktop, out of need for an ASIO backend. More will be added as needed. Supporting other platforms shouldn't be much more than a matter of getting the project configuration setup, if you'd like to help out with that then please discuss with issues or send a pull request.

### Installation

You can use [TinderBox](https://libcinder.org/docs/guides/tinderbox/index.html) to create a startup project. Clone this repo into your cinder/blocks folder and select the template "PortAudio: Sinewave". Note that the cinderblock.xml is setup to create a project with ASIO support, due to Steinberg's licensing you'll need to install the ASIO SDK following the instructions below.

### Usage

To use PortAudio as the master `audio::Context`, add the following include:

```
#include "cinder/audio/ContextPortAudio.h"
```

and then make the following call before any other audio calls:

```
audio::ContextPortAudio::setAsMaster();
```

After that, you can use `audio::master()` like you usually would with cinder's built in `audio::Context` implementations, and you can query which devices are available with the usual `audio::Device::printDevicesToString()`.

Also see the [PortAudioBasic](samples/PortAudioBasic/src/PortAudioBasicApp.cpp) sample for how to get up and running. It has two sets of configurations, one with ASIO support and one without (`Debug_NoAsio` and `Release_NoAsio`).

### Enabling Windows ASIO backend

For ASIO support, download ASIO Sdk from the Steinberg website [here](https://www.steinberg.net/en/company/developers.html), unzip and move to a folder at `lib/ASIOSDK`.

Then make sure that the following preprocessor macro is defined in our project:

```
PA_USE_ASIO=1
```

Lastly, include the following sources in your project:

```
lib/portaudio/src/hostapi/asio/*
lib/ASIOSDK/common/asio.cpp
lib/ASIOSDK/host/asiodrivers.cpp
lib/ASIOSDK/host/pc/asiolist.cpp
```

PortAudio also provides some docs: ["Portaudio Windows ASIO with MSVC"](http://portaudio.com/docs/v19-doxydocs/compile_windows_asio_msvc.html)

#### Enabling Unicode Character Set

To enable buildiing with Unicode Character Set (Configuration Properties -> General -> Character Set), you need to add the following line to the top of _lib/ASIOSDK/host/pc/asiolist.cpp_, directly above the <#include "windows.h>`:

```
#undef UNICODE
```

### Debugging

To turn on portaudio debug logging functionality, you can define the following preprocessor macros:

```
PA_LOG_API_CALLS=1
PA_ENABLE_DEBUG_OUTPUT
```

If using Visual Studio, you also need the following to see the logs in the console:

```
PA_ENABLE_MSVC_DEBUG_OUTPUT
```

