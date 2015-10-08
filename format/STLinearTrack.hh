/**
 * @brief SpiRITROOT linear track container 
 *
 * @author JungWoo Lee
 */

#ifndef STLINEARTRACK
#define STLINEARTRACK

#include "STHit.hh"

#include "TObject.h"
#include "TVector3.h"

#include <vector>

class STLinearTrack : public TObject
{
  public:
    STLinearTrack();
    ~STLinearTrack();

    STLinearTrack(STLinearTrack *track);

    STLinearTrack(Int_t id, 
                  Bool_t isPrimary, 
                  Bool_t isFitted, 
                  TVector3 vertex, 
                  TVector3 normal);

    STLinearTrack(Int_t id, Bool_t isPrimary, Bool_t isFitted,
                  Double_t xV, Double_t yV, Double_t zV,
                  Double_t xN, Double_t yN, Double_t zN);


    void SetTrackID(Int_t id);     ///< Set track ID
    void SetIsPrimary(Bool_t val); ///< Set track primary flag
    void SetIsFitted(Bool_t val);  ///< Set fitted flag

    void SetVertex(TVector3 pos);  ///< Set position of vextex
    void SetNormal(TVector3 vec);  ///< Set normal vector

    void SetXVertex(Double_t x);   ///< Set x position of vextex
    void SetYVertex(Double_t y);   ///< Set y position of vextex
    void SetZVertex(Double_t z);   ///< Set z position of vextex

    void SetXNormal(Double_t x);   ///< Set x component of normal vector
    void SetYNormal(Double_t y);   ///< Set y component of normal vector
    void SetZNormal(Double_t z);   ///< Set z component of normal vector

    void AddHitID(STHit *hit);     ///< Add id into hit ID array


       Int_t GetTrackID() const; ///< Get track ID
      Bool_t IsPrimary()  const; ///< Get track primary flag
      Bool_t IsFitted()   const; ///< Get fitted flag

    TVector3 GetVertex()  const; ///< Get position of vextex
    TVector3 GetNormal()  const; ///< Get normal vector

    Double_t GetXVertex() const; ///< Get x position of vextex
    Double_t GetYVertex() const; ///< Get y position of vextex
    Double_t GetZVertex() const; ///< Get z position of vextex

    Double_t GetXNormal() const; ///< Get x component of normal vector
    Double_t GetYNormal() const; ///< Get y component of normal vector
    Double_t GetZNormal() const; ///< Get z component of normal vector

    std::vector<Int_t>  *GetHitIDArray();      ///< Get hit ID array
    std::vector<STHit*> *GetHitPointerArray(); ///< Get hit ID array

  private:
    Int_t  fTrackID;   ///< Track ID.
    Bool_t fIsPrimary; ///< True if track is primary track.
    Bool_t fIsFitted;  ///< True if fitted.

    Double_t fXVertex; ///< x position of vextex
    Double_t fYVertex; ///< y position of vextex
    Double_t fZVertex; ///< z position of vextex

    Double_t fXNormal; ///< x component of normal vector
    Double_t fYNormal; ///< y component of normal vector
    Double_t fZNormal; ///< z component of normal vector

    std::vector<STHit*> fHitPointerArray; //! < STHit pointer array
    std::vector<Int_t>  fHitIDArray;      /// < hit id array

    void Init(Int_t id, Bool_t isPrimary, Bool_t isFitted,
              Double_t xV, Double_t yV, Double_t zV,
              Double_t xN, Double_t yN, Double_t zN);


  ClassDef(STLinearTrack, 1);
};

#endif