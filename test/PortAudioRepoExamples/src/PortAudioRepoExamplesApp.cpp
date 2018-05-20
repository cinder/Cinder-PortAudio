#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

extern int paex_saw_main();

using namespace ci;
using namespace ci::app;
using namespace std;

class PortAudioRepoExamplesApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
};

void PortAudioRepoExamplesApp::setup()
{
	paex_saw_main();
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
