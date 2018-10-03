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
#include "cinder/audio/dsp/RingBuffer.h"
#include "cinder/Log.h"

#include "portaudio.h"

#define LOG_XRUN( stream )	CI_LOG_I( stream )
//#define LOG_XRUN( stream )	    ( (void)( 0 ) )

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

void OutputDeviceNodePortAudio::initialize()
{
	auto manager = dynamic_cast<DeviceManagePortAudio *>( Context::deviceManager() );

	PaDeviceIndex devIndex = (PaDeviceIndex) manager->getPaDeviceIndex( getDevice() );
	const PaDeviceInfo *devInfo = Pa_GetDeviceInfo( devIndex );

	// Open an audio I/O stream.
	PaStreamParameters outputParams;
	outputParams.device = devIndex;
	outputParams.channelCount = getNumChannels();
	//outputParams.sampleFormat = paFloat32 | paNonInterleaved; // TODO: try non-interleaved
	outputParams.sampleFormat = paFloat32;
	outputParams.hostApiSpecificStreamInfo = NULL;

	size_t framesPerBlock = getOutputFramesPerBlock();
	double sampleRate = getOutputSampleRate();
	outputParams.suggestedLatency = getDevice()->getFramesPerBlock() / sampleRate;	

	PaStreamFlags flags = 0;
	PaError err = Pa_OpenStream( &mImpl->mStream, nullptr, &outputParams, sampleRate, framesPerBlock, flags, &Impl::streamCallback, this );
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
// InputDeviceNodePortAudio::Impl
// ----------------------------------------------------------------------------------------------------

struct InputDeviceNodePortAudio::Impl {
	Impl( InputDeviceNodePortAudio *parent )
		: mParent( parent )
	{}

	void init( size_t framesPerBlock, size_t numChannels )
	{
		const size_t RINGBUFFER_PADDING_FACTOR = 2;

		size_t numBufferFrames = framesPerBlock * RINGBUFFER_PADDING_FACTOR;

		mNumFramesBuffered = 0;

		// TODO: if using duplex i/o we shouldn't need ringbuffers
		for( size_t ch = 0; ch < numChannels; ch++ ) {
			mRingBuffers.emplace_back( numBufferFrames );
		}

		mReadBuffer.setSize( framesPerBlock, numChannels );
	}

	void captureAudio( const float *audioBuffer, size_t framesPerBuffer, size_t numChannels )
	{
		// deinterleave to read buffer
		mReadBuffer.setNumFrames( framesPerBuffer );
		if( numChannels == 1 ) {
			// TODO: remove this path if not needed once conversion is in
			memcpy( mReadBuffer.getData(), audioBuffer, framesPerBuffer * 4 );
		}
		else {
			dsp::deinterleave( (float *)audioBuffer, mReadBuffer.getData(), framesPerBuffer, numChannels, framesPerBuffer );
		}

		// write to ring buffer
		// TODO: use Converter if installed (see Wasapi's captureAudio())
		for( size_t ch = 0; ch < numChannels; ch++ ) {
			if( ! mRingBuffers[ch].write( mReadBuffer.getChannel( ch ), framesPerBuffer ) ) {
				LOG_XRUN( "[" << mParent->getContext()->getNumProcessedFrames() << "] buffer overrun. failed to read from ringbuffer, num samples to write: " << framesPerBuffer << ", channel: " << ch );
				mParent->markOverrun();
			}
		}

		mNumFramesBuffered += framesPerBuffer;

	}

	//! This callback handles non-duplexed audio input
	static int streamCallback( const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData )
	{
		auto parent = (InputDeviceNodePortAudio *)userData;
		const float *in = (const float *)inputBuffer;

		parent->mImpl->captureAudio( in, (size_t)framesPerBuffer, parent->getNumChannels() );	

		// TODO: see docs on Pa_OpenStream()'s return value for what to return here when there is either an
		// interruption or the appliction quits.
		return paContinue;
	}

	PaStream *mStream = nullptr;
	InputDeviceNodePortAudio*	mParent;

	//std::unique_ptr<dsp::Converter>		mConverter; // TODO: use this when samplerate differs from context
	vector<dsp::RingBufferT<float>>		mRingBuffers; // storage for captured samples
	BufferDynamic						mReadBuffer/*, mConvertedReadBuffer*/;
	size_t								mNumFramesBuffered;
};

// ----------------------------------------------------------------------------------------------------
// InputDeviceNodePortAudio
// ----------------------------------------------------------------------------------------------------

InputDeviceNodePortAudio::InputDeviceNodePortAudio( const DeviceRef &device, const Format &format )
	: InputDeviceNode( device, format ), mImpl( new Impl( this ) )
{
	CI_LOG_I( "device key: " << device->getKey() );
	CI_LOG_I( "device channels: " << device->getNumInputChannels() << ", samplerate: " << device->getSampleRate() << ", framesPerBlock: " << device->getFramesPerBlock() );
}

InputDeviceNodePortAudio::~InputDeviceNodePortAudio()
{
}

void InputDeviceNodePortAudio::initialize()
{
	auto manager = dynamic_cast<DeviceManagePortAudio *>( Context::deviceManager() );

	PaDeviceIndex devIndex = (PaDeviceIndex) manager->getPaDeviceIndex( getDevice() );
	const PaDeviceInfo *devInfo = Pa_GetDeviceInfo( devIndex );
	size_t numChannels = getNumChannels();

	// Open an audio I/O stream.
	PaStreamParameters inputParams;
	inputParams.device = devIndex;
	inputParams.channelCount = numChannels;
	inputParams.sampleFormat = paFloat32;
	inputParams.hostApiSpecificStreamInfo = NULL;

	size_t framesPerBlock = getFramesPerBlock();
	double sampleRate = getSampleRate(); // TODO: use the samplerate of the Device, if it doesn't match the context's samplerate then install a Converter
	inputParams.suggestedLatency = getDevice()->getFramesPerBlock() / sampleRate;	

	PaStreamFlags flags = 0;
	PaError err = Pa_OpenStream( &mImpl->mStream, &inputParams, nullptr, sampleRate, framesPerBlock, flags, &Impl::streamCallback, this );
	CI_ASSERT( err == paNoError );

	mImpl->init( framesPerBlock, numChannels );
}

void InputDeviceNodePortAudio::uninitialize()
{
	PaError err = Pa_CloseStream( mImpl->mStream );
	CI_ASSERT( err == paNoError );

	mImpl->mStream = nullptr;
}

void InputDeviceNodePortAudio::enableProcessing()
{
	PaError err = Pa_StartStream( mImpl->mStream );
	CI_ASSERT( err == paNoError );
}

void InputDeviceNodePortAudio::disableProcessing()
{
	PaError err = Pa_StopStream( mImpl->mStream );
	CI_ASSERT( err == paNoError );
}

void InputDeviceNodePortAudio::process( Buffer *buffer )
{
	// TODO: when duplex, don't need to use mNumFramesBuffered or RingBuffers - just copy read buffer over

	// read from ring buffer
	const size_t framesNeeded = buffer->getNumFrames();
	if( mImpl->mNumFramesBuffered < framesNeeded ) {
		LOG_XRUN( "[" << getContext()->getNumProcessedFrames() << "] buffer underrun. failed to read from ringbuffer, mCaptureImpl->mNumFramesBuffered: " << mImpl->mNumFramesBuffered << ", framesNeeded: " << framesNeeded );
		markUnderrun();
		return;
	}

	for( size_t ch = 0; ch < getNumChannels(); ch++ ) {
		bool readSuccess = mImpl->mRingBuffers[ch].read( buffer->getChannel( ch ), framesNeeded );
		CI_VERIFY( readSuccess );
	}

	mImpl->mNumFramesBuffered -= framesNeeded;
}

// ----------------------------------------------------------------------------------------------------
// ContextPortAudio
// ----------------------------------------------------------------------------------------------------

// static
void ContextPortAudio::setAsMaster()
{
	Context::setMaster( new ContextPortAudio, new DeviceManagePortAudio );
}

ContextPortAudio::ContextPortAudio()
{
	PaError err = Pa_Initialize();
	CI_ASSERT( err == paNoError );
}

ContextPortAudio::~ContextPortAudio()
{
	// uninit any device nodes before the portaudio context is destroyed
	for( auto& deviceNode : mDeviceNodes ) {
		auto node = deviceNode.lock();
		if( node ) {
			node->disable();
			uninitializeNode( node );
		}
	}

	PaError err = Pa_Terminate();
	CI_ASSERT( err == paNoError );
}

OutputDeviceNodeRef	ContextPortAudio::createOutputDeviceNode( const DeviceRef &device, const Node::Format &format )
{
	auto result = makeNode( new OutputDeviceNodePortAudio( device, format ) );
	mDeviceNodes.push_back( result );
	return result;
}

InputDeviceNodeRef ContextPortAudio::createInputDeviceNode( const DeviceRef &device, const Node::Format &format )
{
	auto result = makeNode( new InputDeviceNodePortAudio( device, format ) );
	mDeviceNodes.push_back( result );
	return result;
}

} } // namespace cinder::audio
