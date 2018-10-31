## Cinder-PortAudio

PortAudio audio backend for cinder.

### Enabling Windows ASIO backend

Also see portaudio docs: ["Portaudio Windows ASIO with MSVC"](http://portaudio.com/docs/v19-doxydocs/compile_windows_asio_msvc.html)

For ASIO support, download ASIO Sdk from the Steinberg website [here](https://www.steinberg.net/en/company/developers.html), unzip and move to a folder at `lib/ASIOSDK`.

Then make sure that the following preprocessor macro is defined in our project:

```
PA_USE_ASIO
```

Lastly, include the following sources in your project:

```
lib/portaudio/src/hostapi/asio/*
lib/ASIOSDK/common/asio.cpp
lib/ASIOSDK/host/asiodrivers.cpp
lib/ASIOSDK/host/pc/asiolist.cpp
```

#### Enabling Unicode Character Set

To enable buildiing with Unicode Character Set (Configuration Properties -> General -> Character Set), you need to add the following line to the top of _lib/ASIOSDK/host/pc/asiolist.cpp_, directly above the <#include "windows.h>`:

```
#undef UNICODE
```

### Debugging

To turn on portaudio debug logging functionality, define the following preprocessor macros:

```
PA_LOG_API_CALLS=1
PA_ENABLE_DEBUG_OUTPUT
PA_ENABLE_MSVC_DEBUG_OUTPUT
```

The latter is obviously only useful on Windows.
