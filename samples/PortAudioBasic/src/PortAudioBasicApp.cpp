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

void drawAudioBuffer( const ci::audio::Buffer &buffer, const ci::Rectf &bounds, bool drawFrame = false, const ci::ColorA &color = ci::ColorA( 0, 0.9f, 0, 1 ) );

class PortAudioBasicApp : public App {
  public:
	void setup() override;
	void draw() override;
	void keyDown( KeyEvent event ) override;

  private:
	void printContextInfo();

	audio::MonitorNodeRef			mMonitor;
};

void PortAudioBasicApp::setup()
{
	// make ContextPortAudio the master context, overriding cinder's default
	audio::ContextPortAudio::setAsMaster();

	// uncomment to set a specific device as output
	audio::master()->setOutput( audio::master()->createOutputDeviceNode( audio::Device::findDeviceByName( "Speakers (Realtek High Definition Audio)" ) ) );

	// setup basic audio graph
	auto genNode = audio::master()->makeNode<audio::GenSineNode>( 440.0f );
	auto gainNode = audio::master()->makeNode<audio::GainNode>( 0.5f );
	mMonitor = audio::master()->makeNode( new audio::MonitorNode );

	genNode >> gainNode >> mMonitor >> audio::master()->getOutput();

	// enable gen and the master context
	genNode->enable();
	audio::master()->enable();
}   

void PortAudioBasicApp::keyDown( KeyEvent event )
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

void PortAudioBasicApp::printContextInfo()
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

void PortAudioBasicApp::draw()
{
	gl::clear();

	gl::clear( Color( 0, 0.1f, 0.1f ) );

	// Draw the Scope's recorded Buffer, all channels as separate line arrays.
	if( mMonitor && mMonitor->isEnabled() ) {
		Rectf scopeRect( 10, 10, (float)getWindowWidth() - 10, (float)getWindowHeight() - 10 );
		drawAudioBuffer( mMonitor->getBuffer(), scopeRect, true );
	}

	string str = "master context enabled: " + string( audio::master()->isEnabled() ? "true" : "false" );
	gl::drawString( str, vec2( 20, 20 ), Color( 1, 0.5f, 0 ), Font( "Arial", 30 ) );
}

void drawAudioBuffer( const audio::Buffer &buffer, const Rectf &bounds, bool drawFrame, const ci::ColorA &color )
{
	gl::ScopedGlslProg glslScope( getStockShader( gl::ShaderDef().color() ) );

	gl::color( color );

	const float waveHeight = bounds.getHeight() / (float)buffer.getNumChannels();
	const float xScale = bounds.getWidth() / (float)buffer.getNumFrames();

	float yOffset = bounds.y1;
	for( size_t ch = 0; ch < buffer.getNumChannels(); ch++ ) {
		PolyLine2f waveform;
		const float *channel = buffer.getChannel( ch );
		float x = bounds.x1;
		for( size_t i = 0; i < buffer.getNumFrames(); i++ ) {
			x += xScale;
			float y = ( 1 - ( channel[i] * 0.5f + 0.5f ) ) * waveHeight + yOffset;
			waveform.push_back( vec2( x, y ) );
		}

		if( ! waveform.getPoints().empty() )
			gl::draw( waveform );

		yOffset += waveHeight;
	}

	if( drawFrame ) {
		gl::color( color.r, color.g, color.b, color.a * 0.6f );
		gl::drawStrokedRect( bounds );
	}
}

CINDER_APP( PortAudioBasicApp, RendererGl )
