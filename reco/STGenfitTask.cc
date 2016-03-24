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
#include "STEvent.hh"
#include "STGenfitTask.hh"
#include "STRiemannTrack.hh"
#include "STRiemannHit.hh"
#include "STTrack.hh"
#include "STGlobal.hh"
#include "STDebugLogger.hh"

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
#include "TMath.h"
#include "TGeoManager.h"

//#define DEBUG

ClassImp(STGenfitTask);

STGenfitTask::STGenfitTask()
{
  fIsPersistence = kFALSE;

  fLogger = FairLogger::GetLogger();
  fPar = NULL;

//  fTrackArray = new TClonesArray("genfit::Track");
//  fTrackArray -> SetOwner();
  fTrackArray = new TClonesArray("STTrack");
  fVertexArray = new TClonesArray("genfit::GFRaveVertex");
  fRiemannTrackArray = NULL;
  fHitClusterArray = new TClonesArray("STHitCluster");
  fEventArray = new TClonesArray("STEvent");

  fTPCDetID = 0;

  fMinIterations = 5;
  fMaxIterations = 20;
  fFitter = NULL;

  fIsFindVertex = kFALSE;
  fVertexFactory = NULL;

  fIsDisplay = kFALSE;
  fDisplay = NULL;
}

STGenfitTask::~STGenfitTask()
{
}

void STGenfitTask::SetPersistence(Bool_t value)     { fIsPersistence = value; }
void STGenfitTask::SetMaxIterations(Int_t value)    { fMaxIterations = value; }
void STGenfitTask::SetFindVertex(Bool_t value)      { fIsFindVertex = value; }
void STGenfitTask::SetDisplay(Bool_t value)         { fIsDisplay = value; }

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

  fEventArray = (TClonesArray *) ioMan -> GetObject("STEvent");
  if (fEventArray == 0) {
    fLogger -> Error(MESSAGE_ORIGIN, "Cannot find STEvent array!");
    return kERROR;
  }

  ioMan -> Register("STTrack", "SPiRIT", fTrackArray, fIsPersistence);

  // GENFIT initialization
  fFitter = new genfit::KalmanFitterRefTrack();
  fFitter -> setMinIterations(fMinIterations);
  fFitter -> setMaxIterations(fMaxIterations);

  if (fIsFindVertex == kTRUE) {
    fVertexFactory = new genfit::GFRaveVertexFactory(/* verbosity */ 2, /* useVacuumPropagator */ kFALSE);
    fVertexFactory -> setMethod("kalman-smoothing:1");

    ioMan -> Register("STVertex", "SPiRIT", fVertexArray, fIsPersistence);
  }

  fMeasurementProducer = new genfit::MeasurementProducer<STHitCluster, genfit::STSpacepointMeasurement>(fHitClusterArray);
  fMeasurementFactory = new genfit::MeasurementFactory<genfit::AbsMeasurement>();
  fMeasurementFactory -> addProducer(fTPCDetID, fMeasurementProducer); // detector ID of TPC is 0

  genfit::FieldManager::getInstance() -> init(new genfit::ConstField(0., 5., 0.)); // 0.5 T = 5 kGauss
  genfit::MaterialEffects *materialEffects = genfit::MaterialEffects::getInstance();
  materialEffects -> init(new genfit::TGeoMaterialInterface());
//  materialEffects -> setNoiseCoulomb(kFALSE);
//  materialEffects -> setNoiseBrems(kFALSE);
//  materialEffects -> setNoiseBetheBloch(kFALSE);

  if (fIsDisplay)
    fDisplay = genfit::EventDisplay::getInstance();

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
#ifdef TASKTIMER
  STDebugLogger::Instance() -> TimerStart("GenfitTask");
#endif
#ifdef DEBUG
  fLogger -> Info(MESSAGE_ORIGIN, "Start STGenfitTask!");
