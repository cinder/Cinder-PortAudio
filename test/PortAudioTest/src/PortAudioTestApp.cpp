#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/Rand.h"
#include "cinder/audio/audio.h"

#include "cinder/audio/ContextPortAudio.h"
#include "cinder/audio/DeviceManagerPortAudio.h"

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
	void testContext();

	PaStream *mStream = nullptr;
};

void PortAudioTestApp::setup()
{
	printPaInfo();
	//paex_saw_main();	
	//testOpenStream();

	testContext();
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

void PortAudioTestApp::testContext()
{
	// make context / dev manager and set as master
	// TODO: document that the Context owns these
	audio::Context::setMaster( new audio::ContextPortAudio, new audio::DeviceManagePortAudio );

	auto genNode = audio::master()->makeNode<audio::GenSineNode>( 440 );
	auto gainNode = audio::master()->makeNode<audio::GainNode>( 0.5f );

	genNode >> gainNode >> audio::master()->getOutput();

	genNode->enable();
	audio::master()->enable();
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
}

void PortAudioTestApp::update()
{
}

void PortAudioTestApp::draw()
{
	gl::clear( Color( 0, 0.3f, 0 ) ); 
}

CINDER_APP( PortAudioTestApp, RendererGl )
