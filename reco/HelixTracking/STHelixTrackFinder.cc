#include "STHelixTrackFinder.hh"

#include <iostream>
using namespace std;

#include "STGlobal.hh"

ClassImp(STHelixTrackFinder)

STHelixTrackFinder::STHelixTrackFinder()
{
  fFitter = new STHelixTrackFitter();
  fEventMap = new STPadPlaneMap();

  fCandHits = new std::vector<STHit*>;
  fGoodHits = new std::vector<STHit*>;
  fBadHits = new std::vector<STHit*>;
}

void 
STHelixTrackFinder::BuildTracks(TClonesArray *hitArray, TClonesArray *trackArray, TClonesArray *hitClusterArray)
{
  fTrackArray = trackArray;
  fHitClusterArray = hitClusterArray;
  fEventMap -> Clear();
  fCandHits -> clear();
  fGoodHits -> clear();
  fBadHits -> clear();

  Int_t numTotalHits = hitArray -> GetEntries();
  for (Int_t iHit = 0; iHit < numTotalHits; iHit++)
    fEventMap -> AddHit((STHit *) hitArray -> At(iHit));

  while(1)
  {
    fCandHits -> clear();
    fGoodHits -> clear();
    fBadHits -> clear();

    STHelixTrack *track = NewTrack();
    if (track == nullptr)
      break;

    bool survive = TrackInitialization(track);
    survive = true;

    while (survive) {
      TrackContinuum(track);
      survive = TrackQualityCheck(track);
      if (!survive)
        break;

      TrackExtrapolation(track);
      survive = TrackQualityCheck(track);
      if (!survive)
        break;

      TrackConfirmation(track);
      break;
    }

    for (auto badHit : *fBadHits)
      fEventMap -> AddHit(badHit);
    fBadHits -> clear();

    if (track -> TrackLength() < 150 || track -> GetHelixRadius() < 25)
      survive = false;

    if (survive) {
      auto trackHits = track -> GetHitArray();
      auto trackID = track -> GetTrackID();
      for (auto trackHit : *trackHits) {
        trackHit -> AddTrackCand(trackID);
        fEventMap -> AddHit(trackHit);
      }
    }
    else {
      auto trackHits = track -> GetHitArray();
      for (auto trackHit : *trackHits) {
        trackHit -> AddTrackCand(-1);
        fEventMap -> AddHit(trackHit);
      }
      fTrackArray -> Remove(track);
    }
  }
  fTrackArray -> Compress();

  TVector3 vertex = FindVertex(fTrackArray);

  auto numTracks = fTrackArray -> GetEntries();
  for (auto iTrack = 0; iTrack < numTracks; iTrack++) {
    auto track = (STHelixTrack *) fTrackArray -> At(iTrack);
    track -> DetermineParticleCharge(vertex);
    if (fClusteringOption == 0)
      HitClustering(track, 24);
    else if (fClusteringOption == 1)
      HitClustering(track, 12);
    else if (fClusteringOption == 2)
      HitClustering2(track);
    track -> FinalizeHits();
    track -> FinalizeClusters();
  }
}

STHelixTrack *
STHelixTrackFinder::NewTrack()
{
  STHit *hit = fEventMap -> PullOutNextFreeHit();
  if (hit == nullptr)
    return nullptr;

  Int_t idx = fTrackArray -> GetEntries();
  STHelixTrack *track = new ((*fTrackArray)[idx]) STHelixTrack(idx);
  track -> AddHit(hit);
  fGoodHits -> push_back(hit);

  return track;
}

STHitCluster *
STHelixTrackFinder::NewCluster(STHit *hit)
{
  Int_t idx = fHitClusterArray -> GetEntries();
  STHitCluster *cluster = new ((*fHitClusterArray)[idx]) STHitCluster();
  cluster -> AddHit(hit);
  cluster -> SetClusterID(idx);
  return cluster;
}

