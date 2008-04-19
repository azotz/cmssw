#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include <TFile.h>
#include <TH1.h>
#include <TH3.h>
#include <Math/GenVector/VectorUtil.h>

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EDAnalyzer.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/InputTag.h"
#include "FWCore/ParameterSet/interface/Registry.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "SimDataFormats/JetMatching/interface/JetFlavour.h"
#include "SimDataFormats/JetMatching/interface/JetFlavourMatching.h"
#include "DataFormats/Common/interface/View.h"
#include "DataFormats/Common/interface/TriggerResults.h"
#include "DataFormats/JetReco/interface/Jet.h"
#include "DataFormats/JetReco/interface/JetTracksAssociation.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/VertexReco/interface/VertexFwd.h"
#include "DataFormats/BTauReco/interface/JetTag.h"
#include "DataFormats/HLTReco/interface/TriggerEvent.h"
#include "DataFormats/HLTReco/interface/TriggerEventWithRefs.h"
#include "HLTriggerOffline/BJet/interface/JetPlots.h"
#include "HLTriggerOffline/BJet/interface/OfflineJetPlots.h"
#include "HLTriggerOffline/BJet/interface/FlavouredJetPlots.h"
#include "HLTriggerOffline/BJet/interface/VertexPlots.h"


// search the Registry for the ParameterSetID of the ParameterSet describing the given process
// returns an invalid ID if the process name is not found
edm::ParameterSetID psetIdForProcess(const std::string & process) {
  const edm::pset::Registry * registry = edm::pset::Registry::instance();
  for (edm::pset::Registry::const_iterator i = registry->begin(); i != registry->end(); ++i) {
    if (i->second.exists("@process_name") and i->second.getParameter<std::string>("@process_name") == process)
      return i->first;
  }
  return edm::ParameterSetID();
}


// find the index of the object key of an association vector closest to a given jet, within a given distance
template <typename T, typename V>
int closestJet(const reco::Jet & jet, const edm::AssociationVector<T, V> & association, double distance) {
  int closest = -1;
  for (unsigned int i = 0; i < association.size(); ++i) {
    double d = ROOT::Math::VectorUtil::DeltaR(jet.momentum(), association[i].first->momentum());
    if (d < distance) {
      distance = d;
      closest  = i;
    }
  }
  return closest;
}


class HLTBtagLifetimeAnalyzer : public edm::EDAnalyzer {
public:
  explicit HLTBtagLifetimeAnalyzer(const edm::ParameterSet& config);
  virtual ~HLTBtagLifetimeAnalyzer();
    
  virtual void beginJob(const edm::EventSetup & setup);
  virtual void analyze(const edm::Event & event, const edm::EventSetup & setup);
  virtual void endJob();

private:
  bool cachePathDescription(const edm::TriggerResults & triggerResult, const trigger::TriggerEventWithRefs & triggerEvent);
  bool cachePathDescription(const edm::ParameterSetID & triggerPSetID, const edm::ParameterSetID & processPSetID);

private:
  struct InputData {
    std::string     m_name;                     // name used for the ROOT plots
    std::string     m_title;                    // title shown on the plots
    edm::InputTag   m_filter;                   // filter checked for pass/fail
    edm::InputTag   m_jets;                     // jet collection used for detailed efficiencies
    edm::InputTag   m_tracks;                   // track collection, associated to jets
    unsigned int    m_filterIndex;              // index of the filter relative to its own path
  };
  
  // input collections
  std::string               m_triggerPath;      // HLT path
  edm::InputTag             m_triggerEvent;     // HLT trigger summary with trigger objects
  edm::InputTag             m_triggerResults;   // HLT trigger results
  edm::InputTag             m_vertex;           // primary vertex
  std::vector<InputData>    m_levels;

  // counters for per-event efficiencies
  std::vector<unsigned int> m_events;           // number of jets passing each level

  // match to MC truth
  edm::InputTag             m_mcPartons;        // MC truth match - jet association to partons
  std::vector<std::string>  m_mcLabels;         // MC truth match - labels
  std::vector<flavours_t>   m_mcFlavours;       // MC truth match - flavours selection
  double                    m_mcRadius;         // MC truth match - deltaR association radius

