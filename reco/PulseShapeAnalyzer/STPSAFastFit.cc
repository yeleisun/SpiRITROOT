// SpiRITROOT classes
#include "STPSAFastFit.hh"

// STL
#include <cmath>
#include <thread>
#include <iostream>

using namespace std;

// ROOT
#include "RVersion.h"

//#define DEBUG
//#define DEBUG_WHERE
//#define DEBUG_PEAKFINDING
//#define DEBUG_FIT

#define NEW_ITERATION_METHOD

ClassImp(STPSAFastFit)

STPSAFastFit::STPSAFastFit()
: STPulse()
{
  Init();
}

STPSAFastFit::STPSAFastFit(TString pulseData)
: STPulse(pulseData)
{
  Init();
}

STPSAFastFit::STPSAFastFit(Int_t shapingTime)
: STPulse(shapingTime)
{
  Init();
}

void
STPSAFastFit::Init()
{
  fThreadHitArray = new TClonesArray*[NUMTHREAD];
  for (Int_t iThread = 0; iThread < NUMTHREAD; iThread++)
    fThreadHitArray[iThread] = new TClonesArray("STHit", 100);

  fPadIndex = 0;
  fNumPads = 0;
  
  if (fWindowStartTb == 0)
    fWindowStartTb = 1;

  fTbStartCut = 512 - fNDFTbs - 1;

  fThresholdOneTbStep = fThresholdTbStep * fThreshold;
}

void
STPSAFastFit::Analyze(STRawEvent *rawEvent, STEvent *event)
{
  fNumPads = rawEvent -> GetNumPads();
  fPadArray = rawEvent -> GetPads();
  fPadIndex = 0;

#ifdef DEBUG
  LOG(INFO) << "Start to create threads!" << FairLogger::endl;
#endif

  std::thread thread[NUMTHREAD];
  for (Int_t iThread = 0; iThread < NUMTHREAD; iThread++)
    thread[iThread] = std::thread([this](TClonesArray *array) { this -> PadAnalyzer(array); }, fThreadHitArray[iThread]);

#ifdef DEBUG
  LOG(INFO) << "Successfully created threads!" << FairLogger::endl;
#endif

  for (Int_t iThread = 0; iThread < NUMTHREAD; iThread++) {

#ifdef DEBUG
  LOG(INFO) << "Thread: " << iThread << " is not joinable!"  << FairLogger::endl;
#endif

      thread[iThread].join();
  }

#ifdef DEBUG
  LOG(INFO) << "Joining completed! Merging data!"  << FairLogger::endl;
#endif

  Int_t hitNum = 0;
  for (Int_t iThread = 0; iThread < NUMTHREAD; iThread++) {
    Int_t numHits = fThreadHitArray[iThread] -> GetEntriesFast();

    for (Int_t iHit = 0; iHit < numHits; iHit++) {
      STHit *hit = (STHit *) fThreadHitArray[iThread] -> At(iHit);
      hit -> SetHitID(hitNum++);

      Double_t x = hit -> GetZ();
      Double_t y = hit -> GetY();

      event -> AddHit(hit);
    }
  }
}

void
STPSAFastFit::Analyze(STRawEvent *rawEvent, TClonesArray *hitArray)
{
  fNumPads = rawEvent -> GetNumPads();
  fPadArray = rawEvent -> GetPads();
  fPadIndex = 0;

#ifdef DEBUG
  LOG(INFO) << "Start to create threads!" << FairLogger::endl;
#endif

  std::thread thread[NUMTHREAD];
  for (Int_t iThread = 0; iThread < NUMTHREAD; iThread++)
    thread[iThread] = std::thread([this](TClonesArray *array) { this -> PadAnalyzer(array); }, fThreadHitArray[iThread]);

#ifdef DEBUG
  LOG(INFO) << "Successfully created threads!" << FairLogger::endl;
#endif

  for (Int_t iThread = 0; iThread < NUMTHREAD; iThread++) {

#ifdef DEBUG
  LOG(INFO) << "Thread: " << iThread << " is not joinable!"  << FairLogger::endl;
#endif

      thread[iThread].join();
  }

#ifdef DEBUG
  LOG(INFO) << "Joining completed! Merging data!"  << FairLogger::endl;
#endif

  Int_t hitNum = 0;
  for (Int_t iThread = 0; iThread < NUMTHREAD; iThread++) {
    Int_t numHits = fThreadHitArray[iThread] -> GetEntriesFast();

    for (Int_t iHit = 0; iHit < numHits; iHit++) {
      STHit *hit = (STHit *) fThreadHitArray[iThread] -> At(iHit);
      hit -> SetHitID(hitNum++);

      Double_t x = hit -> GetZ();
      Double_t y = hit -> GetY();

      new ((*hitArray)[hitArray->GetEntriesFast()]) STHit(hit);
    }
  }
}

