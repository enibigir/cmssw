#ifndef UCTLayer1_hh
#define UCTLayer1_hh

#include <vector>
#include <memory>

class UCTCrate;
class UCTRegion;
class UCTTower;

#include "UCTGeometry.hh"

class UCTLayer1 {
public:
  // Layer1 hardware firmware version
  // Default (0): initial version for 2016 running
  // 1: Update to include saturated tower codes to layer 2
  //    (put online at run >= 275908: http://cmsonline.cern.ch/cms-elog/931059)
  // 2: Update for all-LUT processing, initially no change in behavior
  //    (put online at run >= 291173: http://cmsonline.cern.ch/cms-elog/973914)
  // 3: Update to handle saturation codes HF (and do division in LUT, and consider HBHE saturation before decompression)
  //    (put online at run >= 299756: http://cmsonline.cern.ch/cms-elog/999604)
  //
  UCTLayer1(int fwv = 0);

  // No copy constructor is needed

  UCTLayer1(const UCTLayer1&) = delete;

  // No equality operator is needed

  const UCTLayer1& operator=(const UCTLayer1&) = delete;

  virtual ~UCTLayer1();

  // To access Layer1 information

  std::vector<UCTCrate*>& getCrates() { return crates; }
  const UCTRegion* getRegion(UCTRegionIndex r) const { return getRegion(r.first, r.second); }
  const std::shared_ptr<UCTTower> getTower(UCTTowerIndex t) const { return getTower(t.first, t.second); }

  // To zero out event in case of selective tower filling
  bool clearEvent();
  // To be called for each non-zero tower to set the event
  // If calling for all towers clearEvent() can be avoided
  bool setECALData(UCTTowerIndex t, bool ecalFG, uint32_t ecalET);
  bool setHCALData(UCTTowerIndex t, uint32_t hcalFB, uint32_t hcalET);
  // To process event
  bool process();

  // More access functions

  uint32_t getSummary() { return uctSummary; }
  uint32_t et() { return uctSummary; }

  friend std::ostream& operator<<(std::ostream&, const UCTLayer1&);

private:
  // Helper functions

  const UCTRegion* getRegion(int regionEtaIndex, uint32_t regionPhiIndex) const;
  const std::shared_ptr<UCTTower> getTower(int caloEtaIndex, int caloPhiIndex) const;

  //Private data

  std::vector<UCTCrate*> crates;

  uint32_t uctSummary;
  const int fwVersion;
};

#endif
