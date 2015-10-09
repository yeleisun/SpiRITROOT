
/// @brief Orthogonal Distance Regression (ODR) Fitter.
/// @author JungWoo Lee

#include "ODRFitter.hh"

ClassImp(ODRFitter)

ODRFitter::ODRFitter()
{
  Init();
}

ODRFitter::~ODRFitter()
{
}

void 
ODRFitter::Init()
{
  nPoints = 0;

  fCentroid.Clear();
  fCentroid.ResizeTo(3,1);
  fWeightSum = 0;

  fMatrixA.Clear();
  fMatrixA.ResizeTo(3,3);

  fEigenValues.Clear();
  fEigenValues.ResizeTo(3);

  fEigenVectors.Clear();
  fEigenVectors.ResizeTo(3,3);

  fNormal.Clear();
  fNormal.ResizeTo(3);

  fRMS = 0;
}

void 
ODRFitter::SetCentroid(Double_t x, Double_t y, Double_t z)
{
  fCentroid[0][0] = x;
  fCentroid[1][0] = y;
  fCentroid[2][0] = z;
}

void 
ODRFitter::AddPoint(Double_t x, Double_t y, Double_t z, Double_t w)
{
  // building position matrix
  TMatrixD matrixP(3, 1);
  matrixP[0][0] = x;
  matrixP[1][0] = y;
  matrixP[2][0] = z;
  
  // building difference from position to centroid matrix
  TMatrixD matrixD(3, 1); 
  matrixD = matrixP - fCentroid;

  // adding to matrix A with position matrix and difference matrix
  TMatrixD matrixDT(TMatrixD::kTransposed, matrixD);
  TMatrixD matrixD2(matrixD, TMatrixD::kMult, matrixDT);
  matrixD2 *= w;
  fMatrixA += matrixD2;  

  // adding to the square of distance from point to the centroid
  Double_t PC2 = matrixD2[0][0] + matrixD2[1][0] + matrixD2[2][0];
  fSumOfPC2 += PC2;

  fWeightSum += w;

  nPoints++;
}

void 
ODRFitter::SolveEigenValueEquation()
{
  std::cout << "Solving eigen value equation!" << std::endl;

  if (fWeightSum == 0)
    fWeightSum = 1;
  
  fMatrixA *= 1./fWeightSum;
  fEigenVectors = fMatrixA.EigenVectors(fEigenValues);
}

Int_t 
ODRFitter::FitLine()
{
  SolveEigenValueEquation();

  std::cout << "caculating normal vector!" << std::endl;

  fNormal = TMatrixDColumn(fEigenVectors, 0);
  Double_t absNormal = TMath::Sqrt(fNormal.Norm2Sqr());
  fNormal *= 1./absNormal;

  return 0;
}

Int_t 
ODRFitter::FitPlane()
{
  return 0;
}

TVector3 
ODRFitter::GetCentroid() 
{ return TVector3(fCentroid[0][0], fCentroid[1][0], fCentroid[2][0]); }

TVector3 
ODRFitter::GetNormal() 
{ return TVector3(fNormal[0], fNormal[1], fNormal[2]); }

TVector3 
ODRFitter::GetDirection() 
{ return TVector3(fNormal[0], fNormal[1], fNormal[2]); }

Double_t ODRFitter::GetRMS() { return fRMS; }