void STPSAFastFit::PadAnalyzer(TClonesArray *hitArray)
{
  Int_t hitNum = 0;

  hitArray -> Clear("C");

  while (1) {
    STPad *pad = nullptr;

    {
      std::lock_guard<std::mutex> lock(fMutex);
      if (fPadIndex == fNumPads) {

#ifdef DEBUG
  LOG(INFO) << "Thread ended!" << FairLogger::endl;
#endif

        return;
      }

      pad = &(fPadArray -> at(fPadIndex++));

      if (pad -> GetLayer() <= fLayerLowCut || pad -> GetLayer() >= fLayerHighCut )
        continue;
    }

    FindHits(pad, hitArray, hitNum);
  }

#ifdef DEBUG
  LOG(INFO) << "Thread ended!" << FairLogger::endl;
#endif
}

void 
STPSAFastFit::FindHits(STPad *pad, TClonesArray *hitArray, Int_t &hitNum)
{
#ifndef DEBUG_PSA_ITERATION
  Double_t *adcSource = pad -> GetADC();
  Double_t adc[512] = {0};
  memcpy(&adc, adcSource, sizeof(Double_t)*fNumTbs);

  // Pad information
  Int_t row   = pad -> GetRow();
  Int_t layer = pad -> GetLayer();
  Double_t xPos = (row   + 0.5) * fPadSizeX - fPadPlaneX/2.;
  Double_t zPos = (layer + 0.5) * fPadSizeZ;

  // Found peak information
  Int_t tbCurrent = fWindowStartTb;
  Int_t tbStart;

  // Fitted hit information
  Double_t yHit;
  Double_t tbHit;
  Double_t amplitude;
  Double_t squareSum;
     Int_t ndf = fNDFTbs;

  // Previous hit information
  Double_t tbHitPre = 0;
  Double_t amplitudePre = 0;

#ifdef DEBUG_WHERE
  LOG(INFO) << "Start" << FairLogger::endl;
#endif
  while (FindPeak(adc, tbCurrent, tbStart)) 
  {
    if (tbStart > fTbStartCut - 1)
      break;

    if (FitPulse(adc, tbStart, tbCurrent, tbHit, amplitude, squareSum, ndf) == kFALSE)
      continue;

#ifdef DEBUG_PEAKFINDING
    LOG(INFO) 
      << "Fitted Pulse,"
      << " tb:" << tbStart
      << " tbPeak:" << tbCurrent
      << " tbHit:" << tbHit
      << " amp:" << amplitude 
      << " squareSum:" << squareSum 
      << FairLogger::endl;
#endif

    if (TestPulse(adc, tbHitPre, amplitudePre, tbHit, amplitude)) 
    {
#ifdef DEBUG_PEAKFINDING
      LOG(INFO) 
        << "Found hit!"
        << FairLogger::endl;
#endif
      yHit = tbHit * fTbToYConv;

      STHit *hit = (STHit *) hitArray -> ConstructedAt(hitNum);
      hit -> Clear();
      if (amplitude > 3500) amplitude = 3500;
      hit -> SetHit(hitNum, xPos, yHit, zPos, amplitude);
      hit -> SetRow(row);
      hit -> SetLayer(layer);
      hit -> SetTb(tbHit);
      hit -> SetChi2(squareSum);
      hit -> SetNDF(ndf);
      hitNum++;

#ifdef DEBUG_PEAKFINDING
      LOG(INFO) 
        << " amp:" << amplitude 
        << " tb:" << tbHit << "~" << tbHit + ndf << " (y=" << yHit << ")"
        << " squareSum:" << squareSum 
        << FairLogger::endl;
#endif

      tbHitPre = tbHit;
      amplitudePre = amplitude;

      tbCurrent = Int_t(tbHit) + 9;
    }
  }
#ifdef DEBUG_WHERE
  LOG(INFO) << "END" << FairLogger::endl;
#endif

#endif
}

