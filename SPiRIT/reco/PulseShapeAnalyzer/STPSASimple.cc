//-----------------------------------------------------------
// Description:
//   Simple version of analyzing pulse shape of raw signal.
//
// Environment:
//   Software developed for the SPiRIT-TPC at RIKEN
//
// Author List:
//   Genie Jhang     Korea University     (original author)
//-----------------------------------------------------------

// SpiRITROOT classes
#include "STPSASimple.hh"

// FairRoot classes
#include "FairRuntimeDb.h"
#include "FairRun.h"

// STL
#include <algorithm>

using std::max_element;
using std::min_element;
using std::distance;

ClassImp(STPSASimple)

STPSASimple::STPSASimple()
{
  fLogger = FairLogger::GetLogger();

  FairRun *run = FairRun::Instance();
  if (!run)
    fLogger -> Fatal(MESSAGE_ORIGIN, "No analysis run!");

  FairRuntimeDb *db = run -> GetRuntimeDb();
  if (!db)
    fLogger -> Fatal(MESSAGE_ORIGIN, "No runtime database!");

  fPar = (STDigiPar *) db -> getContainer("STDigiPar");
  if (!fPar)
    fLogger -> Fatal(MESSAGE_ORIGIN, "STDigiPar not found!!");

  fPadSizeX = fPar -> GetPadSizeX();
  fPadSizeZ = fPar -> GetPadSizeZ();

  fNumTbs = fPar -> GetNumTbs();
  fTBTime = fPar -> GetTBTime();
  fDriftVelocity = fPar -> GetDriftVelocity();
}

STPSASimple::~STPSASimple()
{
}

void
STPSASimple::Analyze(STRawEvent *rawEvent, STEvent *event)
{
  Int_t numPads = rawEvent -> GetNumPads();

  for (Int_t iPad = 0; iPad < numPads; iPad++) {
    STPad *pad = rawEvent -> GetPad(iPad);
    
    Double_t xPos = CalculateX(pad -> GetRow());
    Double_t zPos = CalculateZ(pad -> GetLayer());

    Int_t *rawAdc = pad -> GetRawADC();
    Int_t minAdcIdx = distance(rawAdc, min_element(rawAdc + 4, rawAdc + fNumTbs - 5));

    Double_t yPos = CalculateY(rawAdc, minAdcIdx);
    Double_t charge = rawAdc[minAdcIdx];

    event -> AddHit(new STHit(xPos, yPos, zPos, charge));
  }
}

Double_t
STPSASimple::CalculateX(Int_t row)
{
  return (row + 0.5)*fPadSizeX;
}

Double_t
STPSASimple::CalculateY(Int_t *adc, Int_t peakIdx)
{
  return -peakIdx*fTBTime*fDriftVelocity*10.;
}

Double_t
STPSASimple::CalculateZ(Int_t layer)
{
  return (layer + 0.5)*fPadSizeZ;
}