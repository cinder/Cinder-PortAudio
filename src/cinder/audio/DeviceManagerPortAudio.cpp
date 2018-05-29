/*
Copyright (c) 2018, The Cinder Project

This code is intended to be used with the Cinder C++ library, http://libcinder.org

Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and
the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "cinder/audio/DeviceManagerPortAudio.h"
#include "cinder/Log.h"

#include "portaudio.h"

namespace cinder { namespace audio {

DeviceManagePortAudio::DeviceManagePortAudio()
{
	PaError err = Pa_Initialize();
	CI_ASSERT( err == paNoError );
}

DeviceManagePortAudio::~DeviceManagePortAudio()
{
	PaError err = Pa_Terminate();
	CI_ASSERT( err == paNoError );
}

DeviceRef DeviceManagePortAudio::getDefaultOutput()
{
	CI_ASSERT_NOT_REACHABLE();
	return {};
}

DeviceRef DeviceManagePortAudio::getDefaultInput()
{
	CI_ASSERT_NOT_REACHABLE();
	return {};
}

const std::vector<DeviceRef>& DeviceManagePortAudio::getDevices()
{
	CI_ASSERT_NOT_REACHABLE();
	// TODO: build devices
	return mDevices;
}

std::string DeviceManagePortAudio::getName( const DeviceRef &device )
{
	CI_ASSERT_NOT_REACHABLE();
	return {};
}

size_t DeviceManagePortAudio::getNumInputChannels( const DeviceRef &device )
{
	CI_ASSERT_NOT_REACHABLE();
	return {};
}

size_t DeviceManagePortAudio::getNumOutputChannels( const DeviceRef &device )
{
	CI_ASSERT_NOT_REACHABLE();
	return {};
}

size_t DeviceManagePortAudio::getSampleRate( const DeviceRef &device )
{
	CI_ASSERT_NOT_REACHABLE();
	return {};
}

size_t DeviceManagePortAudio::getFramesPerBlock( const DeviceRef &device )
{
	CI_ASSERT_NOT_REACHABLE();
	return {};
}

void DeviceManagePortAudio::setSampleRate( const DeviceRef &device, size_t sampleRate )
{
	CI_ASSERT_NOT_REACHABLE();
}

void DeviceManagePortAudio::setFramesPerBlock( const DeviceRef &device, size_t framesPerBlock )
{
	CI_ASSERT_NOT_REACHABLE();
}

} } // namespace cinder::audio