bool
STHelixTrackFinder::TrackInitialization(STHelixTrack *track)
{
  fEventMap -> PullOutNeighborHits(fGoodHits, fCandHits);
  fGoodHits -> clear();

  Int_t numCandHits = fCandHits -> size();;

  while (numCandHits != 0) {
    sort(fCandHits->begin(), fCandHits->end(), STHitByDistanceTo(track->GetMean()));

    for (Int_t iHit = 0; iHit < numCandHits; iHit++) {
      STHit *candHit = fCandHits -> back();
      fCandHits -> pop_back();

      Double_t quality = CorrelateSimple(track, candHit);

      if (quality > 0) {
        fGoodHits -> push_back(candHit);
        track -> AddHit(candHit);

        if (track -> GetNumHits() > 6) {
          if (track -> GetNumHits() > 15) {
            for (auto candHit2 : *fCandHits)
              fEventMap -> AddHit(candHit2);
            fCandHits -> clear();
            break;
          }

          fFitter -> Fit(track);

          if (!(track -> GetNumHits() < 10 && track -> GetHelixRadius() < 30) && (track -> TrackLength() > fDefaultScale * track -> GetRMSW()))
            return true;
        }
        fFitter -> FitPlane(track);
      }
      else
        fBadHits -> push_back(candHit);
    }

    fEventMap -> PullOutNeighborHits(fGoodHits, fCandHits);
    fGoodHits -> clear();

    numCandHits = fCandHits -> size();
  }

  for (auto badHit : *fBadHits)
    fEventMap -> AddHit(badHit);
  fBadHits -> clear();

  return false;
}

bool 
STHelixTrackFinder::TrackContinuum(STHelixTrack *track)
{
  fEventMap -> PullOutNeighborHits(fGoodHits, fCandHits);
  fGoodHits -> clear();

  Int_t numCandHits = fCandHits -> size();

  while (numCandHits != 0)
  {
    sort(fCandHits -> begin(), fCandHits -> end(), STHitSortCharge());

    for (Int_t iHit = 0; iHit < numCandHits; iHit++) {
      STHit *candHit = fCandHits -> back();
      fCandHits -> pop_back();

      Double_t quality = 0; 
      if (CheckHitOwner(candHit) == -2)
        quality = Correlate(track, candHit);

      if (quality > 0) {
        fGoodHits -> push_back(candHit);
        track -> AddHit(candHit);
        fFitter -> Fit(track);
      } else
        fBadHits -> push_back(candHit);
    }

    fEventMap -> PullOutNeighborHits(fGoodHits, fCandHits);
    fGoodHits -> clear();

    numCandHits = fCandHits -> size();
  }

  return true;
}

bool 
STHelixTrackFinder::TrackExtrapolation(STHelixTrack *track)
{
  for (auto badHit : *fBadHits)
    fEventMap -> AddHit(badHit);
  fBadHits -> clear();

  Int_t count = 0;
  bool buildHead = true;
  Double_t extrapolationLength = 0;
  while (AutoBuildByExtrapolation(track, buildHead, extrapolationLength)) {
    if (count++ > 100)
      break;
  }

  count = 0;
  buildHead = !buildHead;
  extrapolationLength = 0;
  while (AutoBuildByExtrapolation(track, buildHead, extrapolationLength)) {
    if (count++ > 100)
      break;
  }

  for (auto badHit : *fBadHits)
    fEventMap -> AddHit(badHit);
  fBadHits -> clear();

  return true;
}

bool 
STHelixTrackFinder::AutoBuildByExtrapolation(STHelixTrack *track, bool &buildHead, Double_t &extrapolationLength)
{
  auto helicity = track -> Helicity();

  TVector3 p;
  if (buildHead) p = track -> ExtrapolateHead(extrapolationLength);
  else           p = track -> ExtrapolateTail(extrapolationLength);

  return AutoBuildAtPosition(track, p, buildHead, extrapolationLength);
}

bool 
STHelixTrackFinder::AutoBuildByInterpolation(STHelixTrack *track, bool &tailToHead, Double_t &extrapolationLength, Double_t rScale)
{
  TVector3 p;
  if (tailToHead) p = track -> InterpolateByLength(extrapolationLength);
  else            p = track -> InterpolateByLength(track -> TrackLength() - extrapolationLength);

  return AutoBuildAtPosition(track, p, tailToHead, extrapolationLength, rScale);
}

