// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
///
/// \brief Joined tables can be used as argument to the process function.
/// \author
/// \since

#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/ASoAHelpers.h"
#include "Framework/HistogramRegistry.h"
#include "Framework/RunningWorkflowInfo.h"
#include "CommonConstants/MathConstants.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Common/DataModel/Centrality.h"
#include "PWGCF/Core/CorrelationContainer.h"
#include "PWGCF/Core/PairCuts.h"

namespace o2::aod
{
namespace hash
{
DECLARE_SOA_COLUMN(Bin, bin, int);
} // namespace hash
DECLARE_SOA_TABLE(Hashes, "AOD", "HASH", hash::Bin);

using Hash = Hashes::iterator;
} // namespace o2::aod

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;

static constexpr float cfgPairCutDefaults[1][5] = {{-1, -1, -1, -1, -1}};

struct firstcorrelations {
  Configurable<float> cfgZVtxCut = {"zvtxcut", 7.0, "Vertex z cut. Default 7 cm"};
  Configurable<float> cfgPtCutMin = {"minpt", 0.2, "Minimum accepted track pT. Default 0.2 GeV"};
  Configurable<float> cfgPtCutMax = {"maxpt", 5.0, "Maximum accepted track pT. Default 5.0 GeV"};
  Configurable<float> cfgEtaCut = {"etacut", 0.8, "Eta cut. Default 0.8"};

  Configurable<LabeledArray<float>> cfgPairCut{"cfgPairCut", {cfgPairCutDefaults[0], 5, {"Photon", "K0", "Lambda", "Phi", "Rho"}}, "Pair cuts on various particles"};

  ConfigurableAxis axisVertex{"axisVertex", {7, -7, 7}, "vertex axis for histograms"};
  ConfigurableAxis axisDeltaPhi{"axisDeltaPhi", {72, -constants::math::PIHalf, constants::math::PIHalf * 3}, "delta phi axis for histograms"};
  ConfigurableAxis axisDeltaEta{"axisDeltaEta", {40, -2, 2}, "delta eta axis for histograms"};
  ConfigurableAxis axisPtTrigger{"axisPtTrigger", {VARIABLE_WIDTH, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 10.0}, "pt trigger axis for histograms"};
  ConfigurableAxis axisPtAssoc{"axisPtAssoc", {VARIABLE_WIDTH, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0}, "pt associated axis for histograms"};
  ConfigurableAxis axisMultiplicity{"axisMultiplicity", {VARIABLE_WIDTH, 0, 5, 10, 20, 30, 40, 50, 100.1}, "multiplicity / centrality axis for histograms"};
  ConfigurableAxis axisVertexEfficiency{"axisVertexEfficiency", {10, -10, 10}, "vertex axis for efficiency histograms"};
  ConfigurableAxis axisEtaEfficiency{"axisEtaEfficiency", {20, -1.0, 1.0}, "eta axis for efficiency histograms"};
  ConfigurableAxis axisPtEfficiency{"axisPtEfficiency", {VARIABLE_WIDTH, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5, 2.75, 3.0, 3.25, 3.5, 3.75, 4.0, 4.5, 5.0, 6.0, 7.0, 8.0}, "pt axis for efficiency histograms"};

  OutputObj<CorrelationContainer> same{"sameEvent"};
  OutputObj<CorrelationContainer> mixed{"mixedEvent"};

  HistogramRegistry registry{"registry"};
  PairCuts mPairCuts;
  bool doPairCuts = false;
  int logcolls = 0;
  int logcollpairs = 0;

  void init(InitContext&)
  {
    LOGF(info, "Starting init");
    registry.add("yields", "centrality vs pT vs eta", {HistType::kTH3F, {{100, 0, 100, "centrality"}, {40, 0, 20, "p_{T}"}, {100, -2, 2, "#eta"}}});
    registry.add("etaphi", "centrality vs eta vs phi", {HistType::kTH3F, {{100, 0, 100, "centrality"}, {100, -2, 2, "#eta"}, {200, 0, 2 * M_PI, "#varphi"}}});

    const int maxMixBin = axisMultiplicity->size() * axisVertex->size();
    registry.add("eventcount", "bin", {HistType::kTH1F, {{maxMixBin + 2, -2.5, -0.5 + maxMixBin, "bin"}}});

    mPairCuts.SetHistogramRegistry(&registry);

    LOGF(info, "Middle init");
    if (cfgPairCut->get("Photon") > 0 || cfgPairCut->get("K0") > 0 || cfgPairCut->get("Lambda") > 0 || cfgPairCut->get("Phi") > 0 || cfgPairCut->get("Rho") > 0) {
      mPairCuts.SetPairCut(PairCuts::Photon, cfgPairCut->get("Photon"));
      mPairCuts.SetPairCut(PairCuts::K0, cfgPairCut->get("K0"));
      mPairCuts.SetPairCut(PairCuts::Lambda, cfgPairCut->get("Lambda"));
      mPairCuts.SetPairCut(PairCuts::Phi, cfgPairCut->get("Phi"));
      mPairCuts.SetPairCut(PairCuts::Rho, cfgPairCut->get("Rho"));
      doPairCuts = true;
    }
    std::vector<AxisSpec> corrAxis = {{axisDeltaEta, "#Delta#eta"},
                                      {axisPtAssoc, "p_{T} (GeV/c)"},
                                      {axisPtTrigger, "p_{T} (GeV/c)"},
                                      {axisMultiplicity, "multiplicity / centrality"},
                                      {axisDeltaPhi, "#Delta#varphi (rad)"},
                                      {axisVertex, "z-vtx (cm)"}};
    std::vector<AxisSpec> effAxis = {{axisEtaEfficiency, "#eta"},
                                     {axisEtaEfficiency, "#eta"},
                                     {axisPtEfficiency, "p_{T} (GeV/c)"},
                                     {axisVertexEfficiency, "z-vtx (cm)"}};
    same.setObject(new CorrelationContainer("sameEvent", "sameEvent", corrAxis, effAxis, {}));
    mixed.setObject(new CorrelationContainer("mixedEvent", "mixedEvent", corrAxis, effAxis, {}));
    LOGF(info, "Finishing init");
  }

