/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008 Free Software Foundation, Inc.
 * Copyright 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef __SIGPROCLIB_H__
#define __SIGPROCLIB_H__

#include "Vector.h"
#include "Complex.h"
#include "UMTSTransfer.h"
#include "signalVector.h"
#include "GSMCommon.h"

/** Convolution type indicator */
enum ConvType {
  FULL_SPAN = 0,
  OVERLAP_ONLY = 1,
  START_ONLY = 2,
  WITH_TAIL = 3,
  NO_DELAY = 4,
  CUSTOM = 5,
  UNDEFINED = 255
};

/** Convert a linear number to a dB value */
float dB(float x);

/** Convert a dB value into a linear value */
float dBinv(float x);

/** Compute the energy of a vector */
float vectorNorm2(const signalVector &x);

/** Compute the average power of a vector */
float vectorPower(const signalVector &x);

/** conjugate a vector*/
signalVector *conjugate(signalVector *b);

/** reverse conjugate a vector, useful for repeated correlations */
signalVector* reverseConjugate(signalVector *b);


/** Setup the signal processing library */
void sigProcLibSetup(int samplesPerSymbol);

/** Destroy the signal processing library */
void sigProcLibDestroy(void);

/** 
 	Convolve two vectors. 
	@param a,b The vectors to be convolved.
	@param c, A preallocated vector to hold the convolution result.
	@param spanType The type/span of the convolution.
	@return The convolution result.
*/
signalVector* convolve(const signalVector *a,
		       const signalVector *b,
		       signalVector *c,
		       ConvType spanType,
		       unsigned startIx = 0,
		       unsigned len = 0);

/** 
	Generate the GSM pulse. 
	@param samplesPerSymbol The number of samples per GSM symbol.
	@param symbolLength The size of the pulse.
	@return The GSM pulse.
*/
signalVector* generateGSMPulse(int samplesPerSymbol,
			       int symbolLength);

/** 
        Frequency shift a vector.
	@param y The frequency shifted vector.
	@param x The vector to-be-shifted.
	@param freq The digital frequency shift
	@param startPhase The starting phase of the oscillator 
	@param finalPhase The final phase of the oscillator
	@return The frequency shifted vector.
*/
signalVector* frequencyShift(signalVector *y,
			     signalVector *x,
			     double freq = 0.0,
			     float startPhase = 0.0,
			     float *finalPhase=NULL);

/** 
        Correlate two vectors. 
        @param a,b The vectors to be correlated.
        @param c, A preallocated vector to hold the correlation result.
        @param spanType The type/span of the correlation.
        @return The correlation result.
*/
signalVector* correlate(signalVector *a,
			signalVector *b,
			signalVector *c,
			ConvType spanType,
                        bool bReversedConjugated = false,
			unsigned startIx = 0,
			unsigned len = 0);

/** Operate soft slicer on real-valued portion of vector */ 
bool vectorSlicer(signalVector *x);

/** GMSK modulate a GSM burst of bits */
signalVector *modulateBurst(const BitVector &wBurst,
			    const signalVector &gsmPulse,
			    int guardPeriodLength,
			    int samplesPerSymbol);

/** Sinc function */
float sinc(float x);

/** Delay a vector */
void delayVector(signalVector &wBurst,
		 float delay);

/** Add two vectors in-place */
bool addVector(signalVector &x,
	       signalVector &y);

/** Multiply two vectors in-place*/
bool multVector(signalVector &x,
                signalVector &y);

/** Generate a vector of gaussian noise */
signalVector *gaussianNoise(int length,
                            float variance = 1.0,
                            complex mean = complex(0.0));

/**
	Given a non-integer index, interpolate a sample.
	@param inSig The signal from which to interpolate.
	@param ix The index.
	@return The interpolated signal value.
*/
complex interpolatePoint(const signalVector &inSig,
			 float ix);

/**
	Given a correlator output, locate the correlation peak.
	@param rxBurst The correlator result.
	@param peakIndex Pointer to value to receive interpolated peak index.
	@param avgPower Power to value to receive mean power.
	@return Peak value.
*/
complex peakDetect(const signalVector &rxBurst,
		   float *peakIndex,
		   float *avgPwr);

/**
        Apply a scalar to a vector.
        @param x The vector of interest.
        @param scale The scalar.
*/
void scaleVector(signalVector &x,
		 complex scale);

/**      
        Add a constant offset to a vecotr.
        @param x The vector of interest.
        @param offset The offset.
*/
void offsetVector(signalVector &x,
		  complex offset);

/**
        Generate a modulated GSM midamble, stored within the library.
        @param gsmPulse The GSM pulse used for modulation.
        @param samplesPerSymbol The number of samples per GSM symbol.
        @param TSC The training sequence [0..7]
        @return Success.
*/
bool generateMidamble(signalVector &gsmPulse,
		      int samplesPerSymbol,
		      int TSC);
/**
        Generate a modulated RACH sequence, stored within the library.
        @param gsmPulse The GSM pulse used for modulation.
        @param samplesPerSymbol The number of samples per GSM symbol.
        @return Success.
*/
bool generateRACHSequence(signalVector &gsmPulse,
			  int samplesPerSymbol);

