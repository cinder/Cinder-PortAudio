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

#include "cinder/audio/DeviceManagerPortAudio.h"
#include "cinder/Log.h"

#include "portaudio.h"

#include <math.h>

using namespace std;

namespace cinder { namespace audio {

// ----------------------------------------------------------------------------------------------------
// DeviceManagePortAudio - virtual
// ----------------------------------------------------------------------------------------------------

DeviceManagePortAudio::DeviceManagePortAudio()
{
	PaError err = Pa_Initialize();
	CI_ASSERT( err == paNoError );
}

DeviceManagePortAudio::~DeviceManagePortAudio()
{
	PaError err = Pa_Terminate();
	CI_ASSERT( err == paNoError );
}

DeviceRef DeviceManagePortAudio::getDefaultOutput()
{
	PaDeviceIndex devIndex = Pa_GetDefaultOutputDevice();
	return findDeviceByPaIndex( devIndex );
}

DeviceRef DeviceManagePortAudio::getDefaultInput()
{
	PaDeviceIndex devIndex = Pa_GetDefaultInputDevice();
	return findDeviceByPaIndex( devIndex );
}

const std::vector<DeviceRef>& DeviceManagePortAudio::getDevices()
{
	if( mDevices.empty() ) {
		rebuildDevices();
	}
	return mDevices;
}

std::string DeviceManagePortAudio::getName( const DeviceRef &device )
{
	return getDeviceInfo( device ).mName;
}

size_t DeviceManagePortAudio::getNumInputChannels( const DeviceRef &device )
{
	return getDeviceInfo( device ).mNumInputChannels;
}

size_t DeviceManagePortAudio::getNumOutputChannels( const DeviceRef &device )
{
	return getDeviceInfo( device ).mNumOutputChannels;
}

size_t DeviceManagePortAudio::getSampleRate( const DeviceRef &device )
{
	return getDeviceInfo( device ).mSampleRate;
}

size_t DeviceManagePortAudio::getFramesPerBlock( const DeviceRef &device )
{
	return getDeviceInfo( device ).mFramesPerBlock;
}

void DeviceManagePortAudio::setSampleRate( const DeviceRef &device, size_t sampleRate )
{
	// TODO: use Pa_IsFormatSupported to check if valid first
	getDeviceInfo( device ).mSampleRate = sampleRate;
}

void DeviceManagePortAudio::setFramesPerBlock( const DeviceRef &device, size_t framesPerBlock )
{
	getDeviceInfo( device ).mFramesPerBlock = framesPerBlock;
}

// ----------------------------------------------------------------------------------------------------
// DeviceManagePortAudio - specific
// ----------------------------------------------------------------------------------------------------

int DeviceManagePortAudio::getPaDeviceIndex( const DeviceRef &device ) const
{
	const auto &devInfo = getDeviceInfo( device );
	return devInfo.mPaDeviceIndex;
}

// ----------------------------------------------------------------------------------------------------
// DeviceManagePortAudio Private
// ----------------------------------------------------------------------------------------------------

DeviceRef DeviceManagePortAudio::findDeviceByPaIndex( int index )
{
	if( mDeviceInfoSet.empty() )
		rebuildDevices();

	for( const auto &mp : mDeviceInfoSet ) {
		if( mp.second.mPaDeviceIndex == index )
			return mp.first;
	}

	CI_ASSERT_NOT_REACHABLE();
	return {};
}

DeviceManagePortAudio::DeviceInfo& DeviceManagePortAudio::getDeviceInfo( const DeviceRef &device )
{
	return mDeviceInfoSet.at( device );
}

const DeviceManagePortAudio::DeviceInfo& DeviceManagePortAudio::getDeviceInfo( const DeviceRef &device ) const
{
	return mDeviceInfoSet.at( device );
}

void DeviceManagePortAudio::rebuildDevices()
{
	mDeviceInfoSet.clear();

	PaDeviceIndex numDevices = Pa_GetDeviceCount();
	for( PaDeviceIndex i = 0; i < numDevices; i++ ) {
		DeviceInfo devInfo;
		auto devInfoPa = Pa_GetDeviceInfo( i );
		auto hostInfoPa = Pa_GetHostApiInfo( devInfoPa->hostApi );

		devInfo.mPaDeviceIndex = i;
		devInfo.mPaHostindex = devInfoPa->hostApi;
		devInfo.mName = devInfoPa->name;
		devInfo.mKey = to_string( (int)i ) + " - " + hostInfoPa->name + " - " + devInfo.mName;
		devInfo.mNumInputChannels = devInfoPa->maxInputChannels;
		devInfo.mNumOutputChannels = devInfoPa->maxOutputChannels;
		devInfo.mSampleRate = (size_t)devInfoPa->defaultSampleRate;

		// TODO: need to decide if this should be high, low, input or output
		// - maybe should have DeviceManagerPortAudio-specific method for getting the range based on the DeviceRef
		PaTime latencySeconds = devInfo.mNumOutputChannels > 0 ? devInfoPa->defaultHighOutputLatency : devInfoPa->defaultHighInputLatency;
		//PaTime latencySeconds = devInfo.mNumOutputChannels > 0 ? devInfoPa->defaultLowOutputLatency : devInfoPa->defaultLowInputLatency;
		devInfo.mFramesPerBlock = size_t( lround( latencySeconds * devInfoPa->defaultSampleRate ) );

		DeviceRef addedDevice = addDevice( devInfo.mKey );
		mDeviceInfoSet.insert( make_pair( addedDevice, devInfo ) );
	}
}

} } // namespace cinder::audio