bool 
STHelixTrackFinder::AutoBuildAtPosition(STHelixTrack *track, TVector3 p, bool &tailToHead, Double_t &extrapolationLength, Double_t rScale)
{
  if (p.X() < -432 || p.X() < -432 || p.Z() < 0 || p.Z() > 1344 || p.Y() < -530 || p.Y() > 0)
    return false;

  auto helicity = track -> Helicity();

  Double_t rms = 3*track -> GetRMSW();
  if (rms < 25) 
    rms = 25;

  fEventMap -> PullOutNeighborHits(p, rms, fCandHits);
  sort(fCandHits -> begin(), fCandHits -> end(), STHitSortCharge());

  Int_t numCandHits = fCandHits -> size();
  Bool_t foundHit = false;

  for (Int_t iHit = 0; iHit < numCandHits; iHit++) {
    STHit *candHit = fCandHits -> back();
    fCandHits -> pop_back();

    Double_t quality = 0; 
    if (CheckHitOwner(candHit) < 0) 
      quality = Correlate(track, candHit, rScale);

    if (quality > 0) {
      track -> AddHit(candHit);
      fFitter -> Fit(track);
      foundHit = true;
    } else
      fBadHits -> push_back(candHit);
  }

  if (foundHit) {
    extrapolationLength = 10; 
    if (helicity != track -> Helicity())
      tailToHead = !tailToHead;
  }
  else {
    extrapolationLength += 10; 
    if (extrapolationLength > 0.8 * track -> TrackLength()) {
      return false;
    }
  }

  return true;
}

bool
STHelixTrackFinder::TrackQualityCheck(STHelixTrack *track)
{
  Double_t continuity = track -> Continuity();
  if (continuity < .6) {
    if (track -> TrackLength() * continuity < 500)
      return false;
  }

  if (track -> GetHelixRadius() < 25)
    return false;

  return true;
}

bool
STHelixTrackFinder::TrackConfirmation(STHelixTrack *track)
{
  auto tailToHead = false;
  if (track -> PositionAtTail().Z() > track -> PositionAtHead().Z())
    tailToHead = true;

  for (auto badHit : *fBadHits)
    fEventMap -> AddHit(badHit);
  fBadHits -> clear();
  ConfirmHits(track, tailToHead);

  tailToHead = !tailToHead; 

  for (auto badHit : *fBadHits)
    fEventMap -> AddHit(badHit);
  fBadHits -> clear();
  ConfirmHits(track, tailToHead);

  for (auto badHit : *fBadHits)
    fEventMap -> AddHit(badHit);
  fBadHits -> clear();

  return true;
}


bool
STHelixTrackFinder::ConfirmHits(STHelixTrack *track, bool &tailToHead)
{
  track -> SortHits(!tailToHead);
  auto trackHits = track -> GetHitArray();
  auto numHits = trackHits -> size();

  TVector3 q, m;
  auto lPre = track -> ExtrapolateByMap(trackHits->at(numHits-1)->GetPosition(), q, m);

  auto extrapolationLength = 10.;
  for (auto iHit = 1; iHit < numHits; iHit++) 
  {
    STHit *trackHit = trackHits -> at(numHits-iHit-1);
    auto lCur = track -> ExtrapolateByMap(trackHit->GetPosition(), q, m);

    Double_t quality = Correlate(track, trackHit);

    if (quality <= 0) {
      track -> Remove(trackHit);
      trackHit -> RemoveTrackCand(trackHit -> GetTrackID());
      auto helicity = track -> Helicity();
      fFitter -> Fit(track);
      if (helicity != track -> Helicity())
        tailToHead = !tailToHead;
    }

    auto dLength = abs(lCur - lPre);
    extrapolationLength = 10;
    while(dLength > 0 && AutoBuildByInterpolation(track, tailToHead, extrapolationLength, 1)) { dLength -= 10; }
  }

  extrapolationLength = 0;
  while (AutoBuildByExtrapolation(track, tailToHead, extrapolationLength)) {}

  return true;
}

