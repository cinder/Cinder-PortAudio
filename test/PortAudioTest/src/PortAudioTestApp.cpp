#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/Rand.h"
#include "cinder/audio/audio.h"

#include "cinder/audio/ContextPortAudio.h"
#include "cinder/audio/DeviceManagerPortAudio.h"

#include "../../../../../samples/_audio/common/AudioDrawUtils.h"

#include "portaudio.h"

extern int paex_saw_main();

using namespace ci;
using namespace ci::app;
using namespace std;

class PortAudioTestApp : public App {
  public:
	void setup() override;
	void update() override;
	void draw() override;

	void keyDown( KeyEvent event ) override;
	void mouseDown( MouseEvent event ) override;

	void printPaInfo();
	void printContextInfo();
	void testOpenStream();
	void testSimple();
	void testMultichannel();
	void testInputOutput();
	void rampGain();
	void shiftRouteChannel();

	PaStream *mStream = nullptr;

	audio::GenNodeRef				mGen;
	audio::GainNodeRef				mGain;
	audio::MonitorNodeRef			mMonitor;
	audio::ChannelRouterNodeRef		mChannelRouterNode;
	size_t mCurrentChannel = 0;
	float  mVolume = 0.4f;
};

void PortAudioTestApp::setup()
{
	printPaInfo();
	//paex_saw_main();	
	//testOpenStream();

	// make ContextPortAudio the master context, overriding cinder's default
	audio::ContextPortAudio::setAsMaster();

	//testSimple();
	//testMultichannel();
	testInputOutput();
}

void PortAudioTestApp::printPaInfo()
{
	PaError err = Pa_Initialize();
	CI_ASSERT( err == paNoError );

	PaHostApiIndex numHosts = Pa_GetHostApiCount();
	PaHostApiIndex defaultHostIndex = Pa_GetDefaultHostApi();

	auto defaultHost = Pa_GetHostApiInfo( defaultHostIndex );

	CI_LOG_I( "numHosts: " << numHosts << ", default name: " << defaultHost->name );
	for( PaHostApiIndex i = 0; i < numHosts; i++ ) {
		auto host = Pa_GetHostApiInfo( i );
		CI_LOG_I( "host index: " << i << ", PaHostApiTypeId: " << host->type << ", name: " << host->name << ", device count: " << host->deviceCount );
	}

	PaDeviceIndex numDevices = Pa_GetDeviceCount();
	CI_LOG_I( "numDevices: " << numDevices );
	for( PaDeviceIndex i = 0; i < numDevices; i++ ) {
		auto dev = Pa_GetDeviceInfo( i );
		CI_LOG_I( "device index: " << i << ", host api: " << dev->hostApi << ", name: " << dev->name << ", input channels: " << dev->maxInputChannels
			<< ", output channels: " << dev->maxOutputChannels << ", samplerate: " << dev->defaultSampleRate
			<< ", latency (low): [" << dev->defaultLowInputLatency << ", " << dev->defaultLowOutputLatency << "]"
			<< ", latency (high): [" << dev->defaultHighInputLatency << ", " << dev->defaultHighOutputLatency << "]" );
	}

	CI_LOG_I( "default input: " << Pa_GetDefaultInputDevice() << ", default output: " << Pa_GetDefaultOutputDevice() );

	Pa_Terminate();
}

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

void PortAudioTestApp::testOpenStream()
{
	PaDeviceIndex realtekWasapiIndex = 2;
	const unsigned long framesPerBuffer = 256;

	PaError err = Pa_Initialize();
	CI_ASSERT( err == paNoError );

	auto devInfo = Pa_GetDeviceInfo( realtekWasapiIndex );

	// Open an audio I/O stream.
	PaStreamParameters outputParams;
	outputParams.device = realtekWasapiIndex;
	outputParams.channelCount = devInfo->maxOutputChannels;
	outputParams.sampleFormat = paFloat32;
	outputParams.suggestedLatency = devInfo->defaultLowInputLatency;
	outputParams.hostApiSpecificStreamInfo = NULL;



	PaStreamFlags flags = 0;
	//PaError err = Pa_OpenDefaultStream( &stream, 0, 2, paFloat32, 44100, 256, patestCallback, &data );
	err = Pa_OpenStream( &mStream, nullptr, &outputParams, devInfo->defaultSampleRate, framesPerBuffer, flags, noiseCallback, this );
	CI_ASSERT( err == paNoError );

	err = Pa_StartStream( mStream );
	CI_ASSERT( err == paNoError );
}

void PortAudioTestApp::testSimple()
{
	// sinewave -> gain (stereo) -> out
	auto genNode = audio::master()->makeNode<audio::GenSineNode>( 440.0f );
	auto gainNode = audio::master()->makeNode<audio::GainNode>( 0.5f, audio::Node::Format().channels( 2 ) );

	genNode >> gainNode >> audio::master()->getOutput();

	genNode->enable();
	audio::master()->enable();
}

