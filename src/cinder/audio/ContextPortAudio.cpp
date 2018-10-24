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

#define LOG_CAPTURE( stream )	CI_LOG_I( stream )
//#define LOG_CAPTURE( stream )	    ( (void)( 0 ) )

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
	const size_t RINGBUFFER_PADDING_FACTOR = 4;

	Impl( InputDeviceNodePortAudio *parent )
		: mParent( parent )
	{}

	void init()
	{
		mNumFramesBuffered = 0;
		mTotalFramesCaptured = 0;

		auto device = mParent->getDevice();
		auto manager = dynamic_cast<DeviceManagePortAudio *>( Context::deviceManager() );

		PaDeviceIndex devIndex = (PaDeviceIndex) manager->getPaDeviceIndex( device );
		//const PaDeviceInfo *devInfo = Pa_GetDeviceInfo( devIndex );
		size_t numChannels = mParent->getNumChannels();

		size_t framesPerBlock = mParent->getFramesPerBlock(); // frames per block for the audio graph / context
		size_t deviceFramesPerBlock = device->getFramesPerBlock(); // frames per block for the input device (might be different, if the samplerate is different)
		double deviceSampleRate = device->getSampleRate();

		if( deviceSampleRate != mParent->getSampleRate() ) {
			// samplerate doesn't match the context, install Converter
			mConverter = audio::dsp::Converter::create( deviceSampleRate, mParent->getSampleRate(), numChannels, numChannels, deviceFramesPerBlock );
			mConverterDestBuffer.setSize( mConverter->getDestMaxFramesPerBlock(), numChannels );
			mMaxReadFrames = deviceFramesPerBlock;
		}
		else {
			mMaxReadFrames = framesPerBlock;
		}

		// TODO: if using duplex i/o we shouldn't need ringbuffers
		for( size_t ch = 0; ch < numChannels; ch++ ) {
			mRingBuffers.emplace_back( framesPerBlock * RINGBUFFER_PADDING_FACTOR );
		}

		mReadBuffer.setSize( deviceFramesPerBlock, numChannels );

		// Open an audio I/O stream. No callbacks, we'll get pulled from the audio graph and read non-blocking
		PaStreamParameters inputParams;
		inputParams.device = devIndex;
		inputParams.channelCount = numChannels;
		inputParams.sampleFormat = paFloat32;
		inputParams.hostApiSpecificStreamInfo = NULL;

		inputParams.suggestedLatency = framesPerBlock / deviceSampleRate;	

		PaStreamFlags flags = 0;
		PaError err = Pa_OpenStream( &mStream, &inputParams, nullptr, deviceSampleRate, framesPerBlock, flags, nullptr, nullptr );
		CI_VERIFY( err == paNoError );
	}

	void captureAudio( float *audioBuffer, size_t framesPerBuffer, size_t numChannels )
	{
		// Using Read/Write I/O Methods
		signed long readAvailable = Pa_GetStreamReadAvailable( mStream );
		CI_ASSERT( readAvailable >= 0 );
		LOG_CAPTURE( "[" << mParent->getContext()->getNumProcessedFrames() << "] read available: " << readAvailable << ", ring buffer write available: " << mRingBuffers[0].getAvailableWrite() );
		if( readAvailable <= 0 )
			return;

		unsigned long framesToRead = min( (unsigned long)readAvailable, (unsigned long)mMaxReadFrames );
		mReadBuffer.setNumFrames( framesToRead );
		if( numChannels == 1 ) {
			// capture directly into mReadBuffer
			// TODO: remove this path if not needed once conversion is in
			PaError err = Pa_ReadStream( mStream, mReadBuffer.getData(), framesToRead );
			CI_VERIFY( err == paNoError );
		}
		else {
			// read into audioBuffer, then de-interleave into mReadBuffer
			// TODO: does it make sense to pass in the audioBuffer here?
			// - might also be too small when downsampling (ex. input: 48k, output: 44.1k)
			PaError err = Pa_ReadStream( mStream, audioBuffer, framesToRead );
			CI_VERIFY( err == paNoError );
			dsp::deinterleave( (float *)audioBuffer, mReadBuffer.getData(), framesPerBuffer, numChannels, framesPerBuffer );
		}

		// write to ring buffer, use Converter if one was installed
		if( mConverter ) {
			pair<size_t, size_t> count = mConverter->convert( &mReadBuffer, &mConverterDestBuffer );
			LOG_CAPTURE( "\t- frames read: " << framesToRead << ", converted: " << count.second );
			for( size_t ch = 0; ch < numChannels; ch++ ) {
				if( ! mRingBuffers[ch].write( mConverterDestBuffer.getChannel( ch ), count.second ) ) {
					LOG_XRUN( "[" << mParent->getContext()->getNumProcessedFrames() << "] buffer overrun (with converter). failed to read from ringbuffer,  num samples to write: " << count.second << ", channel: " << ch );
					mParent->markOverrun();
				}
			}

			mNumFramesBuffered += count.second;
			mTotalFramesCaptured += count.second;
		}
		else {
			LOG_CAPTURE( "\t- frames read: " << framesToRead );
			for( size_t ch = 0; ch < numChannels; ch++ ) {
				if( ! mRingBuffers[ch].write( mReadBuffer.getChannel( ch ), framesToRead ) ) {
					LOG_XRUN( "[" << mParent->getContext()->getNumProcessedFrames() << "] buffer overrun. failed to read from ringbuffer, num samples to write: " << framesToRead << ", channel: " << ch );
					mParent->markOverrun();
					return;
				}
			}
			mNumFramesBuffered += framesToRead;
			mTotalFramesCaptured += framesToRead;
		}

		LOG_CAPTURE( "[" << mParent->getContext()->getNumProcessedFrames() << "] frames buffered: " << mNumFramesBuffered << ", read available: " << readAvailable << ", ring buffer write available: " << mRingBuffers[0].getAvailableWrite() );
		int blarg = 2;
	}

	PaStream *mStream = nullptr;
	InputDeviceNodePortAudio*	mParent;

	std::unique_ptr<dsp::Converter>		mConverter;
	vector<dsp::RingBufferT<float>>		mRingBuffers; // storage for samples ready for consumption in the audio graph
	BufferDynamic						mReadBuffer, mConverterDestBuffer;
	size_t								mNumFramesBuffered;
	size_t								mMaxReadFrames;
	uint64_t							mTotalFramesCaptured = 0;
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
	mImpl->init();
}