bool
STHelixTrackFinder::HitClustering2(STHelixTrack *track)
{
  track -> SortHitsByTimeOrder();

  auto trackHits = track -> GetHitArray();
  auto numHits = trackHits -> size();

  auto CheckRange = [](STHitCluster *cluster) {
    auto x = cluster -> GetX();
    auto y = cluster -> GetY();
    auto z = cluster -> GetZ();

    if (x > 412 || x < -412 || y < -520 || y > 20 || z < 20 || z > 1324)
      return false;

    Int_t layer = z / 12;
    if (layer > 89 && layer < 100)
      return false;

    return true;
  };

  auto CheckBuildByLayer = [track](STHit *hit) {
    TVector3 q;
    Double_t alpha;
    track -> ExtrapolateToPointAlpha(hit -> GetPosition(), q, alpha);

    auto direction = track -> Direction(alpha);
    Double_t angle = TMath::ATan2(TMath::Abs(direction.Z()), direction.X());
    //if (angle > TMath::ATan2(12,8) && angle < TMath::ATan2(12,-8))
    if (angle > TMath::ATan2(1,1) && angle < TMath::ATan2(1,-1))
      return true;
    else
      return false;
  };

  auto SetClusterLength = [track](STHitCluster *cluster) {
    auto row = cluster -> GetRow();
    auto layer = cluster -> GetLayer();
    auto alpha = track -> AlphaAtPosition(cluster -> GetPosition());
    Double_t length;
    TVector3 q0;
    TVector3 q1;
    if (layer == -1) {
      Double_t x0 = (row)*8.-432.;
      Double_t x1 = (row+1)*8.-432.;
      length = 1;
      track -> ExtrapolateToX(x0, alpha, q0);
      track -> ExtrapolateToX(x1, alpha, q1);
      length = TMath::Abs(track -> Map(q0).Z() - track -> Map(q1).Z());
    } else {
      Double_t z0 = (layer)*12.;
      Double_t z1 = (layer+1)*12.;
      track -> ExtrapolateToZ(z0, alpha, q0);
      track -> ExtrapolateToZ(z1, alpha, q1);
      length = TMath::Abs(track -> Map(q0).Z() - track -> Map(q1).Z());
    }
    cluster -> SetLength(length);
  };

  bool buildNewCluster = true;
  auto curHit = trackHits -> at(0);
  bool buildByLayer = CheckBuildByLayer(curHit);

  STHitCluster *lastCluster = nullptr;
  lastCluster = NewCluster(curHit);

  Int_t rowMin = curHit -> GetRow();
  Int_t rowMax = curHit -> GetRow();
  Int_t layerMin = curHit -> GetLayer();
  Int_t layerMax = curHit -> GetLayer();

  if (buildByLayer) {
    layerMin = layerMin = 0;
    layerMin = layerMax = -1;
    lastCluster -> SetRow(-1);
    lastCluster -> SetLayer(curHit -> GetLayer());
  } else {
    rowMin = rowMin = 0;
    rowMin = rowMax = -1;
    lastCluster -> SetRow(curHit -> GetRow());
    lastCluster -> SetLayer(-1);
  }
  SetClusterLength(lastCluster);

  std::vector<STHitCluster *> buildingClusters;
  buildingClusters.push_back(lastCluster);

  for (auto iHit = 1; iHit < numHits; iHit++)
  {
    curHit = trackHits -> at(iHit);

    auto row = curHit -> GetRow();
    auto layer = curHit -> GetLayer();

    if (!buildNewCluster)
    {
      if (buildByLayer) {
        if (row < rowMin || row > rowMax) {
          buildNewCluster = true;
          layerMin = layer;
          layerMax = layer;
        }
      } else {
        if (layer < layerMin || layer > layerMax) {
          buildNewCluster = true;
          rowMin = row;
          rowMax = row;
        }
      }

      if (buildNewCluster) {
        buildingClusters.clear();
        buildByLayer = !buildByLayer;
      }
    }

    if (buildByLayer)
    {
      bool createNewCluster = true;
      if (layer < layerMin || layer > layerMax) {
        for (auto cluster : buildingClusters) {
          if (layer == cluster -> GetLayer()) {
            createNewCluster = false;
            cluster -> AddHit(curHit);
          }
        }
      } else
        createNewCluster = false;

      if (buildNewCluster) {
        if (row < rowMin) rowMin = row;
        if (row > rowMax) rowMax = row;
      } else
        createNewCluster = false;

      if (createNewCluster) {
        if (buildByLayer !=  CheckBuildByLayer(curHit)) {
          buildNewCluster = false;
          lastCluster = nullptr;
        }
        else {
          if (lastCluster != nullptr) {
            track -> AddHitCluster(lastCluster);
            lastCluster -> SetIsStable(true);
          }
          lastCluster = NewCluster(curHit);
          lastCluster -> SetRow(-1);
          lastCluster -> SetLayer(layer);
          SetClusterLength(lastCluster);
          buildingClusters.push_back(lastCluster);
        }
      }
    }
    else
    {
      bool createNewCluster = true;
      if (row < rowMin || row > rowMax) {
        for (auto cluster : buildingClusters) {
          if (row == cluster -> GetRow()) {
            createNewCluster = false;
            cluster -> AddHit(curHit);
          }
        }
      } else
        createNewCluster = false;

      if (buildNewCluster) {
        if (layer < layerMin) layerMin = layer;
        if (layer > layerMax) layerMax = layer;
      } else
        createNewCluster = false;

      if (createNewCluster) {
        if (buildByLayer !=  CheckBuildByLayer(curHit)) {
          buildNewCluster = false;
          lastCluster = nullptr;
        }
        else {
          if (lastCluster != nullptr) {
            track -> AddHitCluster(lastCluster);
            lastCluster -> SetIsStable(true);
          }
          lastCluster = NewCluster(curHit);
          lastCluster -> SetRow(row);
          lastCluster -> SetLayer(-1);
          SetClusterLength(lastCluster);
          buildingClusters.push_back(lastCluster);
        }
      }
    }
  }

  Int_t numCluster = fHitClusterArray -> GetEntries();
  for (auto iCluster = 0; iCluster < numCluster; iCluster++)
  {
    auto cluster = (STHitCluster *) fHitClusterArray -> At(iCluster);

    if (cluster -> IsStable()) {
      if (CheckRange(cluster))
        cluster -> SetIsStable(true);
      else
        cluster -> SetIsStable(false);
    }
  }

  if (track -> GetNumClusters() < 5) {
    track -> SetIsBad();
    return false;
  }

  if (fFitter -> FitCluster(track) == false) {
    fFitter -> Fit(track);
    track -> SetIsLine();
  }

  return true;
}

