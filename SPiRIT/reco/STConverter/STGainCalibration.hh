// =================================================
//  STGainCalibration Class
// 
//  Description:
//    This class provides gain scale factor.
// 
//  Genie Jhang ( geniejhang@majimak.com )
//  2014. 08. 25
// =================================================

#ifndef STGAINCALIBRAITON_H
#define STGAINCALIBRAITON_H

#include "TROOT.h"
#include "TObject.h"

#include "TFile.h"
#include "TTree.h"

#include <fstream>

class STGainCalibration : public TObject {
  public:
    STGainCalibration();
    STGainCalibration(TString gainCalibrationData);
    ~STGainCalibration() {};

    void Initialize();
    Bool_t SetGainCalibrationData(TString gainCalibrationData);

    Bool_t IsSetGainCalibrationData();
    Double_t GetScaleFactor(Int_t padRow, Int_t padLayer);

  private:
    TFile *fOpenFile;
    TTree *fGainCalibrationTree;

    Bool_t fIsSetGainCalibrationData;
    Double_t fScaleFactor[108][112];

  ClassDef(STGainCalibration, 1);
};

#endif