Bool_t
STPSAFastFit::FindPeak(Double_t *adc, 
                          Int_t &tbCurrent, 
                          Int_t &tbStart)
{
#ifdef DEBUG_WHERE
    LOG(INFO) << " Find peak" << FairLogger::endl;
#endif
  Int_t countAscending      = 0;
  Int_t countAscendingBelow = 0;

  for (; tbCurrent < fWindowEndTb; tbCurrent++)
  {
    Double_t diff = adc[tbCurrent] - adc[tbCurrent - 1];

    // If adc difference of step is above threshold
    if (diff > fThresholdOneTbStep) 
    {
      if (adc[tbCurrent] > fThreshold) countAscending++;
      else countAscendingBelow++;
    }
    else 
    {
      // If acended step is below 5, 
      // or negative pulse is bigger than the found pulse, continue
      if (countAscending < fNumAscending || ((countAscendingBelow >= countAscending) && (-adc[tbCurrent - 1 - countAscending - countAscendingBelow] > adc[tbCurrent - 1]))) 
      {
        countAscending = 0;
        countAscendingBelow = 0;
        continue;
      }

      tbCurrent -= 1;
      if (adc[tbCurrent] < fThreshold)
        continue;

      // Peak is found!
      tbStart = tbCurrent - countAscending;
      while (adc[tbStart] < adc[tbCurrent] * 0.05)
        tbStart++;

#ifdef DEBUG_PEAKFINDING
      LOG(INFO) 
        << "Found peak " << tbCurrent 
        << ", starting from " << tbStart
        << ", ascended " << countAscending
        << ", ascended-below " << countAscendingBelow
        << ", peak " << adc[tbCurrent]
        << ", below-peak " << adc[tbCurrent - countAscendingBelow] 
        << FairLogger::endl;
#endif
#ifdef DEBUG_WHERE
    LOG(INFO) << " Peak is found!" << FairLogger::endl;
#endif

      return kTRUE;
    }
  }

  return kFALSE;
}

#ifdef DEBUG_PSA_ITERATION
Bool_t
STPSAFastFit::FitPulse(Double_t *adc, 
                          Int_t tbStart,
                          Int_t tbPeak,
                       Double_t &tbHit, 
                       Double_t &amplitude,
                       Double_t &squareSum,
                          Int_t &ndf,
                          Int_t &option)
#else
Bool_t
STPSAFastFit::FitPulse(Double_t *adc, 
                          Int_t tbStart,
                          Int_t tbPeak,
                       Double_t &tbHit, 
                       Double_t &amplitude,
                       Double_t &squareSum,
                          Int_t &ndf)