bool
STHelixTrackFinder::HitClustering(STHelixTrack *track, Double_t cut)
{
  track -> SortHitsByTimeOrder();

  auto trackHits = track -> GetHitArray();
  auto numHits = trackHits -> size();

  auto CheckMean = [](STHitCluster *cluster, STHit *hit) {
    auto pHit = cluster -> GetPosition();
    auto wHit = cluster -> GetCharge();
    auto pCluster = cluster -> GetPosition();
    auto wCluster = cluster -> GetCharge();
    auto W = wHit + wCluster;

    auto x = (wCluster * pCluster.X() + wHit * pHit.X()) / W;
    auto y = (wCluster * pCluster.Y() + wHit * pHit.Y()) / W;
    auto z = (wCluster * pCluster.Z() + wHit * pHit.Z()) / W;

    return TVector3(x,y,z);
  };

  auto CheckRange = [](STHitCluster *cluster) {
    auto x = cluster -> GetX();
    auto y = cluster -> GetY();
    auto z = cluster -> GetZ();

    if (x > 412 || x < -412 || y < -520 || y > 20 || z < 20 || z > 1324)
      return false;

    Int_t layer = z / 12;
    if (layer > 89 && layer < 100)
      return false;

    return true;
  };

  auto lengthCut = cut / TMath::Cos(track -> DipAngle());

  TVector3 q, m;
  bool isStableCluster = false;
  bool isStableState = false;
  auto addedLength = 0.;

  auto rmsW = 2 * track -> GetRMSW();
  auto rmsH = 2 * track -> GetRMSH();

  auto curHit = trackHits -> at(0);
  auto curCluster = NewCluster(curHit);

  auto curLength = 0;
  auto preLength = track -> ExtrapolateByMap(curHit->GetPosition(),q,m);

  for (auto iHit = 1; iHit < numHits; iHit++)
  {
    curHit = trackHits -> at(iHit);
    curLength = track -> ExtrapolateByMap(curHit -> GetPosition(), q, m);
    auto dLength = std::abs(curLength - preLength);
    preLength = curLength;

    addedLength += .5*dLength;

    bool endCluster = false; // add hit if false, end cluster if true.

    if (dLength > 15.) {
      isStableState = false;
      isStableCluster = false;
      endCluster = true;
    }
    else if (isStableState == false)
    {
      if (addedLength > rmsW) {
        isStableState = true;
        isStableCluster = false;
        endCluster = true;
      }
      else {
        isStableState = false;
        endCluster = false;
      }
    }
    else if (isStableState == true) {
      if (addedLength < lengthCut) {
        endCluster = false;
      }
      else {
        auto p0 = track -> Map(curCluster -> GetPosition());
        auto pc = track -> Map(CheckMean(curCluster, curHit));
        auto w0 = p0.X();
        auto wc = pc.X();
        auto h0 = p0.Y();

        if (abs(wc) < abs(w0)) {
          endCluster = false;
        }
        else {
          if (abs(w0) < rmsW && abs(h0) < rmsH && CheckRange(curCluster))
            isStableCluster = true;
          else
            isStableCluster = false;
          endCluster = true;
        }
      }
    }

    if (endCluster) {
      curCluster -> SetIsStable(isStableCluster);
      addedLength += .5*dLength;
      curCluster -> SetLength(addedLength);
      track -> AddHitCluster(curCluster);
      curCluster = NewCluster(curHit);
      addedLength = .5*dLength;
    }
    else {
      curCluster -> AddHit(curHit);
      addedLength += .5*dLength;
    }
  }

  curCluster -> SetIsStable(false);
  curCluster -> SetLength(addedLength);
  track -> AddHitCluster(curCluster);

  if (track -> GetNumClusters() < 5) {
    track -> SetIsBad();
    return false;
  }

  if (fFitter -> FitCluster(track) == false) {
    fFitter -> Fit(track);
    track -> SetIsLine();
  }

  return true;
}