  // match to Offline reco
  edm::InputTag             m_offlineBJets;     // Offline match - jet association to discriminator
  std::vector<std::string>  m_offlineLabels;    // Offline match - labels
  std::vector<double>       m_offlineCuts;      // Offline match - discriminator cuts
  double                    m_offlineRadius;    // Offline match - deltaR association radius

  // plot configuration
  double m_jetMinEnergy;
  double m_jetMaxEnergy;
  double m_jetMaxEta;

  double m_vertexMaxR;
  double m_vertexMaxZ;

  // plot data
  VertexPlots                       m_vertexPlots;
  std::vector<JetPlots>             m_jetPlots;
  std::vector<FlavouredJetPlots>    m_mcPlots;
  std::vector<OfflineJetPlots>      m_offlinePlots;

  // output configuration
  std::string m_outputFile;

  // leep a cache of the path description
  std::vector<std::string>          m_pathModules;
  unsigned int                      m_pathIndex;
  bool                              m_pathCached;
};


HLTBtagLifetimeAnalyzer::HLTBtagLifetimeAnalyzer(const edm::ParameterSet & config) :
  m_triggerPath( config.getParameter<std::string>("triggerPath") ),
  m_triggerEvent( config.getParameter<edm::InputTag>("triggerEvent") ),
  m_triggerResults( config.getParameter<edm::InputTag>("triggerResults") ),
  m_vertex( config.getParameter<edm::InputTag>("vertex") ),
  m_levels(),
  m_events(),
  m_mcPartons( config.getParameter<edm::InputTag>("mcPartons") ),
  m_mcLabels(),
  m_mcFlavours(),
  m_mcRadius( config.getParameter<double>("mcRadius") ),
  m_offlineBJets( config.getParameter<edm::InputTag>("offlineBJets") ),
  m_offlineLabels(),
  m_offlineCuts(),
  m_offlineRadius( config.getParameter<double>("offlineRadius") ),
  m_jetMinEnergy(  0. ),    //   0 GeV
  m_jetMaxEnergy( 300. ),   // 300 GeV
  m_jetMaxEta( 5. ),        //  ±5 pseudorapidity units
  m_vertexMaxR( 0.1 ),      //   1 mm
  m_vertexMaxZ( 15. ),      //  15 cm
  m_vertexPlots(),
  m_jetPlots(),
  m_mcPlots(),
  m_offlinePlots(),
  m_outputFile( config.getParameter<std::string>("outputFile") ),
  m_pathModules(),
  m_pathIndex((unsigned int) -1),
  m_pathCached(false)
{
  const std::vector<edm::ParameterSet> levels = config.getParameter<std::vector<edm::ParameterSet> >("levels");
  for (unsigned int i = 0; i < levels.size(); ++i) {
    InputData level;
    level.m_jets   = levels[i].getParameter<edm::InputTag>("jets");
    level.m_filter = levels[i].getParameter<edm::InputTag>("filter");
    level.m_tracks = levels[i].exists("tracks") ? levels[i].getParameter<edm::InputTag>("tracks") : edm::InputTag("none");
    level.m_name   = levels[i].exists("name")   ? levels[i].getParameter<std::string>("name")     : level.m_jets.encode();
    level.m_title  = levels[i].exists("title")  ? levels[i].getParameter<std::string>("title")    : level.m_name;
    m_levels.push_back(level);
  }
    
  const edm::ParameterSet & jetConfig = config.getParameter<edm::ParameterSet>("jetConfiguration");
  m_jetMaxEnergy = jetConfig.getParameter<double>("maxEnergy");
  m_jetMaxEta    = jetConfig.getParameter<double>("maxEta");
  const edm::ParameterSet & vertexConfig = config.getParameter<edm::ParameterSet>("vertexConfiguration");
  m_vertexMaxR = vertexConfig.getParameter<double>("maxR");
  m_vertexMaxZ = vertexConfig.getParameter<double>("maxZ");

  edm::ParameterSet mc = config.getParameter<edm::ParameterSet>("mcFlavours");
  m_mcLabels = mc.getParameterNamesForType<std::vector<unsigned int> >();
  for (unsigned int i = 0; i < m_mcLabels.size(); ++i)
    m_mcFlavours.push_back( mc.getParameter<std::vector<unsigned int> >(m_mcLabels[i]) );

  edm::ParameterSet offline = config.getParameter<edm::ParameterSet>("offlineCuts");
  m_offlineLabels = offline.getParameterNamesForType<double>();
  for (unsigned int i = 0; i < m_offlineLabels.size(); ++i)
    m_offlineCuts.push_back( offline.getParameter<double>(m_offlineLabels[i]) );
}

