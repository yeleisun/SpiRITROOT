// =================================================
//  STMap Class
// 
//  Description:
//    This class is used for finding the local pad
//    coordinates corresponding to user-input
//    agetIdx & chIdx using map by Tadaaki Isobe.
// 
//  Genie Jhang ( geniejhang@majimak.com )
//  2013. 08. 13
// =================================================

#include "STMap.hh"

#include <fstream>
#include <iostream>

ClassImp(STMap);

STMap::STMap()
{
  fIsSetUAMap = 0;
  fIsSetAGETMap = 0;

  for (Int_t iCobo = 0; iCobo < 12; iCobo++)
    for (Int_t iAsad = 0; iAsad < 4; iAsad++)
      fUAMap[iCobo][iAsad] = -1;

  for (Int_t iCh = 0; iCh < 68; iCh++) {
    fPadRowOfCh[iCh] = -2;
    fPadLayerOfCh[iCh] = -2;
  }
}

// Getters
Bool_t STMap::GetRowNLayer(Int_t coboIdx, Int_t asadIdx, Int_t agetIdx, Int_t chIdx, Int_t &padRow, Int_t &padLayer) {
  if (fPadLayerOfCh[chIdx] == -2) {
    padLayer = -2;
    padRow = -2;

    return kFALSE;
  }

  Int_t UAIdx = GetUAIdx(coboIdx, asadIdx);
  if (UAIdx == -1) {
    padLayer = -2;
    padRow = -2;

    return kFALSE;
  }

  if (UAIdx%100 < 6) {
    padRow = (UAIdx%100)*9 + fPadRowOfCh[chIdx];
    padLayer = (UAIdx/100)*28 + (3 - agetIdx)*7 + fPadLayerOfCh[chIdx];
  } else {
    padRow = (UAIdx%100)*9 + (8 - fPadRowOfCh[chIdx]); 
    padLayer = (UAIdx/100)*28 + agetIdx*7 + (6 - fPadLayerOfCh[chIdx]);
  }


  return kTRUE;
}

Bool_t STMap::GetMapData(Int_t padRow, Int_t padLayer, Int_t &UAIdx, Int_t &coboIdx, Int_t &asadIdx, Int_t &agetIdx, Int_t &chIdx)
{
  if (padRow < 0 || padRow >= 108 || padLayer < 0 || padLayer >= 112) {
    UAIdx = -1;
    coboIdx = -1;
    asadIdx = -1;
    agetIdx = -1;
    chIdx = -1;

    return kFALSE;
  }

  UAIdx = (padLayer/28)*100 + padRow/9;
  coboIdx = GetCoboIdx(UAIdx);
  asadIdx = GetAsadIdx(UAIdx);

  if (padRow/9 < 6) {
    Int_t agetRow = padRow%9;
    Int_t uaLayer = padLayer%28;

    agetIdx = 3 - uaLayer/7;
    Int_t agetLayer = uaLayer%7;

    for (Int_t iCh = 0; iCh < 68; iCh++) {
      if (fPadRowOfCh[iCh] == agetRow && fPadLayerOfCh[iCh] == agetLayer) {
        chIdx = iCh;
        break;
      }
    }
  } else {
    Int_t agetRow = (8 - padRow%9);
    Int_t uaLayer = padLayer%28;

    agetIdx = uaLayer/7;
    Int_t agetLayer = 6 - uaLayer%7;

    for (Int_t iCh = 0; iCh < 68; iCh++)
      if (fPadRowOfCh[iCh] == agetRow && fPadLayerOfCh[iCh] == agetLayer) {
        chIdx = iCh;
        break;
      }
  }

  return kTRUE;
}

Bool_t STMap::IsSetUAMap()
{
  return fIsSetUAMap;
}

Bool_t STMap::IsSetAGETMap()
{
  return fIsSetAGETMap;
}

Bool_t STMap::SetUAMap(TString filename)
{
  fStream.open(filename.Data());

  Char_t dummy[200];
  fStream.getline(dummy, 200);

  if (!fStream.is_open()) {
    std::cout << filename << " is not loaded! Check the existance of the file!" << std::endl;

    fIsSetUAMap = kFALSE;

    return kFALSE;
  }

  Int_t idx = -1, cobo = -1, asad = -1;
  while (!(fStream.eof())) {
    fStream >> idx >> cobo >> asad;
    fUAMap[cobo][asad] = idx;
  }

  fStream.close();

  fIsSetUAMap = kTRUE;
  return kTRUE;
}

Bool_t STMap::SetAGETMap(TString filename)
{
  fStream.open(filename.Data());
  
  Char_t dummy[200];
  fStream.getline(dummy, 200);

  if (!fStream.is_open()) {
    std::cout << filename << " is not loaded! Check the existance of the file!" << std::endl;

    fIsSetAGETMap = kFALSE;

    return kFALSE;
  }

  Int_t ch = -1;
  while (!(fStream.eof())) {
    fStream >> ch;
    fStream >> fPadLayerOfCh[ch] >> fPadRowOfCh[ch];
  }

  fStream.close();

  fIsSetAGETMap = kTRUE;
  return kTRUE;
}

Int_t STMap::GetUAIdx(Int_t coboIdx, Int_t asadIdx)
{
  return fUAMap[coboIdx][asadIdx];
}

Int_t STMap::GetCoboIdx(Int_t uaIdx)
{
  for (Int_t iCobo = 0; iCobo < 12; iCobo++)
    for (Int_t iAsad = 0; iAsad < 4; iAsad++)
      if (fUAMap[iCobo][iAsad] == uaIdx) 
        return iCobo;

  return -1;
}

Int_t STMap::GetAsadIdx(Int_t uaIdx)
{
  for (Int_t iCobo = 0; iCobo < 12; iCobo++)
    for (Int_t iAsad = 0; iAsad < 4; iAsad++)
      if (fUAMap[iCobo][iAsad] == uaIdx) 
        return iAsad;

  return -1;
}

void STMap::SetUAMap(Int_t uaIdx, Int_t coboIdx, Int_t asadIdx)
{
  fUAMap[coboIdx][asadIdx] = uaIdx;
}

void STMap::SetAGETMap(Int_t chIdx, Int_t padRow, Int_t padLayer)
{
  fPadRowOfCh[chIdx] = padRow;
  fPadLayerOfCh[chIdx] = padLayer;
}

void STMap::GetAGETMap(Int_t chIdx, Int_t &padRow, Int_t &padLayer)
{
  padRow = fPadRowOfCh[chIdx];
  padLayer = fPadLayerOfCh[chIdx];
}