Int_t
STHelixTrackFinder::CheckHitOwner(STHit *hit)
{
  auto candTracks = hit -> GetTrackCandArray();
  if (candTracks -> size() == 0)
    return -2;

  Int_t trackID = -1;
  for (auto candTrackID : *candTracks) {
    if (candTrackID != -1) {
      trackID = candTrackID;
    }
  }

  return trackID;
}

Double_t 
STHelixTrackFinder::Correlate(STHelixTrack *track, STHit *hit, Double_t rScale)
{
  Double_t scale = rScale * fDefaultScale;
  Double_t trackLength = track -> TrackLength();
  if (trackLength < 500.)
    scale = scale + (500. - trackLength)/500.;

  Double_t rmsWCut = track -> GetRMSW();
  if (rmsWCut < fTrackWCutLL) rmsWCut = fTrackWCutLL;
  if (rmsWCut > fTrackWCutHL) rmsWCut = fTrackWCutHL;
  rmsWCut = scale * rmsWCut;

  Double_t rmsHCut = track -> GetRMSH();
  if (rmsHCut < fTrackHCutLL) rmsHCut = fTrackHCutLL;
  if (rmsHCut > fTrackHCutHL) rmsHCut = fTrackHCutHL;
  rmsHCut = scale * rmsHCut;

  auto qHead = track -> Map(track -> PositionAtHead());
  auto qTail = track -> Map(track -> PositionAtTail());
  auto q = track -> Map(hit -> GetPosition());

  auto LengthAlphaCut = [track](Double_t dLength) {
    if (dLength > 0) {
      if (dLength > .5*track -> TrackLength()) {
        if (abs(track -> AlphaByLength(dLength)) > .5*TMath::Pi()) {
          return true;
        }
      }
    }
    return false;
  };

  if (qHead.Z() > qTail.Z()) {
    if (LengthAlphaCut(q.Z() - qHead.Z())) return 0;
    if (LengthAlphaCut(qTail.Z() - q.Z())) return 0;
  } else {
    if (LengthAlphaCut(q.Z() - qTail.Z())) return 0;
    if (LengthAlphaCut(qHead.Z() - q.Z())) return 0;
  }

  Double_t dr = abs(q.X());
  Double_t quality = 0;
  if (dr < rmsWCut && abs(q.Y()) < rmsHCut)
    quality = sqrt((dr-rmsWCut)*(dr-rmsWCut)) / rmsWCut;

  return quality;
}