HLTBtagLifetimeAnalyzer::~HLTBtagLifetimeAnalyzer() 
{
}

void HLTBtagLifetimeAnalyzer::beginJob(const edm::EventSetup & setup) 
{
  m_events.resize( m_levels.size(), 0 );
  m_jetPlots.resize( m_levels.size() );
  m_mcPlots.resize( m_levels.size() );
  m_offlinePlots.resize( m_levels.size() );
  
  for (unsigned int i = 0; i < m_levels.size(); ++i) {
    m_jetPlots[i].init(     m_levels[i].m_name, m_levels[i].m_title,                                 jetEnergyBins, m_jetMinEnergy, m_jetMaxEnergy, jetGeometryBins, m_jetMaxEta, m_levels[i].m_tracks.label() != "none" );
    m_mcPlots[i].init(      m_levels[i].m_name, m_levels[i].m_title, m_mcFlavours,  m_mcLabels,      jetEnergyBins, m_jetMinEnergy, m_jetMaxEnergy, jetGeometryBins, m_jetMaxEta, m_levels[i].m_tracks.label() != "none" );
    m_offlinePlots[i].init( m_levels[i].m_name, m_levels[i].m_title, m_offlineCuts, m_offlineLabels, jetEnergyBins, m_jetMinEnergy, m_jetMaxEnergy, jetGeometryBins, m_jetMaxEta, m_levels[i].m_tracks.label() != "none" );
  }
  
  m_vertexPlots.init( "PrimaryVertex", "Primary vertex", vertex1DBins, m_vertexMaxZ, m_vertexMaxR );
}

// access and cache the description of the HLT path and filters
bool HLTBtagLifetimeAnalyzer::cachePathDescription(const edm::TriggerResults & triggerResults, const trigger::TriggerEventWithRefs & triggerEvent) 
{
  return cachePathDescription(triggerResults.parameterSetID(), psetIdForProcess(triggerEvent.usedProcessName()));
}
  
// access and cache the description of the HLT path and filters
bool HLTBtagLifetimeAnalyzer::cachePathDescription(const edm::ParameterSetID & triggerPSetID, const edm::ParameterSetID & processPSetID)
{
  if (m_pathCached)
    return true;

  const edm::pset::Registry * registry = edm::pset::Registry::instance();
  edm::ParameterSet           pset;
  std::vector<std::string>    paths;

  if (registry->getMapped(triggerPSetID, pset)) {
    #if 0
    std::ofstream out("trigger.pset");
    out << pset;
    out.close();
    #endif
    paths = pset.getParameter<std::vector<std::string> >("@trigger_paths");
  } else {
    std::cerr << "cannot map HLT trigger names to indices" << std::endl;
    return false;
  }
  if (registry->getMapped(processPSetID, pset)) {
    #if 0
    std::ofstream out("process.pset");
    out << pset;
    out.close();
    #endif
    m_pathModules = pset.getParameter<std::vector<std::string> >(m_triggerPath);
  } else {
    std::cerr << "cannot find HLT path " << m_triggerPath << " in the process description" << std::endl;
    return false;
  }

  for (m_pathIndex = 0; m_pathIndex < paths.size(); ++m_pathIndex)
    if (paths[m_pathIndex] == m_triggerPath)
      break;
  if (m_pathIndex == paths.size()) {
    std::cerr << "cannot find HLT path " << m_triggerPath << std::endl;
    return false;
  }

  // find the filter index for each "level"
  for (unsigned int l = 0; l < m_levels.size(); ++l) {
    InputData & level = m_levels[l];
    std::vector<std::string>::const_iterator i = std::find(m_pathModules.begin(), m_pathModules.end(), level.m_filter.label());
    if (i != m_pathModules.end()) {
      level.m_filterIndex = i - m_pathModules.begin();
      std::cerr << "filter " << level.m_filter.label() << " has index " << level.m_filterIndex << " in path " << m_triggerPath << std::endl;
    } else {
      level.m_filterIndex = 0;
      std::cerr << "filter " << level.m_filter.label() << " not found in path " << m_triggerPath << std::endl;
    }
  }

  m_pathCached = true;
  return true;
}

