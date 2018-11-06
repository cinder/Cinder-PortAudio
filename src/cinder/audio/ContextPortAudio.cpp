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

#define LOG_XRUN( stream )	CI_LOG_W( stream )
//#define LOG_XRUN( stream )	    ( (void)( 0 ) )

#define LOG_CI_PORTAUDIO( stream )	CI_LOG_I( stream )
//#define LOG_CI_PORTAUDIO( stream )	    ( (void)( 0 ) )

//#define LOG_CAPTURE( stream )	CI_LOG_I( stream )
#define LOG_CAPTURE( stream )	    ( (void)( 0 ) )

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
		const float *in = (const float *)inputBuffer; // needed in the case of full duplex I/O

		LOG_CAPTURE( "framesPerBuffer: " << framesPerBuffer << ", statusFlags: " << statusFlags << hex << ", input buffer: " << inputBuffer << ", outputBuffer: " << outputBuffer << dec );		
		parent->renderAudio( in, out, (size_t)framesPerBuffer );

		return paContinue;
	}

	PaStream *mStream = nullptr;
	OutputDeviceNodePortAudio*	mParent;
};

// ----------------------------------------------------------------------------------------------------
// OutputDeviceNodePortAudio
// ----------------------------------------------------------------------------------------------------

OutputDeviceNodePortAudio::OutputDeviceNodePortAudio( const DeviceRef &device, const Format &format )
	: OutputDeviceNode( device, format ), mImpl( new Impl( this ) ), mFullDuplexIO( false ), mFullDuplexInputDeviceNode( nullptr )
{
	LOG_CI_PORTAUDIO( "device key: " << device->getKey() );
	LOG_CI_PORTAUDIO( "device channels: " << device->getNumOutputChannels() << ", samplerate: " << device->getSampleRate() << ", framesPerBlock: " << device->getFramesPerBlock() );
}

OutputDeviceNodePortAudio::~OutputDeviceNodePortAudio()
{
}