#endif
  fTrackArray -> Delete();

  if (fRiemannTrackArray -> GetEntriesFast() == 0)
    return;

  vector<genfit::Track *> tracks;
  TVector3 posSeed;
  TMatrixDSym covSeed(6);

  STEvent *event = (STEvent *) fEventArray -> At(0);

  Int_t numTrackCand = fRiemannTrackArray -> GetEntriesFast();
  Int_t numTrack = 0;
  for (Int_t iTrackCand = 0; iTrackCand < numTrackCand; iTrackCand++) {

#ifdef DEBUG
    fLogger -> Info(MESSAGE_ORIGIN, Form("Track Candidate: %d/%d!", iTrackCand, numTrackCand));
#endif

    STRiemannTrack *riemannTrack = (STRiemannTrack *) fRiemannTrackArray -> At(iTrackCand);
    if (!(riemannTrack -> IsFitted())) continue;

    fHitClusterArray -> Delete();

    genfit::TrackCand trackCand;

    UInt_t numHits = riemannTrack -> GetNumHits();

    std::cout << "Riemann: " << riemannTrack -> GetMom(5.) << " MeV/c  ";

    // First hit position is used as starting position of the initial track
    STRiemannHit *hit = riemannTrack -> GetFirstHit();
    STHitCluster *cluster = event -> GetCluster(hit -> GetHit() -> GetClusterID());
    posSeed = cluster -> GetPosition();
    posSeed.SetMag(posSeed.Mag()/10.);

    // First hit covariance matrix is used as covariance matrix seed of the initial track
    TMatrixD covMatrix = cluster -> GetCovMatrix();
    for (Int_t iComp = 0; iComp < 3; iComp++)
//      covSeed(iComp, iComp) = 0.5*0.5;
      covSeed(iComp, iComp) = covMatrix(iComp, iComp)/100.;
    for (Int_t iComp = 3; iComp < 6; iComp++)
//      covSeed(iComp, iComp) = 0.5*0.5;
      covSeed(iComp, iComp) = covSeed(iComp - 3, iComp - 3);


    std::vector<STHitCluster *> clusters;
    for (UInt_t iHit = 0; iHit < numHits; iHit++) {
      hit = riemannTrack -> GetHit(iHit);
      cluster = event -> GetCluster(hit -> GetHit() -> GetClusterID());

      clusters.push_back(cluster);
    }

    std::sort(clusters.begin(), clusters.end(), [](const STHitCluster *a, const STHitCluster *b) { return a -> GetPosition().Z() < b -> GetPosition().Z(); });

    for (UInt_t iHit = 0; iHit < numHits; iHit++) {
      cluster = clusters.at(iHit);

      new ((*fHitClusterArray)[iHit]) STHitCluster(*cluster);
      trackCand.addHit(fTPCDetID, iHit);
    }

    Double_t dip = riemannTrack -> GetDip();
    Double_t momSeedMag = riemannTrack -> GetMom(5.);
    TVector3 momSeed(0., 0., momSeedMag);
    momSeed.SetTheta(TMath::Pi()/2. - dip);

    trackCand.setPosMomSeedAndPdgCode(posSeed, momSeed, -211);
    trackCand.setCovSeed(covSeed);

    try {
      genfit::RKTrackRep *trackRep = new genfit::RKTrackRep(-211);
      genfit::Track *trackFit = new genfit::Track(trackCand, *fMeasurementFactory, trackRep);
      fFitter -> processTrack(trackFit);
      assert(trackFit -> checkConsistency());

      TVector3 recopos(0,0,0);
      TVector3 recop(0,0,0);
      TMatrixDSym covv(6,6);

      trackFit -> getFittedState().getPosMomCov(recopos, recop, covv);

      Double_t bChi2, fChi2, bNdf, fNdf;
      fFitter -> getChiSquNdf(trackFit, trackFit -> getCardinalRep(), bChi2, fChi2, bNdf, fNdf);

      STTrack *recoTrack = (STTrack *) fTrackArray -> ConstructedAt(fTrackArray -> GetEntriesFast());
      recoTrack -> SetVertex(recopos*10.);
      recoTrack -> SetMomentum(recop*1000.);
      std::cout << "Genfit: " << recop.Mag() << " MeV/c" << std::endl;
      recoTrack -> SetPID(2212);
      recoTrack -> SetMass(938.27);
      recoTrack -> SetChi2(fChi2);
      recoTrack -> SetNDF(fNdf);
      recoTrack -> SetCharge(trackFit -> getFittedState().getCharge());
      recoTrack -> SetTrackLength(trackFit -> getTrackLen());

      tracks.push_back(trackFit);
//      new ((*fTrackArray)[fTrackArray -> GetEntriesFast()]) genfit::Track(*trackFit);

    } catch (genfit::Exception &e) {
      std::cerr << e.what();
      std::cerr << "Exception, next track" << std::endl;
      continue;
    }
  }

  vector<genfit::GFRaveVertex *> vertices;
  if (fIsFindVertex == kTRUE) {
    fVertexFactory -> findVertices(&vertices, tracks);

    for (UInt_t iVert = 0; iVert < vertices.size(); iVert++) {
      genfit::GFRaveVertex* vtx = static_cast<genfit::GFRaveVertex*>(vertices[iVert]);
      vertices[iVert] -> getPos().Print();

      new ((*fVertexArray)[iVert]) genfit::GFRaveVertex(*vtx);

      event -> AddVertex(vtx);
    }
  }

  if (fIsDisplay)
    fDisplay -> addEvent(tracks);
#ifdef TASKTIMER
  STDebugLogger::Instance() -> TimerStop("GenfitTask");
#endif
}

void
STGenfitTask::OpenDisplay() {
  if (fIsDisplay)
    fDisplay -> open();
}
