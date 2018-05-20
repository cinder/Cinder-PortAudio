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

	int numHosts = Pa_GetHostApiCount();
	int defaultHostIndex = Pa_GetDefaultHostApi();

	auto defaultHost = Pa_GetHostApiInfo( defaultHostIndex );

	CI_LOG_I( "numHosts: " << numHosts << ", default name: " << defaultHost->name );
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