void OutputDeviceNodePortAudio::initialize()
{
	LOG_CI_PORTAUDIO( "bang" );

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

	// check if any current device nodes are an input device node and have this same device
	auto ctx = dynamic_pointer_cast<ContextPortAudio>( getContext() );
	for( const auto &weakNode : ctx->mDeviceNodes ) {
		auto node = weakNode.lock();

	auto inputDeviceNode = dynamic_pointer_cast<InputDeviceNodePortAudio>( node );
	if( inputDeviceNode ) {
			LOG_CI_PORTAUDIO( "\t- found input device node" );

			if( getDevice() == inputDeviceNode->getDevice() ) {
				mFullDuplexIO = true;
				mFullDuplexInputDeviceNode = inputDeviceNode.get();
				break;
			}
		}
	}

	// if full duplex I/O, this output node's stream will be used instead of the input node
	PaStreamFlags streamFlags = 0;
	if( mFullDuplexIO ) {
		LOG_CI_PORTAUDIO( "\t- opening full duplex stream" );
		mFullDuplexIO = true;
		PaStreamParameters inputParams;
		inputParams.device = devIndex;
		inputParams.channelCount = getNumChannels();
		inputParams.sampleFormat = paFloat32;
		inputParams.hostApiSpecificStreamInfo = NULL;
		inputParams.suggestedLatency = getDevice()->getFramesPerBlock() / sampleRate;

		PaError err = Pa_OpenStream( &mImpl->mStream, &inputParams, &outputParams, sampleRate, framesPerBlock, streamFlags, &Impl::streamCallback, this );
		if( err != paNoError ) {
			throw ContextPortAudioExc( "Failed to open stream for output device named '" + getDevice()->getName() + " (full duplex)", err );
		}
	}
	else {
		LOG_CI_PORTAUDIO( "\t- opening half duplex stream" );
		PaError err = Pa_OpenStream( &mImpl->mStream, nullptr, &outputParams, sampleRate, framesPerBlock, streamFlags, &Impl::streamCallback, this );
		if( err != paNoError ) {
			throw ContextPortAudioExc( "Failed to open stream for output device named '" + getDevice()->getName() + " (half duplex)", err );
		}
	}
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

void OutputDeviceNodePortAudio::renderAudio( const float *inputBuffer, float *outputBuffer, size_t framesPerBuffer )
{
	auto ctx = getContext();
	if( ! ctx )
		return;

	lock_guard<mutex> lock( ctx->getMutex() );

	// verify context still exists, since its destructor may have been holding the lock
	ctx = getContext();
	if( ! ctx )
		return;

	CI_ASSERT( framesPerBuffer == getOutputFramesPerBlock() ); // currently expecting these to always match

	ctx->preProcess();

	if( mFullDuplexInputDeviceNode ) {
		mFullDuplexInputDeviceNode->mFullDuplexInputBuffer = inputBuffer;
	}

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

		if( mParent->mFullDuplexIO ) {
			// OutputDeviceNodePortAudio will provide the input buffer each frame, we don't need extra buffers or a stream.
			mReadBuffer = {};
			mRingBuffers.clear();
			return;
		}

		auto manager = dynamic_cast<DeviceManagePortAudio *>( Context::deviceManager() );
		auto device = mParent->getDevice();
		PaDeviceIndex devIndex = (PaDeviceIndex) manager->getPaDeviceIndex( device );
		size_t numChannels = mParent->getNumChannels();

		size_t framesPerBlock = mParent->getFramesPerBlock(); // frames per block for the audio graph / context
		size_t deviceFramesPerBlock = device->getFramesPerBlock(); // frames per block for the input device (might be different, if the samplerate is different)
		size_t deviceSampleRate = device->getSampleRate();

		if( deviceSampleRate != mParent->getSampleRate() ) {
			// samplerate doesn't match the context, install Converter
			mConverter = audio::dsp::Converter::create( deviceSampleRate, mParent->getSampleRate(), numChannels, numChannels, deviceFramesPerBlock );
			mConverterDestBuffer.setSize( mConverter->getDestMaxFramesPerBlock(), numChannels );
			mMaxReadFrames = deviceFramesPerBlock;

			LOG_CI_PORTAUDIO( "created converter, max source frames: " << mConverter->getSourceMaxFramesPerBlock() << ", max dest frames: " << mConverter->getDestMaxFramesPerBlock() );
		}
		else {
			mMaxReadFrames = framesPerBlock;
		}

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
		inputParams.suggestedLatency = (PaTime)framesPerBlock / (PaTime)deviceSampleRate;	

		PaStreamFlags flags = 0;
		PaError err = Pa_OpenStream( &mStream, &inputParams, nullptr, deviceSampleRate, framesPerBlock, flags, nullptr, nullptr );
		if( err != paNoError ) {
			throw ContextPortAudioExc( "Failed to open stream for input device named '" + device->getName(), err );
		}
	}

	void captureAudio( float *audioBuffer, size_t framesPerBuffer, size_t numChannels )
	{
		// Using Read/Write I/O Methods
		signed long readAvailable = Pa_GetStreamReadAvailable( mStream );
		CI_ASSERT( readAvailable >= 0 );
		LOG_CAPTURE( "[" << mParent->getContext()->getNumProcessedFrames() << "] read available: " << readAvailable << ", ring buffer write available: " << mRingBuffers[0].getAvailableWrite() );

		while( readAvailable > 0 ) {
			unsigned long framesToRead = min( (unsigned long)readAvailable, (unsigned long)mMaxReadFrames );
			mReadBuffer.setNumFrames( framesToRead );
			if( numChannels == 1 ) {
				// capture directly into mReadBuffer
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

			readAvailable = Pa_GetStreamReadAvailable( mStream );
			LOG_CAPTURE( "[" << mParent->getContext()->getNumProcessedFrames() << "] frames buffered: " << mNumFramesBuffered << ", read available: " << readAvailable << ", ring buffer write available: " << mRingBuffers[0].getAvailableWrite() );
			int flarg = 2;
		}
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
	: InputDeviceNode( device, format ), mImpl( new Impl( this ) ), mFullDuplexIO( false ), mFullDuplexInputBuffer( nullptr )
{
	LOG_CI_PORTAUDIO( "device key: " << device->getKey() );
	LOG_CI_PORTAUDIO( "device channels: " << device->getNumInputChannels() << ", samplerate: " << device->getSampleRate() << ", framesPerBlock: " << device->getFramesPerBlock() );
}

InputDeviceNodePortAudio::~InputDeviceNodePortAudio()
{
	LOG_CI_PORTAUDIO( "bang" );
}

void InputDeviceNodePortAudio::initialize()
{
	LOG_CI_PORTAUDIO( "bang" );

	// compare device to context output device, if they are the same key then we know we need to setup duplex audio
	auto outputDeviceNode = dynamic_pointer_cast<OutputDeviceNodePortAudio>( getContext()->getOutput() );
	if( getDevice() == outputDeviceNode->getDevice() ) {
		LOG_CI_PORTAUDIO( "\t- setting up duplex audio.." );

		mFullDuplexIO = true;
		if( ! outputDeviceNode->mFullDuplexIO ) {
			LOG_CI_PORTAUDIO( "\t- OutputDeviceNode needs re-initializing." );

			outputDeviceNode->mFullDuplexIO = true;

			bool outputWasInitialized = outputDeviceNode->isInitialized();
			bool outputWasEnabled = outputDeviceNode->isEnabled();
			if( outputWasInitialized ) {
				outputDeviceNode->disable();
				outputDeviceNode->uninitialize();
			}

			if( outputWasInitialized )
				outputDeviceNode->initialize();

			if( outputWasEnabled )
				outputDeviceNode->enable();
		}
	}
	else {
		mFullDuplexIO = false;
		mFullDuplexInputBuffer = nullptr;
	}

	mImpl->init();
}

void InputDeviceNodePortAudio::uninitialize()
{
	LOG_CI_PORTAUDIO( "bang" );
	if( mImpl->mStream ) {
		PaError err = Pa_CloseStream( mImpl->mStream );
		CI_VERIFY( err == paNoError );
	}

	mImpl->mStream = nullptr;
	mImpl->mConverter = nullptr;
}

void InputDeviceNodePortAudio::enableProcessing()
{
	LOG_CI_PORTAUDIO( "bang" );
	if( mImpl->mStream ) {
		PaError err = Pa_StartStream( mImpl->mStream );
		CI_VERIFY( err == paNoError );
	}
}

void InputDeviceNodePortAudio::disableProcessing()
{
	LOG_CI_PORTAUDIO( "bang" );
	if( mImpl->mStream ) {
		PaError err = Pa_StopStream( mImpl->mStream );
		CI_VERIFY( err == paNoError );
	}
}

void InputDeviceNodePortAudio::process( Buffer *buffer )
{
	if( mFullDuplexIO ) {
		// read from the buffer provided by OutputDeviceNodePortAudio
		LOG_CAPTURE( "copying duplex buffer " );
		CI_ASSERT( mFullDuplexInputBuffer );
		
		dsp::deinterleave( mFullDuplexInputBuffer, buffer->getData(), buffer->getNumFrames(), buffer->getNumChannels(), buffer->getNumFrames() );
	}
	else {
		// read from ring buffer
		const size_t framesNeeded = buffer->getNumFrames();

		LOG_CAPTURE( "[" << getContext()->getNumProcessedFrames() << "] audio thread: " << getContext()->isAudioThread() << ",  frames buffered: " << mImpl->mNumFramesBuffered << ", frames needed: " << framesNeeded );

		mImpl->captureAudio( buffer->getData(), framesNeeded, buffer->getNumChannels() );

		if( mImpl->mNumFramesBuffered < framesNeeded ) {
			// only mark underrun once audio capture has begun
			if( mImpl->mTotalFramesCaptured >= framesNeeded ) {
				LOG_XRUN( "[" << getContext()->getNumProcessedFrames() << "] buffer underrun. total frames buffered: " << mImpl->mNumFramesBuffered << ", less than frames needed: " << framesNeeded << ", total captured: " << mImpl->mTotalFramesCaptured );
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
	LOG_CI_PORTAUDIO( "created OutputDeviceNodePortAudio for device named '" << device->getName() << "'" );
	return result;
}

InputDeviceNodeRef ContextPortAudio::createInputDeviceNode( const DeviceRef &device, const Node::Format &format )
{
	auto result = makeNode( new InputDeviceNodePortAudio( device, format ) );
	mDeviceNodes.push_back( result );
	LOG_CI_PORTAUDIO( "created InputDeviceNodePortAudio for device named '" << device->getName() << "'" );
	return result;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - ContextPortAudioExc
// ----------------------------------------------------------------------------------------------------

ContextPortAudioExc::ContextPortAudioExc( const std::string &description )
	: AudioExc( description )
{
}

ContextPortAudioExc::ContextPortAudioExc( const std::string &description, int err )
	: AudioExc( "", err ) 
{
	stringstream ss;
	ss << description << " (PaError: " << err << ", '" << Pa_GetErrorText( err ) << "')";
	setDescription( ss.str() );
}

} } // namespace cinder::audio
