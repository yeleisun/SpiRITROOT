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

#ifndef STPSASIMPLE_H
#define STPSASIMPLE_H

// SpiRITROOT classes
#include "STRawEvent.hh"
#include "STPad.hh"
#include "STEvent.hh"
#include "STHit.hh"
#include "STDigiPar.hh"

// FairRoot classes
#include "FairRootManager.h"
#include "FairLogger.h"

// STL
#include <vector>

// ROOT classes
#include "TClonesArray.h"

class STPSASimple
{
  public:
    STPSASimple();
    ~STPSASimple();

    void Analyze(STRawEvent *rawEvent, STEvent *event);

  private:
    FairLogger *fLogger;     //!< logger pointer
    STDigiPar *fPar;         //!< parameter container

    Int_t fPadSizeX;         //!< pad size x in mm
    Int_t fPadSizeZ;         //!< pad size y in mm

    Int_t fNumTbs;           //!< the number of time buckets used in taking data
    Int_t fTBTime;           //!< time duration of a time bucket in ns
    Double_t fDriftVelocity; //!< drift velocity of electron in cm/ns

    //!< Calculate x position in mm. This returns the center position of given pad row.
    Double_t CalculateX(Int_t row);
    //!< Calculate y position in mm using the peak index in adc array.
    Double_t CalculateY(Int_t *adc, Int_t peakIdx);
    //!< Calculate z position in mm. This returns the center position of given pad layer.
    Double_t CalculateZ(Int_t layer);

  ClassDef(STPSASimple, 1)
};

#endif