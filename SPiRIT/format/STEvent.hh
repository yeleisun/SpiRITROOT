// =================================================
//  STEvent Class                          
//                                                  
//  Description:                                    
//    Container for a event data (hits & tracks)
//                                                  
//  Genie Jhang ( geniejhang@majimak.com )          
//  2014. 05. 19                                    
// =================================================

#ifndef STEVENT_H
#define STEVENT_H

#include "TROOT.h"
#include "TObject.h"
#include "TClonesArray.h"

#include <vector>

#include "STHit.hh"
//#include "STTrack.hh"


class STEvent : public TNamed {
  public:
    STEvent();
    ~STEvent();

    // setters
    void SetEventID(Int_t evtid);
    void AddHit(STHit *hit);

    void SetIsClustered(Bool_t value);
    void SetIsTracked(Bool_t value);

    // getters
    Int_t GetEventID();
    Int_t GetNumHits();
//    Int_t GetNumTracks();

    STHit *GetHit(Int_t hitNo);
    STHit *RemoveHit(Int_t hitNo);

    Bool_t IsClustered();
    Bool_t IsTracked();

  private:
    Bool_t fIsClustered;
    Bool_t fIsTracked;

    Int_t fEventID;

    TClonesArray *fHitsArray;
//    TClonesArray *fTrackArray;

  ClassDef(STEvent, 1);
};

#endif