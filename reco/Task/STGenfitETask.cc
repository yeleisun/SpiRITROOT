#include "STGenfitETask.hh"
#include "STDatabasePDG.hh"
#include "STTrack.hh"

#include "FairRun.h"
#include "FairRuntimeDb.h"
#include "FairRootManager.h"

#include "Track.h"

#include <iostream>
using namespace std;

ClassImp(STGenfitETask)

STGenfitETask::STGenfitETask()
: STRecoTask("GENFIT Task", 1, false)
{
}

STGenfitETask::STGenfitETask(Bool_t persistence, Bool_t removeNoVertexEvent)
: STRecoTask("GENFIT Task", 1, persistence)
{
  fRemoveNoVertexEvent = removeNoVertexEvent;
}

STGenfitETask::~STGenfitETask()
{
}

void STGenfitETask::SetIterationCut(Int_t min, Int_t max) 
{
  fGenfitTest -> SetMinIterations(min);
  fGenfitTest -> SetMaxIterations(max);
}

InitStatus
STGenfitETask::Init()
{
  if (STRecoTask::Init() == kERROR)
    return kERROR;

  fHelixTrackArray = (TClonesArray *) fRootManager -> GetObject("STHelixTrack");
  if (fHelixTrackArray == nullptr) {
    LOG(ERROR) << "Cannot find STHelixTrack array!" << FairLogger::endl;
    return kERROR;
  }

  fHitClusterArray = (TClonesArray *) fRootManager -> GetObject("STHitCluster");
  if (fHitClusterArray == nullptr) {
    LOG(ERROR) << "Cannot find STHitCluster array!" << FairLogger::endl;
    return kERROR;
  }

  fTrackArray = new TClonesArray("STTrack");
  fRootManager -> Register("STTrack", "SpiRIT", fTrackArray, fIsPersistence);

  fGenfitTest = new STGenfitTestE();

  fVertexArray = new TClonesArray("genfit::GFRaveVertex");
  fRootManager -> Register("STVertex", "SpiRIT", fVertexArray, fIsPersistence);

  fVertexFactory = new genfit::GFRaveVertexFactory(/* verbosity */ 2, /* useVacuumPropagator */ kFALSE);
  //fVertexFactory -> setMethod("kalman-smoothing:1");
  fVertexFactory -> setMethod("avf-smoothing:1-Tini:256-ratio:0.25-sigmacut:5");

  return kSUCCESS;
}

void STGenfitETask::Exec(Option_t *opt)
{
  fTrackArray -> Clear("C");
  fVertexArray -> Delete();

  if (fEventHeader -> IsBadEvent())
    return;

  vector<genfit::Track *> genfitTrackArray;

  fGenfitTest -> Init();

  Int_t numTrackCand = fHelixTrackArray -> GetEntriesFast();
  for (Int_t iHelixTrack = 0; iHelixTrack < numTrackCand; iHelixTrack++)
  {
    STHelixTrack *helixTrack = (STHelixTrack *) fHelixTrackArray -> At(iHelixTrack);
    if (helixTrack -> IsHelix() == false)
      continue;

    Int_t trackID = fTrackArray -> GetEntriesFast();
    STTrack *recoTrack = (STTrack *) fTrackArray -> ConstructedAt(trackID);
    recoTrack -> SetTrackID(trackID);
    recoTrack -> SetHelixID(helixTrack -> GetTrackID());

    genfit::Track *track = fGenfitTest -> FitTrack(recoTrack, fHitClusterArray, helixTrack);
    if (track != nullptr)
    {
      fGenfitTest -> SetTrack(recoTrack, track);
      if (recoTrack -> IsFitted()) {
        helixTrack -> SetIsGenfitTrack();
        helixTrack -> SetGenfitID(recoTrack -> GetTrackID());
        helixTrack -> SetGenfitMomentum(recoTrack -> GetP());
      }
      else {
        fTrackArray -> Remove(recoTrack);
        continue;
      }
      genfitTrackArray.push_back(track);
    }
    else
      fTrackArray -> Remove(recoTrack);
  }
  fTrackArray -> Compress();

  vector<genfit::GFRaveVertex *> vertices;
  try {
    fVertexFactory -> findVertices(&vertices, genfitTrackArray);
  } catch (...) {
  }

  Int_t numVertices = vertices.size();

  for (UInt_t iVert = 0; iVert < numVertices; iVert++) {
    genfit::GFRaveVertex* vertex = static_cast<genfit::GFRaveVertex*>(vertices[iVert]);

    Int_t numTracks = vertex -> getNTracks();
    for (Int_t iTrack = 0; iTrack < numTracks; iTrack++) {
      genfit::GFRaveTrackParameters *par = vertex -> getParameters(iTrack);
      const genfit::Track *track = par -> getTrack();

      std::vector<genfit::Track *>::iterator iter = std::find(genfitTrackArray.begin(), genfitTrackArray.end(), track);
      if (iter != genfitTrackArray.end())
      {
        Int_t index = std::distance(genfitTrackArray.begin(), iter);
        genfit::Track *gfTrack = genfitTrackArray.at(iTrack);
        STTrack *recoTrack = (STTrack *) fTrackArray -> At(index);
        if (recoTrack -> GetParentID() < 0)
        {
          recoTrack -> SetParentID(iVert);
        }
      }
    }
    new ((*fVertexArray)[iVert]) genfit::GFRaveVertex(*vertex);
    delete vertex;
  }

  if (fRemoveNoVertexEvent)
  {
    Int_t numRecoTracks = fTrackArray -> GetEntriesFast();
    for (Int_t iReco = 0; iReco < numRecoTracks; iReco++) {
      STTrack *recoTrack = (STTrack *) fTrackArray -> At(iReco);
      if (recoTrack -> GetParentID() < 0)
        fTrackArray -> Remove(recoTrack);
    }
    fTrackArray -> Compress();
  }
}

void STGenfitETask::SetRemoveNoVertexEvent(Bool_t val) { fRemoveNoVertexEvent = val; }
