//---------------------------------------------------------------------
// Description:
//      STDigiPar reads in parameters and stores them for later use.
//
// Author List:
//      Genie Jhang     Korea Univ.            (original author)
//----------------------------------------------------------------------

#include "STDigiPar.hh"

ClassImp(STDigiPar)

STDigiPar::STDigiPar()
:FairParGenericSet("STDigiPar", "SPiRIT Parameter Container", "")
{
}

STDigiPar::~STDigiPar()
{
}

STGas *STDigiPar::GetGas()
{
  return fGas;
}