  template <typename TCollision, typename TTracks>
  void fillQA(TCollision collision, float centrality, TTracks tracks)
  {
    for (auto& track1 : tracks) {
      registry.fill(HIST("yields"), centrality, track1.pt(), track1.eta());
      registry.fill(HIST("etaphi"), centrality, track1.eta(), track1.phi());
    }
  }

  template <typename TTarget, typename TCollision>
  bool fillCollision(TTarget target, TCollision collision, float centrality)
  {
    target->fillEvent(centrality, CorrelationContainer::kCFStepAll);

    if (!collision.alias()[kINT7] || !collision.sel7()) {
      return false;
    }

    target->fillEvent(centrality, CorrelationContainer::kCFStepReconstructed);

    return true;
  }

  template <typename TTarget, typename TTracks>
  void fillCorrelations(TTarget target, TTracks tracks1, TTracks tracks2, float centrality, float posZ)
  {
    for (auto& track1 : tracks1) {
      target->getTriggerHist()->Fill(CorrelationContainer::kCFStepReconstructed, track1.pt(), centrality, posZ, 1.0);

      for (auto& track2 : tracks2) {
        if (track1 == track2) {
          continue;
        }
        if (doPairCuts && mPairCuts.conversionCuts(track1, track2)) {
          continue;
        }
        float deltaPhi = track1.phi() - track2.phi();
        if (deltaPhi > 1.5f * PI) {
          deltaPhi -= TwoPI;
        }
        if (deltaPhi < -PIHalf) {
          deltaPhi += TwoPI;
        }

        target->getPairHist()->Fill(CorrelationContainer::kCFStepReconstructed,
                                    track1.eta() - track2.eta(), track2.pt(), track1.pt(), centrality, deltaPhi, posZ,
                                    1.0);
      }
    }
  }

  Filter collisionZVtxFilter = nabs(aod::collision::posZ) < cfgZVtxCut;

  Filter trackFilter = (nabs(aod::track::eta) < cfgEtaCut) && (aod::track::pt > cfgPtCutMin) && (aod::track::pt < cfgPtCutMax) &&
                       (requireGlobalTrackInFilter() || (aod::track::isGlobalTrackSDD == (uint8_t) true));

  void processSame(soa::Filtered<soa::Join<aod::Collisions, aod::EvSels, aod::CentRun2V0Ms>>::iterator const& collision, soa::Filtered<soa::Join<aod::Tracks, aod::TrackSelection>> const& tracks)
  {
    const auto centrality = collision.centRun2V0M();

    if (fillCollision(same, collision, centrality) == false) {
      return;
    }
    if (logcolls < 100) {
      logcolls++;
      LOGF(info, "Received collision %ld with %d filtered tracks", collision.globalIndex(), tracks.size());
    }

    registry.fill(HIST("eventcount"), -2);
    fillQA(collision, centrality, tracks);
    fillCorrelations(same, tracks, tracks, centrality, collision.posZ());
  }
  PROCESS_SWITCH(firstcorrelations, processSame, "Process same event", true);

  std::vector<double> vtxBinsEdges{VARIABLE_WIDTH, -7.0f, -5.0f, -3.0f, -1.0f, 1.0f, 3.0f, 5.0f, 7.0f};
  std::vector<double> multBinsEdges{VARIABLE_WIDTH, 0.0f, 5.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0, 100.1f};
  ColumnBinningPolicy<aod::collision::PosZ, aod::cent::CentRun2V0M> bindingOnVtxAndMult{{vtxBinsEdges, multBinsEdges}, true}; // true is for 'ignore overflows' (true by default)
  SameKindPair<soa::Filtered<soa::Join<aod::Collisions, aod::EvSels, aod::CentRun2V0Ms>>, soa::Filtered<soa::Join<aod::Tracks, aod::TrackSelection>>, ColumnBinningPolicy<aod::collision::PosZ, aod::cent::CentRun2V0M>> pair{bindingOnVtxAndMult, 5, -1}; // indicates that 5 events should be mixed and under/overflow (-1) to be ignored

  void processMixed(soa::Filtered<soa::Join<aod::Collisions, aod::EvSels, aod::CentRun2V0Ms>>& collisions, soa::Filtered<soa::Join<aod::Tracks, aod::TrackSelection>> const& tracks)
  {
    LOGF(info, "Received %d collisions", collisions.size());
    for (auto& [collision1, tracks1, collision2, tracks2] : pair) {
      if (logcollpairs < 500) {
        logcollpairs++;
        LOGF(info, "Received collision pair %d: %ld (%f, %f), %ld (%f, %f)", logcollpairs, collision1.globalIndex(), collision1.posZ(), collision1.centRun2V0M(), collision2.globalIndex(), collision2.posZ(), collision2.centRun2V0M());
      }

      if (fillCollision(mixed, collision1, collision1.centRun2V0M()) == false) {
        continue;
      }
      registry.fill(HIST("eventcount"), 1);

      // TODO mixed event weight missing
      fillCorrelations(mixed, tracks1, tracks2, collision1.centRun2V0M(), collision1.posZ());
    }
  }
  PROCESS_SWITCH(firstcorrelations, processMixed, "Process mixed events", true);
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<firstcorrelations>(cfgc),
  };
}