/**
        Energy detector, checks to see if received burst energy is above a threshold.
        @param rxBurst The received GSM burst of interest.
        @param windowLength The number of burst samples used to compute burst energy
        @param detectThreshold The detection threshold, a linear value.
        @param avgPwr The average power of the received burst.
        @return True if burst energy is above threshold.
*/
bool energyDetect(signalVector &rxBurst,
		  unsigned windowLength,
                  float detectThreshold,
                  float *avgPwr = NULL);

/**
        RACH correlator/detector.
        @param rxBurst The received GSM burst of interest.
        @param detectThreshold The threshold that the received burst's post-correlator SNR is compared against to determine validity.
        @param samplesPerSymbol The number of samples per GSM symbol.
        @param amplitude The estimated amplitude of received RACH burst.
        @param TOA The estimate time-of-arrival of received RACH burst.
        @return True if burst SNR is larger that the detectThreshold value.
*/
bool detectRACHBurst(signalVector &rxBurst,
		     float detectThreshold,
		     int samplesPerSymbol,
		     complex *amplitude,
		     float* TOA);

/**
        Normal burst correlator, detector, channel estimator.
        @param rxBurst The received GSM burst of interest.
 
        @param detectThreshold The threshold that the received burst's post-correlator SNR is compared against to determine validity.
        @param samplesPerSymbol The number of samples per GSM symbol.
        @param amplitude The estimated amplitude of received TSC burst.
        @param TOA The estimate time-of-arrival of received TSC burst.
        @param maxTOA The maximum expected time-of-arrival
        @param requestChannel Set to true if channel estimation is desired.
        @param channelResponse The estimated channel.
        @param channelResponseOffset The time offset b/w the first sample of the channel response and the reported TOA.
        @return True if burst SNR is larger that the detectThreshold value.
*/
bool analyzeTrafficBurst(signalVector &rxBurst,
			 unsigned TSC,
			 float detectThreshold,
			 int samplesPerSymbol,
			 complex *amplitude,
			 float *TOA,
                         unsigned maxTOA,
                         bool requestChannel = false,
			 signalVector** channelResponse = NULL,
			 float *channelResponseOffset = NULL);

/**
	Decimate a vector.
        @param wVector The vector of interest.
        @param decimationFactor The amount of decimation, i.e. the decimation factor.
        @return The decimated signal vector.
*/
signalVector *decimateVector(signalVector &wVector,
			     int decimationFactor);

/**
        Demodulates a received burst using a soft-slicer.
	@param rxBurst The burst to be demodulated.
        @param gsmPulse The GSM pulse.
        @param samplesPerSymbol The number of samples per GSM symbol.
        @param channel The amplitude estimate of the received burst.
        @param TOA The time-of-arrival of the received burst.
        @return The demodulated bit sequence.
*/
SoftVector *demodulateBurst(signalVector &rxBurst,
			 const signalVector &gsmPulse,
			 int samplesPerSymbol,
			 complex channel,
			 float TOA);

/**
        Creates a simple Kaiser-windowed low-pass FIR filter.
        @param cutoffFreq The digital 3dB bandwidth of the filter.
        @param filterLen The number of taps in the filter.
        @param gainDC The DC gain of the filter.
        @return The desired LPF
*/
signalVector *createLPF(float cutoffFreq,
			int filterLen,
                        float gainDC = 1.0);

/**
	Change sampling rate of a vector via polyphase resampling.
        @param wVector The vector to be resampled.
        @param P The numerator, i.e. the amount of upsampling.
        @param Q The denominator, i.e. the amount of downsampling.
	@param LPF An optional low-pass filter used in the resampling process.
	@return A vector resampled at P/Q of the original sampling rate.
*/    
signalVector *polyphaseResampleVector(signalVector &wVector,
				      int P, int Q,
				      signalVector *LPF);

/**
	Change the sampling rate of a vector via linear interpolation.
	@param wVector The vector to be resampled.
	@param expFactor Ratio of new sampling rate/original sampling rate.
	@param endPoint ???
	@return A vector resampled a expFactor*original sampling rate.
*/
signalVector *resampleVector(signalVector &wVector,
			     float expFactor,
			     complex endPoint);

/**
	Design the necessary filters for a decision-feedback equalizer.
	@param channelResponse The multipath channel that we're mitigating.
	@param SNRestimate The signal-to-noise estimate of the channel, a linear value
	@param Nf The number of taps in the feedforward filter.
	@param feedForwardFilter The designed feed forward filter.
	@param feedbackFilter The designed feedback filter.
	@return True if DFE can be designed.
*/
bool designDFE(signalVector &channelResponse,
	       float SNRestimate,
	       int Nf,
	       signalVector **feedForwardFilter,
	       signalVector **feedbackFilter);

/**
	Equalize/demodulate a received burst via a decision-feedback equalizer.
	@param rxBurst The received burst to be demodulated.
	@param TOA The time-of-arrival of the received burst.
	@param samplesPerSymbol The number of samples per GSM symbol.
	@param w The feed forward filter of the DFE.
	@param b The feedback filter of the DFE.
	@return The demodulated bit sequence.
*/
SoftVector *equalizeBurst(signalVector &rxBurst,
		       float TOA,
		       int samplesPerSymbol,
		       signalVector &w, 
		       signalVector &b);

#endif