void PortAudioTestApp::testMultichannel()
{
	auto ctx = audio::Context::master();
	
	//auto dev = audio::Device::getDefaultOutput();
	//auto dev = audio::Device::findDeviceByName( "Focusrite USB ASIO" );
	//auto dev = audio::Device::findDeviceByName( "Realtek ASIO" ); // note: currently fails in asio code (ASIOCreateBuffers() returns -997, ASE_InvalidMode)
	//auto dev = audio::Device::findDeviceByName( "Focusrite USB (Focusrite USB Audio)" );
	auto dev = audio::Device::findDeviceByName( "Speakers (Realtek High Definition Audio)" );

	if( ! dev ) {
		CI_LOG_E( "no device selected." );
		return;
	}

	//dev->updateFormat( audio::Device::Format().framesPerBlock( 64 ) ); // test low latency frames per block

	auto outputNode = ctx->createOutputDeviceNode( dev, audio::Node::Format().channels( dev->getNumOutputChannels() ) );
	ctx->setOutput( outputNode );

	mGen = ctx->makeNode( new audio::GenSineNode( 440, audio::Node::Format().autoEnable() ) );
	mGain = ctx->makeNode( new audio::GainNode( 0 ) );

	mChannelRouterNode = ctx->makeNode( new audio::ChannelRouterNode( audio::Node::Format().channels( ctx->getOutput()->getNumChannels() ) ) );

	mMonitor = ctx->makeNode( new audio::MonitorNode );

	mGen >> mGain >> mChannelRouterNode->route( 0, mCurrentChannel ) >> mMonitor >> ctx->getOutput();

	ctx->enable();
}

void PortAudioTestApp::testInputOutput()
{
	// setup simple input -> gain -> monitor -> output graph

	auto ctx = audio::Context::master();

	auto outputDev = audio::Device::getDefaultOutput();
	//auto outputDev = audio::Device::findDeviceByName( "Speakers (Realtek High Definition Audio)" );
	auto outputNode = ctx->createOutputDeviceNode( outputDev );
	ctx->setOutput( outputNode );

	auto inputDev = audio::Device::getDefaultInput();
	//auto inputDev = audio::Device::findDeviceByName( "Microphone (HD Webcam C615)" );
	auto inputNode = ctx->createInputDeviceNode( inputDev );

	mGain = ctx->makeNode( new audio::GainNode( mVolume ) );

	mMonitor = ctx->makeNode( new audio::MonitorNode );

	inputNode >> mGain >> mMonitor >> ctx->getOutput();

	inputNode->enable();
	ctx->enable();
}

void PortAudioTestApp::rampGain()
{
	mGain->getParam()->applyRamp( 0, mVolume, 0.5f );
	mGain->getParam()->appendRamp( 0, 0.5f );
}

void PortAudioTestApp::shiftRouteChannel()
{
	mCurrentChannel = ( mCurrentChannel + 1 ) % mChannelRouterNode->getNumChannels();

	mChannelRouterNode->disconnectAllInputs();
	mGain->disconnectAllOutputs();

	mGain >> mChannelRouterNode->route( 0, mCurrentChannel );
}

void PortAudioTestApp::printContextInfo()
{
	stringstream str;
	auto ctx = ci::audio::master();

	str << "\n-------------- Context info --------------\n";
	str << "enabled: " << boolalpha << ctx->isEnabled() << ", samplerate: " << ctx->getSampleRate() << ", frames per block: " << ctx->getFramesPerBlock() << endl;

	str << "-------------- Graph configuration: --------------" << endl;
	str << ci::audio::master()->printGraphToString();
	str << "--------------------------------------------------" << endl;

	CI_LOG_I( str.str() );
}

void PortAudioTestApp::mouseDown( MouseEvent event )
{
#if 0
	if( ! mStream )
		return;

	CI_LOG_I( "closing stream.." );

	PaError err = Pa_CloseStream( mStream );
	CI_ASSERT( err == paNoError );

	mStream = nullptr;
#endif
}

void PortAudioTestApp::keyDown( KeyEvent event )
{
	if( event.getChar() == 'd' ) {
		CI_LOG_I( "devices:\n" << audio::Device::printDevicesToString() );
	}
	else if( event.getChar() == 'p' ) {
		printContextInfo();
	}
	else if( event.getCode() == KeyEvent::KEY_UP ) {
		mVolume += 0.1f;
		CI_LOG_I( "volume: " << mVolume );
		if( mGain ) {
			mGain->getParam()->applyRamp( mVolume, 0.3f );
		}
	}
	else if( event.getCode() == KeyEvent::KEY_DOWN ) {
		mVolume = max<float>( 0.0f, mVolume - 0.1f );
		CI_LOG_I( "volume: " << mVolume );
		if( mGain ) {
			mGain->getParam()->applyRamp( mVolume, 0.3f );
		}
	}
}

void PortAudioTestApp::update()
{
	if( mChannelRouterNode && mGain->getParam()->getNumEvents() == 0 ) {
		shiftRouteChannel();
		rampGain();
	}
}

void PortAudioTestApp::draw()
{
	gl::clear( Color( 0, 0.1f, 0.1f ) );

	// Draw the Scope's recorded Buffer, all channels as separate line arrays.
	if( mMonitor && mMonitor->isEnabled() ) {
		Rectf scopeRect( 10, 10, (float)getWindowWidth() - 10, (float)getWindowHeight() - 10 );
		drawAudioBuffer( mMonitor->getBuffer(), scopeRect, true );
	}
}

CINDER_APP( PortAudioTestApp, RendererGl )