#endif
#ifdef NEW_ITERATION_METHOD
{
#ifdef DEBUG_WHERE
  LOG(INFO) << " Fit pulse" << FairLogger::endl;
#endif
  Double_t adcPeak = adc[tbPeak];

  if (adcPeak > 3500) // if peak value is larger than 3500(mostly saturated)
  {
    ndf = tbPeak - tbStart;
    if (ndf > fNDFTbs) ndf = fNDFTbs;
  }

  Double_t alpha   = fAlpha   / (adcPeak * adcPeak); // Weight of time-bucket step
  Double_t betaCut = fBetaCut * (adcPeak * adcPeak); // Effective cut for beta

  Double_t lsPre; // Least-squares of previous fit
  Double_t lsCur; // Least-squares of current fit

  Double_t beta = 0;    // -(lsCur-lsPre)/(tbCur-tbPre)/ndf.
  Double_t dTb = - 0.1; // Time-bucket step to next fit

  Double_t tbPre = tbStart + 1; // Pulse starting time-bucket of previous fit
  Double_t tbCur = tbPre + dTb; // Pulse starting time-bucket of current fit

  LSFitPulse(adc, tbPre, ndf, lsPre, amplitude);
#ifdef DEBUG_PSA_ITERATION
  if (option == 0) {
    tbHit = tbPre;
    squareSum = lsPre;
    return kTRUE;
  }
#endif
  LSFitPulse(adc, tbCur, ndf, lsCur, amplitude);
#ifdef DEBUG_PSA_ITERATION
  if (option == 1) {
    tbHit = tbCur;
    squareSum = lsCur;
    return kTRUE;
  }
#endif

  beta = -(lsCur - lsPre) / (tbCur - tbPre) / ndf;

  Int_t numIteration = 1;
  Bool_t doubleCheckFlag = kFALSE; // Checking flag to apply cut twice in a row

  while (dTb != 0 && lsCur != lsPre)
  {
    lsPre = lsCur;
    tbPre = tbCur;

    dTb = alpha * beta;
    if (dTb > 1) dTb = 1;
    if (dTb < -1) dTb = -1;

    tbCur = tbPre + dTb;
    if (tbCur < 0 || tbCur > fTbStartCut)
    {
#if defined(DEBUG_WHERE) || defined(DEBUG_PSA_ITERATION)
      LOG(INFO) << " Out of bound while fitting" << FairLogger::endl;
#endif
      return kFALSE;
    }

    LSFitPulse(adc, tbCur, ndf, lsCur, amplitude);
    beta = -(lsCur - lsPre) / (tbCur - tbPre) / ndf;

    numIteration++;

#ifdef DEBUG_PSA_ITERATION
    if (option == numIteration) {
      tbHit = tbCur;
      squareSum = lsCur;
      return kTRUE;
    }
#endif

    if (abs(beta) < betaCut)
    {
      if (doubleCheckFlag == kTRUE)
        break;
      else
        doubleCheckFlag = kTRUE;
    }
    else
      doubleCheckFlag = kFALSE;

    if (numIteration >= fIterMax)
      break;
  }

  if (beta > 0) {
    tbHit = tbPre;
    squareSum = lsPre;
  }
  else {
    tbHit = tbCur;
    squareSum = lsCur;
  }

#ifdef DEBUG_WHERE
  LOG(INFO) << " Pulse is fitted!" << FairLogger::endl;
#endif
#ifdef DEBUG_PSA_ITERATION
  return kFALSE;
#endif

  return kTRUE;
}
#endif
#ifndef NEW_ITERATION_METHOD
{
  tbHit = tbStart + 0.5;

  ndf = fNDFTbs;
  squareSum = 0;
  amplitude = 0;
  Double_t squareSum2 = 0;
  Double_t amplitude2 = 0;

  Int_t countIteration = 0;

  // if peak value is larger than 3500(mostly saturated)
  if (adc[tbPeak] > 3500) 
  {
    ndf = tbPeak - tbStart + 1;
    if (ndf > fNDFTbs) ndf = fNDFTbs;
    LSFitPulse(adc, tbHit, ndf, squareSum, amplitude);

    return kTRUE;
  }

  Bool_t increasingTbFlag = kFALSE;

  LSFitPulse(adc, tbHit,       ndf, squareSum,  amplitude);
  LSFitPulse(adc, tbHit + 0.1, ndf, squareSum2, amplitude2);

  if (squareSum2 < squareSum) {
    squareSum = squareSum2;
    tbHit = tbHit + 0.1;
    amplitude = amplitude2;
    increasingTbFlag = kTRUE;
  }

  countIteration = 2;

  if (increasingTbFlag == kTRUE) 
  {
    while (tbHit > fTbStartCut) 
    {
      if (countIteration > fIterMax)
        break;

      countIteration++;

      LSFitPulse(adc, tbHit + 0.1, ndf, squareSum2, amplitude2);
      if (squareSum2 < squareSum) {
        squareSum = squareSum2;
        tbHit += 0.1;
        amplitude = amplitude2;
      }
      else 
        break;
    }
  }
  else 
  {
    while (tbHit > fTbStartCut) 
    {
      if (countIteration > fIterMax)
        break;

      countIteration++;

      LSFitPulse(adc, tbHit - 0.1, ndf, squareSum2, amplitude2);
      if (squareSum2 < squareSum) {
        squareSum = squareSum2;
        tbHit -= 0.1;
        amplitude = amplitude2;
      }
      else 
        break;
    }
  }

  return kTRUE;
}
#endif

