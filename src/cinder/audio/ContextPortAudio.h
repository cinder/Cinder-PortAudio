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

#pragma once

#include "cinder/Cinder.h"

#include "cinder/audio/Context.h"

namespace cinder { namespace audio {

class OutputDeviceNodePortAudio : public OutputDeviceNode {
  public:
	OutputDeviceNodePortAudio( const DeviceRef &device, const Format &format );
	~OutputDeviceNodePortAudio();

  protected:
	void initialize()				override;
	void uninitialize()				override;
	void enableProcessing()			override;
	void disableProcessing()		override;
	bool supportsProcessInPlace() const	override	{ return false; }

  private:
	  void renderAudio( float *outputBuffer, size_t framesPerBuffer );

	  struct Impl;
	  std::unique_ptr<Impl>		mImpl;
};

class ContextPortAudio : public Context {
  public:
	ContextPortAudio();
	~ContextPortAudio();
	OutputDeviceNodeRef	createOutputDeviceNode( const DeviceRef &device, const Node::Format &format = Node::Format() )	override;
	InputDeviceNodeRef	createInputDeviceNode( const DeviceRef &device, const Node::Format &format = Node::Format() )	override;

  private:
};

} } // namespace cinder::audio