void InputDeviceNodePortAudio::uninitialize()
{
	PaError err = Pa_CloseStream( mImpl->mStream );
	CI_ASSERT( err == paNoError );

	mImpl->mStream = nullptr;
	mImpl->mConverter = nullptr;
}

void InputDeviceNodePortAudio::enableProcessing()
{
	PaError err = Pa_StartStream( mImpl->mStream );
	CI_ASSERT( err == paNoError );
}

void InputDeviceNodePortAudio::disableProcessing()
{
	LOG_CAPTURE( "bang" );

	PaError err = Pa_StopStream( mImpl->mStream );
	CI_ASSERT( err == paNoError );

	LOG_CAPTURE( "complete" );
}

// TODO: when duplex, don't need to use mNumFramesBuffered or RingBuffers - just copy read buffer over
void InputDeviceNodePortAudio::process( Buffer *buffer )
{
	// read from ring buffer
	const size_t framesNeeded = buffer->getNumFrames();
	LOG_CAPTURE( "[" << getContext()->getNumProcessedFrames() << "] audio thread: " << getContext()->isAudioThread() << ",  frames buffered: " << mImpl->mNumFramesBuffered << ", frames needed: " << framesNeeded );

	// TODO: might have to do this is a for loop to get as many as we can
	mImpl->captureAudio( buffer->getData(), framesNeeded, buffer->getNumChannels() );

	if( mImpl->mNumFramesBuffered < framesNeeded ) {
		// only mark underrun once audio capture has begun
		if( mImpl->mTotalFramesCaptured >= framesNeeded ) {
			LOG_XRUN( "[" << getContext()->getNumProcessedFrames() << "] buffer underrun. failed to read from ringbuffer, framesNeeded: " << framesNeeded << ", frames buffered: " << mImpl->mNumFramesBuffered << ", total captured: " << mImpl->mTotalFramesCaptured );
			markUnderrun();
		}
		return;
	}

	for( size_t ch = 0; ch < getNumChannels(); ch++ ) {
		bool readSuccess = mImpl->mRingBuffers[ch].read( buffer->getChannel( ch ), framesNeeded );
		//CI_VERIFY( readSuccess );
		if( ! readSuccess ) {
			LOG_XRUN( "[" << getContext()->getNumProcessedFrames() << "] buffer underrun. failed to read from ringbuffer, framesNeeded: " << framesNeeded << ", frames buffered: " << mImpl->mNumFramesBuffered << ", total captured: " << mImpl->mTotalFramesCaptured );
			markUnderrun();
		}
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
