#include "RecoLocalCalo/HGCalRecAlgos/interface/ClusterTools.h"

#include "DataFormats/DetId/interface/DetId.h"
#include "DataFormats/ForwardDetId/interface/ForwardSubdetector.h"
#include "DataFormats/HcalDetId/interface/HcalSubdetector.h"
#include "Geometry/HGCalGeometry/interface/HGCalGeometry.h"

#include "FWCore/Framework/interface/ESHandle.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "DataFormats/Common/interface/Handle.h"

#include "vdt/vdtMath.h"

#include <iostream>

using namespace hgcal;
ClusterTools::ClusterTools() {}

ClusterTools::ClusterTools(const edm::ParameterSet& conf, edm::ConsumesCollector& sumes)
    : eetok(sumes.consumes<HGCRecHitCollection>(conf.getParameter<edm::InputTag>("HGCEEInput"))),
      fhtok(sumes.consumes<HGCRecHitCollection>(conf.getParameter<edm::InputTag>("HGCFHInput"))),
      bhtok(sumes.consumes<HGCRecHitCollection>(conf.getParameter<edm::InputTag>("HGCBHInput"))),
      hitMapToken_(sumes.consumes<std::unordered_map<DetId, const unsigned int>>(
          conf.getParameter<edm::InputTag>("hgcalHitMap"))),
      caloGeometryToken_{sumes.esConsumes()} {}

void ClusterTools::getEvent(const edm::Event& ev) {
  eerh_ = &ev.get(eetok);
  fhrh_ = &ev.get(fhtok);
  bhrh_ = &ev.get(bhtok);
  hitMap_ = &ev.get(hitMapToken_);
  rechitManager_ = std::make_unique<MultiVectorManager<HGCRecHit>>();
  rechitManager_->addVector(*eerh_);
  rechitManager_->addVector(*fhrh_);
  rechitManager_->addVector(*bhrh_);
}

void ClusterTools::getEventSetup(const edm::EventSetup& es) { rhtools_.setGeometry(es.getData(caloGeometryToken_)); }

float ClusterTools::getClusterHadronFraction(const reco::CaloCluster& clus) const {
  float energy = 0.f, energyHad = 0.f;
  const auto& hits = clus.hitsAndFractions();
  const auto& rhmanager = *rechitManager_;
  for (const auto& hit : hits) {
    const auto& id = hit.first;
    const float fraction = hit.second;
    auto hitIter = hitMap_->find(id.rawId());
    if (hitIter == hitMap_->end()) {
      continue;
    }
    unsigned int rechitIndex = hitIter->second;
    float hitEnergy = rhmanager[rechitIndex].energy() * fraction;
    energy += hitEnergy;
    if (id.det() == DetId::HGCalHSi || id.det() == DetId::HGCalHSc ||
        (id.det() == DetId::Forward && id.subdetId() == HGCHEF) ||
        (id.det() == DetId::Hcal && id.subdetId() == HcalEndcap)) {
      energyHad += hitEnergy;
    }
  }
  float hadronicFraction = -1.f;
  if (energy > 0.f) {
    hadronicFraction = energyHad / energy;
  }
  return hadronicFraction;
}

math::XYZPoint ClusterTools::getMultiClusterPosition(const reco::HGCalMultiCluster& clu) const {
  if (clu.clusters().empty())
    return math::XYZPoint();

  double acc_x = 0.0;
  double acc_y = 0.0;
  double acc_z = 0.0;
  double totweight = 0.;
  double mcenergy = getMultiClusterEnergy(clu);
  for (const auto& ptr : clu.clusters()) {
    if (mcenergy != 0) {
      if (ptr->energy() < .01 * mcenergy)
        continue;  //cutoff < 1% layer contribution
    }
    const double weight = ptr->energy();  // weigth each corrdinate only by the total energy of the layer cluster
    acc_x += ptr->x() * weight;
    acc_y += ptr->y() * weight;
    acc_z += ptr->z() * weight;
    totweight += weight;
  }
  if (totweight != 0) {
    acc_x /= totweight;
    acc_y /= totweight;
    acc_z /= totweight;
  }
  // return x/y/z in absolute coordinates
  return math::XYZPoint(acc_x, acc_y, acc_z);
}

int ClusterTools::getLayer(const DetId detid) const { return rhtools_.getLayerWithOffset(detid); }

double ClusterTools::getMultiClusterEnergy(const reco::HGCalMultiCluster& clu) const {
  double acc = 0.0;
  for (const auto& ptr : clu.clusters()) {
    acc += ptr->energy();
  }
  return acc;
}

bool ClusterTools::getWidths(const reco::CaloCluster& clus,
                             double& sigmaetaeta,
                             double& sigmaphiphi,
                             double& sigmaetaetal,
                             double& sigmaphiphil) const {
  if (getLayer(clus.hitsAndFractions()[0].first) > (int)rhtools_.lastLayerEE())
    return false;
  const math::XYZPoint& position(clus.position());
  unsigned nhit = clus.hitsAndFractions().size();

  sigmaetaeta = 0.;
  sigmaphiphi = 0.;
  sigmaetaetal = 0.;
  sigmaphiphil = 0.;

  double sumw = 0.;
  double sumlogw = 0.;
  const auto& rhmanager = *rechitManager_;

  for (unsigned int ih = 0; ih < nhit; ++ih) {
    const DetId& id = (clus.hitsAndFractions())[ih].first;
    if ((clus.hitsAndFractions())[ih].second == 0.)
      continue;

    if ((id.det() == DetId::HGCalEE) || (id.det() == DetId::Forward && id.subdetId() == HGCEE)) {
      auto hitIter = hitMap_->find(id.rawId());
      if (hitIter == hitMap_->end()) {
        continue;
      }
      unsigned int rechitIndex = hitIter->second;
      float hitEnergy = rhmanager[rechitIndex].energy();

      GlobalPoint cellPos = rhtools_.getPosition(id);
      double weight = hitEnergy;
      // take w0=2 To be optimized
      double logweight = 0;
      if (clus.energy() != 0) {
        logweight = std::max(0., 2 + std::log(hitEnergy / clus.energy()));
      }
      double deltaetaeta2 = (cellPos.eta() - position.eta()) * (cellPos.eta() - position.eta());
      double deltaphiphi2 = (cellPos.phi() - position.phi()) * (cellPos.phi() - position.phi());
      sigmaetaeta += deltaetaeta2 * weight;
      sigmaphiphi += deltaphiphi2 * weight;
      sigmaetaetal += deltaetaeta2 * logweight;
      sigmaphiphil += deltaphiphi2 * logweight;
      sumw += weight;
      sumlogw += logweight;
    }
  }

  if (sumw <= 0.)
    return false;

  sigmaetaeta /= sumw;
  sigmaetaeta = std::sqrt(sigmaetaeta);
  sigmaphiphi /= sumw;
  sigmaphiphi = std::sqrt(sigmaphiphi);

  if (sumlogw != 0) {
    sigmaetaetal /= sumlogw;
    sigmaetaetal = std::sqrt(sigmaetaetal);
    sigmaphiphil /= sumlogw;
    sigmaphiphil = std::sqrt(sigmaphiphil);
  }

  return true;
}