void HLTBtagLifetimeAnalyzer::analyze(const edm::Event & event, const edm::EventSetup & setup) 
{
  edm::Handle<edm::TriggerResults> h_triggerResults;
  event.getByLabel(m_triggerResults, h_triggerResults);
  if (not h_triggerResults.isValid()) {
    std::cerr << "invalid edm::TriggerResults handle" << std::endl;
    return;
  }

  edm::Handle<trigger::TriggerEventWithRefs> h_triggerEvent;
  event.getByLabel(m_triggerEvent, h_triggerEvent);
  if (not h_triggerEvent.isValid()) {
    std::cerr << "invalid trigger::TriggerEventWithRefs handle" << std::endl;
    return;
  }

  if (not cachePathDescription(* h_triggerResults, * h_triggerEvent)) {
    std::cerr << "unable to access trigger informations and description for path " << m_triggerPath << std::endl;
    return;
  }

  bool         wasrun   = h_triggerResults->wasrun( m_pathIndex );
  unsigned int latest   = h_triggerResults->index( m_pathIndex );
  bool         accepted = h_triggerResults->accept( m_pathIndex );
  if (latest >= m_pathModules.size()) {
    std::cerr << "error determinig the path stopping condition: module position exceeds path length" << std::endl;
    return;
  }

  // debug information regarding th path status
  if (not wasrun)
    std::cout << "  path " << m_triggerPath << " was not run" << std::endl;
  else if (accepted)
    std::cout << "  path " << m_triggerPath << " accepted the event" << std::endl;
  else
    std::cout << "  path " << m_triggerPath << " rejected the event at module " << m_pathModules[latest] << std::endl;

  edm::Handle<reco::VertexCollection> h_vertex;
  event.getByLabel(m_vertex, h_vertex);
  if (h_vertex.isValid() and not h_vertex->empty())
    m_vertexPlots.fill(h_vertex->front());

  edm::Handle<reco::JetFlavourMatchingCollection> h_mcPartons;
  event.getByLabel(m_mcPartons, h_mcPartons);
  const reco::JetFlavourMatchingCollection & mcPartons = * h_mcPartons;
  
  edm::Handle<reco::JetTagCollection> h_offlineBJets;
  event.getByLabel(m_offlineBJets, h_offlineBJets);
  const reco::JetTagCollection & offlineBJets = * h_offlineBJets;

  for (unsigned int l = 0; l < m_levels.size(); ++l) {
    const InputData & level = m_levels[l];

    bool passed = accepted or (latest > level.m_filterIndex);               // accepted by this filter
    bool failed = (not accepted) and (latest == level.m_filterIndex);       // rejected by this filter
    //bool notrun = (not accepted) and (latest  < level.m_filterIndex);       // did not reach this filter
    #ifdef DEBUG
    std::cout << "  path " << m_triggerPath << ", filter " << std::setw(32) << std::left << level.m_filter.label() << std::right << (passed ? "passed" : failed ? "failed" : "not run") << std::endl;
    #endif
    
    edm::Handle<edm::View<reco::Jet> >                  h_jets;
    edm::Handle<reco::JetTracksAssociation::Container>  h_tracks;
    
    if (level.m_jets.label() != "none")
      event.getByLabel(level.m_jets, h_jets);
    if (level.m_tracks.label() != "none")
      event.getByLabel(level.m_tracks, h_tracks);
    
    if (passed) {
      // event did pass this filter, analyze the content
      ++m_events[l];

      if (h_jets.isValid()) {
        const edm::View<reco::Jet> & jets = * h_jets;
        for (unsigned int j = 0; j < jets.size(); ++j) {
          const reco::Jet & jet = jets[j];
          
          // match to MC parton
          int m = closestJet(jet, mcPartons, m_mcRadius);
          unsigned int flavour = (m != -1) ? abs(mcPartons[m].second.getFlavour()) : 0;

          // match to offline reconstruted b jets
          int o = closestJet(jet, offlineBJets, m_offlineRadius);
          double discriminator = (o != -1) ? offlineBJets[o].second : -INFINITY;

          if (not h_tracks.isValid()) {
            // no tracks, fill only the jets
            m_jetPlots[l].fill( jet );
            m_mcPlots[l].fill( jet, flavour);
            m_offlinePlots[l].fill( jet, discriminator );
          } else {
            // fill jets and tracks
            const reco::TrackRefVector & tracks = (*h_tracks)[jets.refAt(j)];
            m_jetPlots[l].fill( jet, tracks );
            m_mcPlots[l].fill( jet, tracks, flavour);
            m_offlinePlots[l].fill( jet, tracks, discriminator );
          }
        }
      }
    } else { 
      // event did not pass this filter, no need to check the following ones
      break;
    }
  }
  #ifdef DEBUG
  std::cout << std::endl;
  #endif
}

