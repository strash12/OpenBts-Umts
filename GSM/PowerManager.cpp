/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2009 Free Software Foundation, Inc.
 * Copyright 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include "PowerManager.h"
#include <Logger.h>
#include <Globals.h>
#include <GSMConfig.h>
#include <TRXManager.h>
#include <ControlCommon.h>

extern TransceiverManager gTRX;

using namespace GSM;



void PowerManager::increasePower()
{
	int maxAtten = gConfig.getNum("UMTS.Radio.PowerManager.MaxAttenDB");
	int minAtten = gConfig.getNum("UMTS.Radio.PowerManager.MinAttenDB");
	if (mAtten==minAtten) {
		LOG(DEBUG) << "power already at maximum";
		return;
	}
	mAtten--;	// raise power by reducing attenuation
	if (mAtten<minAtten) mAtten=minAtten;
	if (mAtten>maxAtten) mAtten=maxAtten;
	LOG(INFO) << "power increased to -" << mAtten << " dB";
	mRadio->setPower(mAtten);
}

void PowerManager::reducePower()
{
	int maxAtten = gConfig.getNum("UMTS.Radio.PowerManager.MaxAttenDB");
	int minAtten = gConfig.getNum("UMTS.Radio.PowerManager.MinAttenDB");
	if (mAtten==maxAtten) {
		LOG(DEBUG) << "power already at minimum";
		return;
	}
	mAtten++; // reduce power be increasing attenuation
	if (mAtten<minAtten) mAtten=minAtten;
	if (mAtten>maxAtten) mAtten=maxAtten;
	LOG(INFO) << "power decreased to -" << mAtten << " dB";
	mRadio->setPower(mAtten);
}


// internal method, does the control step
void PowerManager::internalControlStep()
{
	unsigned target = gConfig.getNum("UMTS.Radio.PowerManager.TargetT3122");
	LOG(DEBUG) << "Avg T3122 " << mAveragedT3122 << ", target " << target;
	// Adapt the power.
	if (mAveragedT3122 > target) reducePower();
	else increasePower();
}


void PowerManager::sampleT3122()
{
	// Tweak it down a little just in case there's no activity.
	mSamples[mNextSampleIndex] = gBTS.shrinkT3122();
	unsigned numSamples = gConfig.getNum("UMTS.Radio.PowerManager.NumSamples");
	mNextSampleIndex = (mNextSampleIndex + 1) % numSamples;
	long sum = 0;
	for (unsigned i=0; i<numSamples; i++) sum += mSamples[i];
	mAveragedT3122 = sum / numSamples;
}


PowerManager::PowerManager()
	: mAveragedT3122(gConfig.getNum("UMTS.Radio.PowerManager.TargetT3122"))
	, mAtten(gConfig.getNum("UMTS.Radio.PowerManager.MaxAttenDB"))
	, mNextSampleIndex(0)
{
	// We don't actually set any power here, since the radio may not exist yet.
	assert(gConfig.getNum("UMTS.Radio.PowerManager.NumSamples")<100);
	bzero(mSamples,sizeof(mSamples));
	LOG(INFO) << "setting initial power to -" << mAtten << " dB";
}



// This is called to actually do the control
void PowerManager::serviceLoop()
{
	while (1) {
		usleep(1000*gConfig.getNum("UMTS.Radio.PowerManager.SamplePeriod"));
		sampleT3122();
		if (mLast.elapsed() < gConfig.getNum("UMTS.Radio.PowerManager.Period")) continue;
		internalControlStep();
		mLast.now();
	}
}


void* GSM::PowerManagerServiceLoopAdapter(PowerManager *pm)
{
	pm->serviceLoop();
	return NULL;
}


void PowerManager::start()
{
	mRadio = gTRX.ARFCN(0);
	mRadio->setPower(mAtten);
	mThread.start((void*(*)(void*))PowerManagerServiceLoopAdapter,this);
}


// vim: ts=4 sw=4
