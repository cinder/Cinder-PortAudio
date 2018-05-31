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
#include "cinder/audio/DeviceManagerPortAudio.h"
#include "cinder/audio/dsp/Converter.h"
#include "cinder/Log.h"
#include "cinder/Rand.h" // TODO: remove

#include "portaudio.h"

using namespace std;
using namespace ci;

namespace cinder { namespace audio {

// ----------------------------------------------------------------------------------------------------
// OutputDeviceNodePortAudio::Impl
// ----------------------------------------------------------------------------------------------------

struct OutputDeviceNodePortAudio::Impl {
	Impl( OutputDeviceNodePortAudio *parent )
		: mParent( parent )
	{}

	static int streamCallback( const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData )
	{
		auto parent = (OutputDeviceNodePortAudio *)userData;

		float *out = (float*)outputBuffer;
		//const float *in = (const float *)inputBuffer; // TODO: for duplex input this will need to be sent to renderAudio() also
		
		parent->renderAudio( out, (size_t)framesPerBuffer );

		//const float vol = 0.1f;
		//for( int i = 0; i < framesPerBuffer; i++ ) {
		//	*out++ = ci::randFloat( -vol, vol ); // left
		//	*out++ = ci::randFloat( -vol, vol ); // right
		//}

		return paContinue;
	}

	PaStream *mStream = nullptr;
	OutputDeviceNodePortAudio*	mParent;
};


// ----------------------------------------------------------------------------------------------------
// OutputDeviceNodePortAudio
// ----------------------------------------------------------------------------------------------------

OutputDeviceNodePortAudio::OutputDeviceNodePortAudio( const DeviceRef &device, const Format &format )
	: OutputDeviceNode( device, format ), mImpl( new Impl( this ) )
{
	CI_LOG_I( "device key: " << device->getKey() );
	CI_LOG_I( "device channels: " << device->getNumOutputChannels() << ", samplerate: " << device->getSampleRate() << ", framesPerBlock: " << device->getFramesPerBlock() );
}

OutputDeviceNodePortAudio::~OutputDeviceNodePortAudio()
{
}

#if 0
static int noiseCallback( const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData )
{
	float *out = (float*)outputBuffer;
	const float *in = (const float *)inputBuffer;

	(void) timeInfo; /* Prevent unused variable warnings. */
	(void) statusFlags;
	(void) userData;

	const float vol = 0.1f;
	for( int i = 0; i < framesPerBuffer; i++ ) {
		*out++ = ci::randFloat( -vol, vol ); // left
		*out++ = ci::randFloat( -vol, vol ); // right
	}

	return paContinue;
}

#endif

void OutputDeviceNodePortAudio::initialize()
{
	auto manager = dynamic_cast<DeviceManagePortAudio *>( Context::deviceManager() );

	PaDeviceIndex devIndex = (PaDeviceIndex) manager->getPaDeviceIndex( getDevice() );
	const PaDeviceInfo *devInfo = Pa_GetDeviceInfo( devIndex );

	// Open an audio I/O stream.
	PaStreamParameters outputParams;
	outputParams.device = devIndex;
	outputParams.channelCount = getNumChannels();
	//outputParams.sampleFormat = paFloat32 | paNonInterleaved; // TODO: get non-interleaved working 
	outputParams.sampleFormat = paFloat32;
	outputParams.suggestedLatency = devInfo->defaultHighOutputLatency; // TODO: device how to 
	outputParams.hostApiSpecificStreamInfo = NULL;

	PaStreamFlags flags = 0;
	PaError err = Pa_OpenStream( &mImpl->mStream, nullptr, &outputParams, getOutputSampleRate(), getFramesPerBlock(), flags, &Impl::streamCallback, this );
	CI_ASSERT( err == paNoError );
}

void OutputDeviceNodePortAudio::uninitialize()
{
	PaError err = Pa_CloseStream( mImpl->mStream );
	CI_ASSERT( err == paNoError );

	mImpl->mStream = nullptr;
}

void OutputDeviceNodePortAudio::enableProcessing()
{
	PaError err = Pa_StartStream( mImpl->mStream );
	CI_ASSERT( err == paNoError );
}

void OutputDeviceNodePortAudio::disableProcessing()
{
	PaError err = Pa_StopStream( mImpl->mStream );
	CI_ASSERT( err == paNoError );
}

void OutputDeviceNodePortAudio::renderAudio( float *outputBuffer, size_t framesPerBuffer )
{
	auto ctx = getContext();
	if( ! ctx )
		return;

	lock_guard<mutex> lock( ctx->getMutex() );

	// verify context still exists, since its destructor may have been holding the lock
	ctx = getContext();
	if( ! ctx )
		return;

	ctx->preProcess();

	auto internalBuffer = getInternalBuffer();
	internalBuffer->zero();
	pullInputs( internalBuffer );

	if( checkNotClipping() )
		internalBuffer->zero();

	const size_t numFrames = internalBuffer->getNumFrames();
	const size_t numChannels = internalBuffer->getNumChannels();

	dsp::interleave( internalBuffer->getData(), outputBuffer, numFrames, numChannels, numFrames );

	ctx->postProcess();
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