void 
STPSAFastFit::LSFitPulse(Double_t *buffer, Double_t tbStart, Int_t ndf, Double_t &chi2, Double_t &amplitude)
{
  Double_t refy = 0;
  Double_t ref2 = 0;

  for (Int_t iTbPulse = 0; iTbPulse < ndf; iTbPulse++) {
    Int_t tb = tbStart + iTbPulse;
    Double_t y = buffer[tb];

    Double_t ref = Pulse(tb + 0.5, 1, tbStart);
    refy += ref * y;
    ref2 += ref * ref;
#ifdef DEBUG_FIT
    LOG(INFO) << "LSFit-" << iTbPulse
              << " tbStart:" << tbStart
              << " y:" << y 
              << " ref:" << ref 
              << " refy:" << refy 
              << " ref2:" << ref2 << FairLogger::endl;
#endif
  }

  if (ref2 == 0)
  {
    chi2 = 1.e10;
    return;
  }

  amplitude = refy / ref2;

  chi2 = 0;
  for (Int_t iTbPulse = 0; iTbPulse < ndf; iTbPulse++) {
    Int_t tb = tbStart + iTbPulse;
    Double_t val = buffer[tb];
    Double_t ref = Pulse(tb + 0.5, amplitude, tbStart);
    chi2 += (val - ref) * (val - ref);
#ifdef DEBUG_FIT
    LOG(INFO) << " tbStart:" << tbStart
              << " tb:" << tb 
              << " val:" << val 
              << " ref:" << ref 
              << " diff:" << val - ref << FairLogger::endl;
#endif
  }
#ifdef DEBUG_FIT
  LOG(INFO) << "==> tbStart:" << tbStart
            << " chi2:" << chi2 
            << " amp:" << amplitude << FairLogger::endl;
#endif
}

Bool_t
STPSAFastFit::TestPulse(Double_t *adc, 
                        Double_t tbHitPre,
                        Double_t amplitudePre, 
                        Double_t tbHit, 
                        Double_t amplitude)
{
#ifdef DEBUG_WHERE
    LOG(INFO) << " Test fit" << FairLogger::endl;
#endif
  Int_t numTbsCorrection = fNumTbsCorrection;

  if (numTbsCorrection + Int_t(tbHit) >= 512)
    numTbsCorrection = 512 - Int_t(tbHit);

  if (amplitude < fThreshold) 
  {
#ifdef DEBUG_PEAKFINDING
    LOG(INFO) 
      << "Amplitude smaller than threshold, "
      << amplitude << " < " << fThreshold
      << FairLogger::endl;
#endif

    return kFALSE;
  }

  if (amplitude < Pulse(tbHit + 9, amplitudePre, tbHitPre) / 2.5) 
  {
#ifdef DEBUG_PEAKFINDING
    LOG(INFO) 
      << "Previous peak shadows current peak, "
      << amplitude << " < " 
      << Pulse(tbHit + 9, amplitudePre, tbHitPre) / 2.5 << FairLogger::endl;
#endif

    for (Int_t iTbPulse = -1; iTbPulse < numTbsCorrection; iTbPulse++) {
      Int_t tb = Int_t(tbHit) + iTbPulse;
      adc[tb] -= Pulse(tb, amplitude, tbHit);
    }

    return kFALSE;
  }

  for (Int_t iTbPulse = -1; iTbPulse < numTbsCorrection; iTbPulse++) {
    Int_t tb = Int_t(tbHit) + iTbPulse;
    adc[tb] -= Pulse(tb, amplitude, tbHit);
  }

#ifdef DEBUG_WHERE
    LOG(INFO) << " Fit is valid!" << FairLogger::endl;
#endif

  return kTRUE;
}
