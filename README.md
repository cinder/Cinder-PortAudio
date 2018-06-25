## Cinder-PortAudio

PortAudio audio backend for cinder.

### Enabling Windows ASIO backend

For ASIO support, download ASIO Sdk from the Steinberg website [here](https://www.steinberg.net/en/company/developers.html), unzip and move to a folder at `lib/ASIOSDK`.

Then make sure that the following preprocessor macro is defined in our project:

```
PA_USE_ASIO
```

Lastly, include the following sources in your project:

```
portaudio/src/hostapi/*
```

### Debugging

To turn on portaudio debug logging functionality, define the following preprocessor macros:

```
PA_LOG_API_CALLS=1
PA_ENABLE_DEBUG_OUTPUT
PA_ENABLE_MSVC_DEBUG_OUTPUT
```

The latter is obviously only useful on Windows.