void HLTBtagLifetimeAnalyzer::endJob()
{
  // compute and print overall per-event efficiencies
  std::cout << m_triggerPath << " HLT Trigger path" << std::endl << std::endl;
  for (unsigned int i = 0; i < m_levels.size(); ++i) {
    std::stringstream out;
    out << std::setw(64) << std::left << ("events passing " + m_levels[i].m_title) << std::right << std::setw(12) << m_events[i];
    std::cout << m_triggerPath << ":" << out.str() << std::endl;
  }
  for (unsigned int i = 1; i < m_levels.size(); ++i) {
    std::stringstream out;
    out << std::setw(64) << std::left << ("step efficiency at " + m_levels[i].m_title);
    if (m_events[i-1] > 0) {
      double eff = (double) m_events[i] / (double) m_events[i-1];
      out << std::right << std::setw(11) << std::fixed << std::setprecision(2) << eff * 100. << "%";
    } else {
      out << std::right << std::setw(12) << "NaN";
    }
    std::cout << m_triggerPath << ":" << out.str() << std::endl;
  }
  for (unsigned int i = 1; i < m_levels.size(); ++i) {
    std::stringstream out;
    out << std::setw(64) << std::left << ("cumulative efficiency at " + m_levels[i].m_title);
    if (m_events[0] > 0) {
      double eff = (double) m_events[i] / (double) m_events[0];
      out << std::right << std::setw(11) << std::fixed << std::setprecision(2) << eff * 100. << "%";
    } else {
      out << std::right << std::setw(12) << "NaN";
    }
    std::cout << m_triggerPath << ":" << out.str() << std::endl;
  }
  std::cout << std::endl;
  
  TFile * file = new TFile(m_outputFile.c_str(), "UPDATE");
  TDirectory * dir = file->mkdir( m_triggerPath.c_str(), (m_triggerPath + " HLT path").c_str() );
  if (dir) {
    for (unsigned int i = 0; i < m_levels.size(); ++i) {
      m_jetPlots[i].save(*dir);
      m_mcPlots[i].save(*dir);
      m_offlinePlots[i].save(*dir);
    }
    for (unsigned int i = 1; i < m_levels.size(); ++i) {
      // make step-by-step efficiency plots
      m_jetPlots[i].efficiency( m_jetPlots[i-1] ).save(*dir);
      m_mcPlots[i].efficiency( m_mcPlots[i-1] ).save(*dir);
      m_offlinePlots[i].efficiency( m_offlinePlots[i-1] ).save(*dir);
    }
    for (unsigned int i = 2; i < m_levels.size(); ++i) {
      // make overall plots
      m_jetPlots[i].efficiency( m_jetPlots[0] ).save(*dir);
      m_mcPlots[i].efficiency( m_mcPlots[0] ).save(*dir);
      m_offlinePlots[i].efficiency( m_offlinePlots[0] ).save(*dir);
    }

    m_vertexPlots.save(*dir);
  }

  file->Write();
  file->Close();
}


#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(HLTBtagLifetimeAnalyzer);
