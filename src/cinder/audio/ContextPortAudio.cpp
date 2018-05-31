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

#include "cinder/audio/ContextPortAudio.h"
#include "cinder/Log.h"

#include "portaudio.h"

using namespace std;
using namespace ci;

namespace cinder { namespace audio {

// ----------------------------------------------------------------------------------------------------
// OutputDeviceNodePortAudio
// ----------------------------------------------------------------------------------------------------

OutputDeviceNodePortAudio::OutputDeviceNodePortAudio( const DeviceRef &device, const Format &format )
	: OutputDeviceNode( device, format )
{
	CI_LOG_I( "device key: " << device->getKey() );
	CI_LOG_I( "channels: " << device->getNumOutputChannels() << ", samplerate: " << device->getSampleRate() << ", framesPerBlock: " << device->getFramesPerBlock() );
}

OutputDeviceNodePortAudio::~OutputDeviceNodePortAudio()
{
	CI_LOG_I( "bang" );
}

void OutputDeviceNodePortAudio::initialize()
{
	CI_LOG_I( "bang" );
}

void OutputDeviceNodePortAudio::uninitialize()
{
	CI_LOG_I( "bang" );
}

void OutputDeviceNodePortAudio::enableProcessing()
{
	CI_LOG_I( "bang" );
}

void OutputDeviceNodePortAudio::disableProcessing()
{
	CI_LOG_I( "bang" );
}

// ----------------------------------------------------------------------------------------------------
// ContextPortAudio
// ----------------------------------------------------------------------------------------------------

ContextPortAudio::ContextPortAudio()
{
	PaError err = Pa_Initialize();
	CI_ASSERT( err == paNoError );
}

ContextPortAudio::~ContextPortAudio()
{
	PaError err = Pa_Terminate();
	CI_ASSERT( err == paNoError );
}

OutputDeviceNodeRef	ContextPortAudio::createOutputDeviceNode( const DeviceRef &device, const Node::Format &format )
{
	return makeNode( new OutputDeviceNodePortAudio( device, format ) );
}

InputDeviceNodeRef ContextPortAudio::createInputDeviceNode( const DeviceRef &device, const Node::Format &format )
{
	CI_LOG_E( "not yet implemented" );
	CI_ASSERT_NOT_REACHABLE();
	return {};
}

} } // namespace cinder::audio