Double_t 
STHelixTrackFinder::CorrelateSimple(STHelixTrack *track, STHit *hit)
{
  if (hit -> GetNumTrackCands() != 0)
    return 0;

  Double_t quality = 0;

  Int_t row = hit -> GetRow();
  Int_t layer = hit -> GetLayer();

  auto trackHits = track -> GetHitArray();
  bool ycut = false;
  for (auto trackHit : *trackHits) {
    if (row == trackHit -> GetRow() && layer == trackHit -> GetLayer())
      return 0;
    if (abs(hit->GetY() - trackHit->GetY()) < 12)
      ycut = true;
  }
  if (ycut == false)
    return 0;

  if (track -> IsBad()) {
    quality = 1;
  }
  else if (track -> IsLine()) {
    auto perp = track -> PerpLine(hit -> GetPosition());

    Double_t rmsCut = track -> GetRMSH();
    if (rmsCut < fTrackHCutLL) rmsCut = fTrackHCutLL;
    if (rmsCut > fTrackHCutHL) rmsCut = fTrackHCutHL;
    rmsCut = 3 * rmsCut;

    if (perp.Y() > rmsCut)
      quality = 0;
    else {
      perp.SetY(0);
      if (perp.Mag() < 15)
        quality = 1;
    }
  }
  else if (track -> IsPlane()) {
    Double_t dist = (track -> PerpPlane(hit -> GetPosition())).Mag();

    Double_t rmsCut = track -> GetRMSH();
    if (rmsCut < fTrackHCutLL) rmsCut = fTrackHCutLL;
    if (rmsCut > fTrackHCutHL) rmsCut = fTrackHCutHL;
    rmsCut = 3 * rmsCut;

    if (dist < rmsCut)
      quality = 1;
  }

  return quality;
}

Double_t 
STHelixTrackFinder::TangentOfMaxDipAngle(STHit *hit)
{
  TVector3 v(0, -213.3, -89);
  TVector3 p = hit -> GetPosition();

  Double_t dx = p.X()-v.X();
  Double_t dy = p.Y()-v.Y();
  Double_t dz = p.Z()-v.Z();

  Double_t r = abs(dy / sqrt(dx*dx + dz*dz));

  return r;
}

TVector3
STHelixTrackFinder::FindVertex(TClonesArray *tracks, Int_t nIterations)
{
  auto TestZ = [tracks](TVector3 &v)
  {
    Double_t s = 0;
    Int_t numUsedTracks = 0;

    v.SetX(0);
    v.SetY(0);

    auto numTracks = tracks -> GetEntries();
    for (auto iTrack = 0; iTrack < numTracks; iTrack++) {
      STHelixTrack *track = (STHelixTrack *) tracks -> At(iTrack);

      TVector3 p;
      bool extrapolated = track -> ExtrapolateToZ(v.Z(), p);
      if (extrapolated == false)
        continue;

      v.SetX((numUsedTracks*v.X() + p.X())/(numUsedTracks+1));
      v.SetY((numUsedTracks*v.Y() + p.Y())/(numUsedTracks+1));

      if (numUsedTracks != 0)
        s = (double)numUsedTracks/(numUsedTracks+1)*s + (v-p).Mag()/numUsedTracks;

      numUsedTracks++;
    }

    return s;
  };

  Double_t z0 = 500;
  Double_t dz = 200;
  Double_t s0 = 1.e8;

  const Int_t numSamples = 9;
  Int_t halfOfSamples = (numSamples)/2;

  Double_t zArray[numSamples] = {0};
  for (Int_t iSample = 0; iSample <= numSamples; iSample++)
    zArray[iSample] = (iSample - halfOfSamples) * dz + z0;

  for (auto z : zArray) {
    TVector3 v(0, 0, z);
    Double_t s = TestZ(v);

    if (s < s0) {
      s0 = s;
      z0 = z;
    }
  }

  while (nIterations > 0) {
    dz = dz/halfOfSamples;
    for (Int_t iSample = 0; iSample <= numSamples; iSample++)
      zArray[iSample] = (iSample - halfOfSamples) * dz + z0;

    for (auto z : zArray) {
      TVector3 v(0, 0, z);
      Double_t s = TestZ(v);

      if (s < s0) {
        s0 = s;
        z0 = z;
      }
    }

    nIterations--;
  }

  TVector3 v(0, 0, z0);
  TestZ(v);

  return v;
}

void STHelixTrackFinder::SetClusteringOption(Int_t opt) { fClusteringOption = opt; }
void STHelixTrackFinder::SetDefaultCutScale(Double_t scale) { fDefaultScale = scale; }
void STHelixTrackFinder::SetTrackWidthCutLimits(Double_t lowLimit, Double_t highLimit)
{
  fTrackWCutLL = lowLimit;
  fTrackHCutHL = highLimit;
}
void STHelixTrackFinder::SetTrackHeightCutLimits(Double_t lowLimit, Double_t highLimit)
{
  fTrackHCutLL = lowLimit;
  fTrackHCutHL = highLimit;
}
