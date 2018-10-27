#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/audio/audio.h"

#include "cinder/audio/ContextPortAudio.h"
#include "cinder/audio/DeviceManagerPortAudio.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class _TBOX_PREFIX_App : public App {
  public:
	void setup() override;
	void draw() override;
	void keyDown( KeyEvent event ) override;

  private:
	void printContextInfo();
};

void _TBOX_PREFIX_App::setup()
{
	// make ContextPortAudio the master context, overriding cinder's default
	audio::ContextPortAudio::setAsMaster();

	// uncomment to set a specific device as output
	//audio::master()->setOutput( audio::master()->createOutputDeviceNode( audio::Device::findDeviceByName( "Speakers (Realtek High Definition Audio)" ) ) );

	// setup basic audio graph
	auto genNode = audio::master()->makeNode<audio::GenSineNode>( 440.0f );
	auto gainNode = audio::master()->makeNode<audio::GainNode>( 0.5f );

	genNode >> gainNode >> audio::master()->getOutput();

	// enable gen and the master context
	genNode->enable();
	audio::master()->enable();
}   

void _TBOX_PREFIX_App::keyDown( KeyEvent event )
{
	if( event.getChar() == 'd' ) {
		CI_LOG_I( "devices:\n" << audio::Device::printDevicesToString() );
	}
	else if( event.getChar() == 'p' ) {
		printContextInfo();
	}
	else if( event.getChar() == ' ' ) {
		audio::master()->setEnabled( ! audio::master()->isEnabled() );
	}
}

void _TBOX_PREFIX_App::printContextInfo()
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

void _TBOX_PREFIX_App::draw()
{
	gl::clear();

	string str = "master context enabled: " + string( audio::master()->isEnabled() ? "true" : "false" );
	gl::drawStringCentered( str, getWindowCenter() - vec2( 40, 10 ), Color( 1, 0.5f, 0 ), Font( "Arial", 30 ) );
}

CINDER_APP( _TBOX_PREFIX_App, RendererGl )
