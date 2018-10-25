#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/audio/audio.h"

#include "cinder/audio/ContextPortAudio.h"
#include "cinder/audio/DeviceManagerPortAudio.h"

using namespace ci;
using namespace ci::app;

class _TBOX_PREFIX_App : public App {
  public:
	void setup() override;
	void draw() override;
	void keyDown( KeyEvent event ) override;	
};

void _TBOX_PREFIX_App::setup()
{
	// TODO NEXT: setup basic audio graph
}   

void _TBOX_PREFIX_App::keyDown( KeyEvent event )
{
	// TODO: a couple keys to print graph, devices, enable / disable context
}

void _TBOX_PREFIX_App::draw()
{
	gl::clear();

	// TODO: audio info with gl::drawString
}

CINDER_APP( _TBOX_PREFIX_App, RendererGl )
