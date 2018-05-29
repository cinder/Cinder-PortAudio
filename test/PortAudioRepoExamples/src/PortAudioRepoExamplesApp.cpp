#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"

#include "portaudio.h"

extern int paex_saw_main();

using namespace ci;
using namespace ci::app;
using namespace std;

class PortAudioRepoExamplesApp : public App {
  public:
	void setup() override;
	void printInfo();
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
};

void PortAudioRepoExamplesApp::setup()
{
	//paex_saw_main();

	printInfo();
}

void PortAudioRepoExamplesApp::printInfo()
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
			<< ", output channels" << dev->maxOutputChannels << ", samplerate: " << dev->defaultSampleRate
			<< ", latency (low): [" << dev->defaultLowInputLatency << ", " << dev->defaultLowOutputLatency << "]"
			<< ", latency (high): [" << dev->defaultHighInputLatency << ", " << dev->defaultHighOutputLatency << "]" );
	}
}

void PortAudioRepoExamplesApp::mouseDown( MouseEvent event )
{
}

void PortAudioRepoExamplesApp::update()
{
}

void PortAudioRepoExamplesApp::draw()
{
	gl::clear( Color( 0, 0.3f, 0 ) ); 
}

CINDER_APP( PortAudioRepoExamplesApp, RendererGl )
