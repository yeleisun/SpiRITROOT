//-----------------------------------------------------------
// Description:
//   Fit the track candidates found by Riemann tracking
//
// Environment:
//   Software developed for the SPiRIT-TPC at RIKEN
//
// Author List:
//   Genie Jhang     Korea University     (original author)
//-----------------------------------------------------------

// SPiRITROOT classes
#include "STGenfitTask.hh"
#include "STRiemannTrack.hh"
#include "STRiemannHit.hh"

// FAIRROOT classes
#include "FairRootManager.h"
#include "FairRun.h"
#include "FairRuntimeDb.h"

// GENFIT2 classes
#include "Track.h"
#include "TrackCand.h"
#include "RKTrackRep.h"
#include "Exception.h"

// STL
#include <iostream>

// ROOT classes
#include "TMatrixDSym.h"
#include "TMatrixD.h"

ClassImp(STGenfitTask);

STGenfitTask::STGenfitTask()
{
  fLogger = FairLogger::GetLogger();
  fPar = NULL;

  fIsPersistence = kFALSE;
  
  fGFTrackArray = new TClonesArray("GENFIT TRACK ARRAY WILL BE HERE");
  fRiemannTrackArray = NULL;
  fHitClusterArray = new TClonesArray("STHitCluster");

  fTPCDetID = 0;

  fMaxIterations = 5;
  fFitter = NULL;

  fIsFindVertex = kFALSE;
  fVertexFactory = NULL;
}

STGenfitTask::~STGenfitTask()
{
}

void STGenfitTask::SetMaxIterations(Int_t value)    { fMaxIterations = value; }
void STGenfitTask::SetFindVertex(Bool_t value)      { fIsFindVertex = value; }

InitStatus
STGenfitTask::Init()
{
  FairRootManager *ioMan = FairRootManager::Instance();
  if (ioMan == 0) {
    fLogger -> Error(MESSAGE_ORIGIN, "Cannot find RootManager!");
    return kERROR;
  }

  fRiemannTrackArray = (TClonesArray *) ioMan -> GetObject("STRiemannTrack");
  if (fRiemannTrackArray == 0) {
    fLogger -> Error(MESSAGE_ORIGIN, "Cannot find STRiemannTrack array!");
    return kERROR;
  }

  ioMan -> Register("STGenfitTrack", "SPiRIT", fGFTrackArray, fIsPersistence);

  // GENFIT initialization
  fFitter = new genfit::KalmanFitterRefTrack();
  fFitter -> setMinIterations(fMaxIterations);

  if (fIsFindVertex == kTRUE) {
    fVertexFactory = new genfit::GFRaveVertexFactory(/* verbosity */ 2, /* useVacuumPropagator */ kFALSE);
    fVertexFactory -> setMethod("kalman-smoothing:1");
  }

  genfit::FieldManager::getInstance() -> init(new genfit::ConstField(0., 5., 0.)); // 0.5 T = 5 kGauss
  genfit::MaterialEffects::getInstance() -> init(new genfit::TGeoMaterialInterface());

  return kSUCCESS;
}

void
STGenfitTask::SetParContainers()
{
  FairRun *run = FairRun::Instance();
  if (!run)
    fLogger -> Fatal(MESSAGE_ORIGIN, "No analysis run!");

  FairRuntimeDb *db = run -> GetRuntimeDb();
  if (!db)
    fLogger -> Fatal(MESSAGE_ORIGIN, "No runtime database!");

  fPar = (STDigiPar *) db -> getContainer("STDigiPar");
  if (!fPar)
    fLogger -> Fatal(MESSAGE_ORIGIN, "STDigiPar not found!!");
}

void
STGenfitTask::Exec(Option_t *opt)
{
  fGFTrackArray -> Delete();

  if (fRiemannTrackArray -> GetEntriesFast() == 0)
    return;

  genfit::MeasurementFactory<genfit::AbsMeasurement> fMeasurementFactory;
  genfit::MeasurementProducer<STHitCluster, genfit::STSpacepointMeasurement> fMeasurementProducer(fHitClusterArray);
  fMeasurementFactory.addProducer(fTPCDetID, &fMeasurementProducer); // detector ID of TPC is 0

  vector<genfit::Track *> tracks;
  TVector3 posSeed;
  TMatrixDSym covSeed(6);

  Int_t numTrackCand = fRiemannTrackArray -> GetEntriesFast();
  for (Int_t iTrackCand = 0; iTrackCand < numTrackCand; iTrackCand++) {
    STRiemannTrack *riemannTrack = (STRiemannTrack *) fRiemannTrackArray -> At(iTrackCand);

    fHitClusterArray -> Delete();
    genfit::TrackCand trackCand;

    UInt_t numHits = riemannTrack -> GetNumHits();

    // First hit position is used as starting position of the initial track
    STRiemannHit *hit = riemannTrack -> GetHit(0);
    posSeed = hit -> GetCluster() -> GetPosition();

    // First hit covariance matrix is used as covariance matrix seed of the initial track
    TMatrixD covMatrix = hit -> GetCluster() -> GetCovMatrix();
    for (Int_t iComp = 0; iComp < 3; iComp++)
      covSeed(iComp, iComp) = covMatrix(iComp, iComp);
    for (Int_t iComp = 3; iComp < 6; iComp++)
      covSeed(iComp, iComp) = covMatrix(iComp - 3, iComp - 3)/(numHits*numHits)/3.;

    for (UInt_t iHit = 1; iHit < numHits; iHit++) {
      hit = riemannTrack -> GetHit(iHit);

      new ((*fHitClusterArray)[iHit]) STHitCluster(*(hit -> GetCluster()));
      trackCand.addHit(fTPCDetID, iHit);
    }

    Double_t dip = riemannTrack -> GetDip();
    Double_t momSeedMag = riemannTrack -> GetMom(5.);
    TVector3 momSeed(0., 0., momSeedMag);
    momSeed.SetTheta(dip);
    momSeed.SetXYZ(momSeed.Y(), momSeed.Z(), momSeed.X());

    trackCand.setPosMomSeedAndPdgCode(posSeed, momSeed, 2212);
    trackCand.setCovSeed(covSeed);

    genfit::Track trackFit(trackCand, fMeasurementFactory, new genfit::RKTrackRep(2212));
    try{
      fFitter -> processTrack(&trackFit);
    }
    catch (genfit::Exception &e){
      std::cerr << e.what();
      std::cerr << "Exception, next track" << std::endl;
      continue;
    }
    
    tracks.push_back(&trackFit);
  }

  vector<genfit::GFRaveVertex *> vertices;
  if (fIsFindVertex == kTRUE) {
    fVertexFactory -> findVertices(&vertices, tracks);
  }
}
