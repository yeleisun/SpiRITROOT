// =================================================
//  STCore Class
// 
//  Description:
//    Process CoBoFrame data into STRawEvent data
// 
//  Genie Jhang ( geniejhang@majimak.com )
//  2013. 09. 25
// =================================================

#ifndef _STCORE_H_
#define _STCORE_H_

#include "TObject.h"
#include "TString.h"
#include "TClonesArray.h"

#include "STRawEvent.hh"
#include "STMap.hh"
#include "STPedestal.hh"
#include "STGainCalibration.hh"
#include "STPlot.hh"

#include "GETDecoder.hh"

#include <tuple>

class STPlot;

class STCore : public TObject {
  public:
    STCore();
    STCore(TString filename);
    STCore(TString filename, Int_t numTbs, Int_t windowNumTbs = 512, Int_t windowStartTb = 0);

    void Initialize();

    // setters
    Bool_t AddData(TString filename, Int_t coboIdx = 0);
    void SetPositivePolarity(Bool_t value = kTRUE);
    Bool_t SetData(Int_t value);
    void SetDiscontinuousData(Bool_t value = kTRUE);
    Int_t GetNumData(Int_t coboIdx = 0);
    TString GetDataName(Int_t index, Int_t coboIdx = 0);
    void SetNumTbs(Int_t value);
    void SetFPNPedestal(Double_t sigmaThreshold = 5);

    Bool_t SetGainCalibrationData(TString filename, TString dataType = "f");
    void SetGainReference(Int_t row, Int_t layer);
    void SetGainReference(Double_t constant, Double_t linear, Double_t quadratic = 0.);

    Bool_t SetUAMap(TString filename);
    Bool_t SetAGETMap(TString filename);

    void SetUseSeparatedData(Bool_t value = kTRUE);

    void ProcessCobo(Int_t coboIdx);

    Bool_t SetWriteFile(TString filename, Int_t coboIdx = 0, Bool_t overwrite = kFALSE);
    void WriteData();

    // getters
    STRawEvent *GetRawEvent(Long64_t eventID = -1);       ///< Returns STRawEvent object filled with the data
    Int_t GetEventID();                                   ///< Returns the current event ID
    Int_t GetNumTbs(Int_t coboIdx = 0);                   ///< Returns the number of time buckets of the data

    STMap *GetSTMap();
    STPlot *GetSTPlot();

    Int_t GetFPNChannel(Int_t chIdx);

  private:
    STMap *fMapPtr;
    STPlot *fPlotPtr;

    Int_t fNumTbs;

    GETDecoder *fDecoderPtr[12];
    Bool_t fIsData;

    STPedestal *fPedestalPtr[12];
    Bool_t fIsNegativePolarity;
    Double_t fFPNSigmaThreshold;

    STGainCalibration *fGainCalibrationPtr;
    Bool_t fIsGainCalibrationData;

    STRawEvent *fRawEventPtr;
    TClonesArray *fPadArray;

    Int_t fCurrentEventID[12];
    Int_t fTargetFrameID;

    Bool_t fIsSeparatedData;

  ClassDef(STCore, 1);
};

#endif
