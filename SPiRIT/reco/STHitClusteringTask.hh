//-----------------------------------------------------------
// Description:
//   Clustering hits processed by PSATask
//
// Environment:
//   Software developed for the SPiRIT-TPC at RIKEN
//
// Author List:
//   Genie Jhang     Korea University     (original author)
//-----------------------------------------------------------

#ifndef _STCLUSTERINGTASK_H_
#define _STCLUSTERINGTASK_H_

// SpiRITROOT classes
#include "STHit.hh"
#include "STDigiPar.hh"

// FairROOT classes
#include "FairTask.h"
#include "FairLogger.h"

// ROOT classes
#include "TClonesArray.h"

class STHitClusteringTask : public FairTask
{
  public:
    STHitClusteringTask();
    ~STHitClusteringTask();

    void SetPersistence(Bool_t value = kTRUE);
    
    virtual InitStatus Init();
    virtual void SetParContainers();
    virtual void Exec(Option_t *opt);

  private:
    FairLogger *fLogger;

    STDigiPar *fPar;

    Bool_t fIsPersistence;

    Double_t fDriftLength;
    Int_t fYDivider;

    TClonesArray *fEventHArray;
    TClonesArray *fEventHCArray;


  ClassDef(STHitClusteringTask, 1);
};

#endif
