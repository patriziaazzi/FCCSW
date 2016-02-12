#include "DelphesSimulation.h"

#include <limits>
#include "ParticleStatus.h"

// Delphes
#include "modules/Delphes.h"
#include "classes/DelphesModule.h"
#include "classes/DelphesClasses.h"
#include "classes/DelphesFactory.h"
#include "DelphesExtHepMCReader.h"
#include "ExRootAnalysis/ExRootConfReader.h"
#include "ExRootAnalysis/ExRootTask.h"
#include "ExRootAnalysis/ExRootTreeWriter.h"
#include "ExRootAnalysis/ExRootTreeBranch.h"

// FCC EDM
#include "datamodel/MCParticleCollection.h"
#include "datamodel/GenVertexCollection.h"
#include "datamodel/GenJetCollection.h"
#include "datamodel/GenJetParticleAssociationCollection.h"
#include "datamodel/GenJetTagAssociationCollection.h"
#include "datamodel/ParticleCollection.h"
#include "datamodel/ParticleMCParticleAssociationCollection.h"
#include "datamodel/TagCollection.h"
#include "datamodel/METCollection.h"


// ROOT
#include "TFile.h"
#include "TObjArray.h"
#include "TStopwatch.h"

DECLARE_COMPONENT(DelphesSimulation)

DelphesSimulation::DelphesSimulation(const std::string& name, ISvcLocator* svcLoc):
GaudiAlgorithm(name, svcLoc) ,
  m_DelphesCard(),
  m_Delphes(nullptr),
  m_DelphesFactory(nullptr),
  m_HepMCReader(nullptr),
  m_inHepMCFile(nullptr),
  m_inHepMCFileName(""),
  m_inHepMCFileLength(0),
  m_eventCounter(0),
  m_outRootFile(nullptr),
  m_outRootFileName(""),
  m_treeWriter(nullptr),
  m_branchEvent(nullptr),
  m_confReader(nullptr),
  m_stablePartOutArray(nullptr),
  m_allPartOutArray(nullptr),
  m_partonOutArray(nullptr),
  m_muonOutArray(nullptr),
  m_electronOutArray(nullptr),
  m_chargedOutArray(nullptr),
  m_neutralOutArray(nullptr),
  m_photonOutArray(nullptr),
  m_jetOutArray(nullptr),
  m_metOutArray(nullptr),
  m_shtOutArray(nullptr) {

  //declareProperty("filename", m_filename="" , "Name of the HepMC file to read");
  declareProperty("DelphesCard"      , m_DelphesCard              , "Name of Delphes tcl config file with detector and simulation parameters");
  declareProperty("HepMCInputFile"   , m_inHepMCFileName          , "Name of HepMC input file; if defined, file read in / if not, data read in directly from the transient data store");
  declareProperty("ROOTOutputFile"   , m_outRootFileName          , "Name of Delphes Root output file, if defined, the Delphes standard tree write out (in addition to FCC-EDM based output to transient data store)");
  declareProperty("MuonsOutArray"    , m_DelphesMuonsArrayName    , "Name of Delphes muons array to be written out to FCC-EDM");
  declareProperty("ElectronsOutArray", m_DelphesElectronsArrayName, "Name of Delphes electrons array to be written out to FCC-EDM");
  declareProperty("ChargedOutArray"  , m_DelphesChargedArrayName  , "Name of Delphes charged hadrons array to be written out to FCC-EDM");
  declareProperty("NeutralOutArray"  , m_DelphesNeutralArrayName  , "Name of Delphes neutral hadrons array to be written out to FCC-EDM");
  declareProperty("PhotonsOutArray"  , m_DelphesPhotonsArrayName  , "Name of Delphes photons array to be written out to FCC-EDM");
  declareProperty("JetsOutArray"     , m_DelphesJetsArrayName     , "Name of Delphes jets array to be written out to FCC-EDM");
  declareProperty("METsOutArray"     , m_DelphesMETsArrayName     , "Name of Delphes METs array to be written out to FCC-EDM");
  declareProperty("SHTsOutArray"     , m_DelphesSHTsArrayName     , "Name of Delphes Scalar HTs array to be written out to FCC-EDM");

  declareInput("hepmc", m_hepmcHandle);
   
  declareOutput("genParticles"      , m_handleGenParticles);
  declareOutput("genVertices"       , m_handleGenVertices);
  declareOutput("recMuons"          , m_handleRecMuons);
  declareOutput("recElectrons"      , m_handleRecElectrons);
  declareOutput("recCharged"        , m_handleRecCharged);
  declareOutput("recNeutral"        , m_handleRecNeutral);
  declareOutput("recPhotons"        , m_handleRecPhotons);
  declareOutput("recJets"           , m_handleRecJets);
  declareOutput("recBTags"          , m_handleRecBTags);
  declareOutput("recTauTags"        , m_handleRecTauTags);
  declareOutput("recMETs"           , m_handleRecMETs);

  declareOutput("recMuonsToMC"      , m_handleRecMuonsToMC);
  declareOutput("recElectronsToMC"  , m_handleRecElectronsToMC);
  declareOutput("recChargedToMC"    , m_handleRecChargedToMC);
  declareOutput("recNeutralToMC"    , m_handleRecNeutralToMC);
  declareOutput("recPhotonsToMC"    , m_handleRecPhotonsToMC);
  declareOutput("recJetsToMC"       , m_handleRecJetsToMC);
  declareOutput("recJetsToBTags"    , m_handleRecJetsToBTags);
  declareOutput("recJetsToTauTags"  , m_handleRecJetsToTauTags);

}

StatusCode DelphesSimulation::initialize() {

  // Open HepMC file if defined
  if (m_inHepMCFileName!="") {

    info()  << "Reading in HepMC file: " << m_inHepMCFileName << endmsg;
    m_inHepMCFile = fopen(m_inHepMCFileName.c_str(), "r");

    if (m_inHepMCFile==nullptr) {

      error() << "Can't open " << m_inHepMCFileName << endmsg;
      return Error ("ERROR, can't open defined HepMC input file.");
    }
  
    fseek(m_inHepMCFile, 0L, SEEK_END);
    m_inHepMCFileLength = ftello(m_inHepMCFile);
    fseek(m_inHepMCFile, 0L, SEEK_SET);
    info() << "Length of HepMC input file: " << m_inHepMCFileLength << endmsg;
    if (m_inHepMCFileLength<=0) {
  
      fclose(m_inHepMCFile);
      return Error ("ERROR, zero length HepMC input file.");
    }
  }
  
  // If required, export output directly to root file
  if (m_outRootFileName!="") {

    info()  << "Opening ROOT output file: " << m_outRootFileName << endmsg;
    m_outRootFile = new TFile(m_outRootFileName.c_str(), "RECREATE");
    if (m_outRootFile->IsZombie()) {

      error() << "Can't open " << m_outRootFileName << endmsg;
      return Error ("ERROR, can't open defined ROOT output file.");
    }
  }

  // Read Delphes configuration card (deleted by finalize())
  m_confReader = new ExRootConfReader;
  m_confReader->ReadFile(m_DelphesCard.c_str());
   
  // Instance of Delphes (deleted by finalize())
  m_Delphes = new Delphes("Delphes");
  m_Delphes->SetConfReader(m_confReader);

  // Get standard Delphes factory (deleted by finalize())
  m_DelphesFactory = m_Delphes->GetFactory();

  // Delphes needs data structure to be defined (ROOT tree) (deleted by finalize())
  m_treeWriter  = new ExRootTreeWriter( m_outRootFile , "DelphesSim");
  m_branchEvent = m_treeWriter->NewBranch("Event", HepMCEvent::Class());
  m_Delphes->SetTreeWriter(m_treeWriter);

  // Define event readers
  //
  //  HepMC reader --> reads either from a file or directly from data store (deleted by finalize())
  m_HepMCReader = new DelphesExtHepMCReader;
  if (m_inHepMCFile) m_HepMCReader->SetInputFile(m_inHepMCFile);
  
  // Create following arrays of Delphes objects --> starting objects
  m_allPartOutArray    = m_Delphes->ExportArray("allParticles");
  m_stablePartOutArray = m_Delphes->ExportArray("stableParticles");
  m_partonOutArray     = m_Delphes->ExportArray("partons");

  // Init Delphes - read in configuration & define modules to be executed
  m_Delphes->InitTask();

  // Print Delphes modules to be used
  ExRootConfParam param = m_confReader->GetParam("::ExecutionPath");
  Long_t          size  = param.GetSize();
  info()  << "Delphes simulation will use the following modules: " << endmsg;
  for( Long_t k = 0; k < size; ++k) {

    TString name = param[k].GetString();
    info()  << "-- Module: " <<  name << endmsg;
  }
  
  // Initialize all variables
  m_muonOutArray     = nullptr;
  m_electronOutArray = nullptr;
  m_chargedOutArray  = nullptr;
  m_neutralOutArray  = nullptr;
  m_photonOutArray   = nullptr;
  m_jetOutArray      = nullptr;
  m_metOutArray      = nullptr;
  m_shtOutArray      = nullptr;

  m_eventCounter     = 0;

  if (m_outRootFile!=nullptr) m_treeWriter->Clear();
  m_Delphes->Clear();
  m_HepMCReader->Clear();
 
  return StatusCode::SUCCESS;
}


StatusCode DelphesSimulation::execute() {

  //
  // Read event & initialize event variables
  TStopwatch readStopWatch;
  readStopWatch.Start();

  bool isEventReady = false;

  if (m_inHepMCFile) {

    // Test end-of-file
    if ( ftello(m_inHepMCFile) == m_inHepMCFileLength) {

      info() << "End of file reached at lenght " << m_inHepMCFileLength << endmsg;
      return StatusCode::SUCCESS;
    }

    // Read event - read line-by-line until event complete
    isEventReady = m_HepMCReader->ReadEventFromFile(m_DelphesFactory, m_allPartOutArray, m_stablePartOutArray, m_partonOutArray);
  }
  else {

    // Read event
    const HepMC::GenEvent *hepMCEvent = m_hepmcHandle.get();
    isEventReady = m_HepMCReader->ReadEventFromStore(hepMCEvent, m_DelphesFactory, m_allPartOutArray, m_stablePartOutArray, m_partonOutArray);

    // Print debug: HepMC event info
    if (msgLevel() <= MSG::DEBUG) {

      for (auto ipart=hepMCEvent->particles_begin(); ipart!=hepMCEvent->particles_end(); ++ipart) {

        int motherID        = 0;
        int motherIDRange   = 0;
        int daughterID      = 0;
        int daughterIDRange = 0;
        if ((*ipart)->production_vertex()!=nullptr) {

          motherID      = (*((*ipart)->production_vertex()->particles_in_const_begin()))->barcode();
          motherIDRange = (*ipart)->production_vertex()->particles_in_size() -1;
        }
        if ((*ipart)->end_vertex()!=nullptr) {

          daughterID      = (*((*ipart)->end_vertex()->particles_out_const_begin()))->barcode();
          daughterIDRange = (*ipart)->end_vertex()->particles_out_size() -1;
        }

        debug() << "HepMC: "
                << " Id: "       << std::setw(3)  << (*ipart)->barcode()
                << " Pdg: "      << std::setw(5)  << (*ipart)->pdg_id()
                << " Mothers: "  << std::setw(4)  << motherID   << " -> " << std::setw(4) << motherID  +motherIDRange
                << " Daughters: "<< std::setw(4)  << daughterID << " -> " << std::setw(4) << daughterID+daughterIDRange
                << " Stat: "     << std::setw(2)  << (*ipart)->status()
                << std::scientific
                << " Px: "       << std::setprecision(2) << std::setw(9) << (*ipart)->momentum().px()
                << " Py: "       << std::setprecision(2) << std::setw(9) << (*ipart)->momentum().py()
                << " Pz: "       << std::setprecision(2) << std::setw(9) << (*ipart)->momentum().pz()
                << " E: "        << std::setprecision(2) << std::setw(9) << (*ipart)->momentum().e()
                << " M: "        << std::setprecision(2) << std::setw(9) << (*ipart)->momentum().m();
        if ((*ipart)->production_vertex()!=nullptr) {
          debug() << " Vx: "       << std::setprecision(2) << std::setw(9) << (*ipart)->production_vertex()->position().x()
                  << " Vy: "       << std::setprecision(2) << std::setw(9) << (*ipart)->production_vertex()->position().y()
                  << " Vz: "       << std::setprecision(2) << std::setw(9) << (*ipart)->production_vertex()->position().z()
                  << " T: "        << std::setprecision(2) << std::setw(9) << (*ipart)->production_vertex()->position().t();
        }
        debug() << std::fixed << endmsg;
      }
    } // Debug
  }

  if (!isEventReady) return StatusCode::FAILURE;

  // Print debug: Delphes event info
  if (msgLevel() <= MSG::DEBUG) {

    for (auto i=0; i<m_allPartOutArray->GetEntries(); i++) {

      Candidate *candidate = static_cast<Candidate *>(m_allPartOutArray->At(i));

      debug() << "DelphesMC: "
              << " Id: "       << std::setw(3)  << i+1
              << " Pdg: "      << std::setw(5)  << candidate->PID
              << " Mothers: "  << std::setw(4)  << candidate->M1+1 << " -> " << std::setw(4) << candidate->M2+1
              << " Daughters: "<< std::setw(4)  << candidate->D1+1 << " -> " << std::setw(4) << candidate->D2+1
              << " Stat: "     << std::setw(2)  << candidate->Status
              << std::scientific
              << " Px: "       << std::setprecision(2) << std::setw(9) << candidate->Momentum.Px()
              << " Py: "       << std::setprecision(2) << std::setw(9) << candidate->Momentum.Py()
              << " Pz: "       << std::setprecision(2) << std::setw(9) << candidate->Momentum.Pz()
              << " E: "        << std::setprecision(2) << std::setw(9) << candidate->Momentum.E()
              << " M: "        << std::setprecision(2) << std::setw(9) << candidate->Mass
              << " Vx: "       << std::setprecision(2) << std::setw(9) << candidate->Position.X()
              << " Vy: "       << std::setprecision(2) << std::setw(9) << candidate->Position.Y()
              << " Vz: "       << std::setprecision(2) << std::setw(9) << candidate->Position.Z()
              << " T: "        << std::setprecision(2) << std::setw(9) << candidate->Position.T()
              << std::fixed
              << endmsg;
    }
  } // Debug

  m_eventCounter++;
  readStopWatch.Stop();

  //
  // Process event
  TStopwatch procStopWatch;

  // Delphes process
  procStopWatch.Start();
  m_Delphes->ProcessTask();
  procStopWatch.Stop();

  // Generate Delphes branch: Event
  m_HepMCReader->MakeEventBranch(m_branchEvent, &readStopWatch, &procStopWatch);
  if (m_outRootFile!=nullptr) m_treeWriter->Fill();

  // FCC EDM (event-data model) based output
  auto genParticles     = new fcc::MCParticleCollection();
  auto genVertices      = new fcc::GenVertexCollection();
  auto recMuons         = new fcc::ParticleCollection();
  auto recElectrons     = new fcc::ParticleCollection();
  auto recCharged       = new fcc::ParticleCollection();
  auto recNeutral       = new fcc::ParticleCollection();
  auto recPhotons       = new fcc::ParticleCollection();
  auto recJets          = new fcc::GenJetCollection();
  auto recBTags         = new fcc::TagCollection();
  auto recTauTags       = new fcc::TagCollection();
  auto recMETs          = new fcc::METCollection();

  auto recMuonsToMC     = new fcc::ParticleMCParticleAssociationCollection();
  auto recElectronsToMC = new fcc::ParticleMCParticleAssociationCollection();
  auto recChargedToMC   = new fcc::ParticleMCParticleAssociationCollection();
  auto recNeutralToMC   = new fcc::ParticleMCParticleAssociationCollection();
  auto recPhotonsToMC   = new fcc::ParticleMCParticleAssociationCollection();
  auto recJetsToMC      = new fcc::GenJetParticleAssociationCollection();
  auto recJetsToBTags   = new fcc::GenJetTagAssociationCollection();
  auto recJetsToTauTags = new fcc::GenJetTagAssociationCollection();

  // Fill FCC collections
  m_muonOutArray     = m_Delphes->ImportArray(m_DelphesMuonsArrayName.c_str());     // "MuonMomentumSmearing/muons" / "MuonIsolation/muons"
  m_electronOutArray = m_Delphes->ImportArray(m_DelphesElectronsArrayName.c_str()); // "ElectronEnergySmearing/electrons" / "ElectronIsolation/electrons"
  m_chargedOutArray  = m_Delphes->ImportArray(m_DelphesChargedArrayName.c_str());   // "ChargedHadronMomentumSmearing/chargedHadrons"
  m_neutralOutArray  = m_Delphes->ImportArray(m_DelphesNeutralArrayName.c_str());   // "Hcal/eflowNeutralHadrons"
  m_photonOutArray   = m_Delphes->ImportArray(m_DelphesPhotonsArrayName.c_str());   // "PhotonEfficiency/photons" / "PhotonIsolation/photons"
  m_jetOutArray      = m_Delphes->ImportArray(m_DelphesJetsArrayName.c_str());      // "JetEnergyScale/jets"
  m_metOutArray      = m_Delphes->ImportArray(m_DelphesMETsArrayName.c_str());      // "MissingET/momentum"
  m_shtOutArray      = m_Delphes->ImportArray(m_DelphesSHTsArrayName.c_str());      // "ScalarHT/energy"

  if (m_muonOutArray    ==nullptr) warning () << "Can't save Delphes muon array: "
                                              << m_DelphesMuonsArrayName
                                              << " to FCCEDM. Doesn't exist!!!";
  if (m_electronOutArray==nullptr) warning () << "Can't save Delphes electron array: "
                                              << m_DelphesElectronsArrayName
                                              << " to FCCEDM. Doesn't exist!!!";
  if (m_chargedOutArray ==nullptr) warning () << "Can't save Delphes charged hadron array: "
                                              << m_DelphesChargedArrayName
                                              << " to FCCEDM. Doesn't exist!!!";
  if (m_neutralOutArray ==nullptr) warning () << "Can't save Delphes neutral hadron array: "
                                              << m_DelphesNeutralArrayName
                                              << " to FCCEDM. Doesn't exist!!!";
  if (m_photonOutArray  ==nullptr) warning () << "Can't save Delphes photon array: "
                                              << m_DelphesPhotonsArrayName
                                              << " to FCCEDM. Doesn't exist!!!";
  if (m_jetOutArray     ==nullptr) warning () << "Can't save Delphes jet array: "
                                              << m_DelphesJetsArrayName
                                              << " to FCCEDM. Doesn't exist!!!";
  if (m_metOutArray     ==nullptr) warning () << "Can't save Delphes MET array: "
                                              << m_DelphesMETsArrayName
                                              << " to FCCEDM. Doesn't exist!!!";
  if (m_shtOutArray     ==nullptr) warning () << "Can't save Delphes Scalar HT array: "
                                              << m_DelphesSHTsArrayName
                                              << " to FCCEDM. Doesn't exist!!!";

  if (m_allPartOutArray !=nullptr) DelphesSimulation::ConvertMCParticles(m_allPartOutArray , genParticles  , genVertices);
  if (m_muonOutArray    !=nullptr) DelphesSimulation::ConvertTracks(     m_muonOutArray    , genParticles  , recMuons    , recMuonsToMC);
  if (m_electronOutArray!=nullptr) DelphesSimulation::ConvertTracks(     m_electronOutArray, genParticles  , recElectrons, recElectronsToMC);
  if (m_chargedOutArray !=nullptr) DelphesSimulation::ConvertTracks(     m_chargedOutArray , genParticles  , recCharged  , recChargedToMC  );
  if (m_neutralOutArray !=nullptr) DelphesSimulation::ConvertTowers(     m_neutralOutArray , genParticles  , recNeutral  , recNeutralToMC  );
  if (m_photonOutArray  !=nullptr) DelphesSimulation::ConvertTowers(     m_photonOutArray  , genParticles  , recPhotons  , recPhotonsToMC  );
  if (m_jetOutArray     !=nullptr) DelphesSimulation::ConvertJets(       m_jetOutArray     , genParticles  , recJets     , recJetsToMC,
                                                                                                             recBTags    , recJetsToBTags,
                                                                                                             recTauTags  , recJetsToTauTags);
  if (m_metOutArray     !=nullptr && m_shtOutArray!=nullptr) DelphesSimulation::ConvertMET(m_metOutArray, m_shtOutArray, recMETs);

  // Save FCC-EDM collections to FCCSw data store
  m_handleGenParticles.put(    genParticles    );
  m_handleGenVertices.put(     genVertices     );
  m_handleRecMuons.put(        recMuons        );
  m_handleRecMuonsToMC.put(    recMuonsToMC    );
  m_handleRecElectrons.put(    recElectrons    );
  m_handleRecElectronsToMC.put(recElectronsToMC);
  m_handleRecCharged.put(      recCharged      );
  m_handleRecChargedToMC.put(  recChargedToMC  );
  m_handleRecNeutral.put(      recNeutral      );
  m_handleRecNeutralToMC.put(  recNeutralToMC  );
  m_handleRecPhotons.put(      recPhotons      );
  m_handleRecPhotonsToMC.put(  recPhotonsToMC  );
  m_handleRecJets.put(         recJets         );
  m_handleRecJetsToMC.put(     recJetsToMC     );
  m_handleRecBTags.put(        recBTags        );
  m_handleRecJetsToBTags.put(  recJetsToBTags  );
  m_handleRecTauTags.put(      recTauTags      );
  m_handleRecJetsToTauTags.put(recJetsToTauTags);
  m_handleRecMETs.put(         recMETs         );

  // Initialize for next event reading (Will also zero Delphes arrays)
  if (m_outRootFile!=nullptr) m_treeWriter->Clear();
  m_Delphes->Clear();
  m_HepMCReader->Clear();

  return StatusCode::SUCCESS;
}

StatusCode DelphesSimulation::finalize() {

  // Finish Delphes task
  m_Delphes->FinishTask();

  // Close HepMC input file if defined
  if (m_inHepMCFile!=nullptr) {

    fclose(m_inHepMCFile);
  }

  // Write output to Root file
  if (m_outRootFile!=nullptr) {

    m_treeWriter->Write();
    m_outRootFile->Close();
    if (m_outRootFile!=nullptr) {delete m_outRootFile; m_outRootFile = nullptr;}
  }
  
  info() << "Exiting Delphes..." << endmsg;
  
  // Clear memory
  if (m_HepMCReader!=nullptr) {delete m_HepMCReader; m_HepMCReader = nullptr; } // Releases also the memory allocated by inHepMCFile
  if (m_Delphes    !=nullptr) {delete m_Delphes;     m_Delphes     = nullptr; } // Releases also the memory allocated by treeWriter
  if (m_confReader !=nullptr) {delete m_confReader;  m_confReader  = nullptr; }
  
  return GaudiAlgorithm::finalize();
}

//
// Convert internal Delphes objects: MCParticles to FCC EDM: MCParticle & GenVertices
//
void DelphesSimulation::ConvertMCParticles(const TObjArray* Input,
                                           fcc::MCParticleCollection* colMCParticles,
                                           fcc::GenVertexCollection* colGenVertices) {

  //Initialize MC particle vertex mapping: production & decay vertex
  std::vector<std::pair<int, int>> vecPartProdVtxIDDecVtxID;

  vecPartProdVtxIDDecVtxID.resize(Input->GetEntries());
  for(int j=0; j<Input->GetEntries(); j++) {
    vecPartProdVtxIDDecVtxID[j].first  = -1;
    vecPartProdVtxIDDecVtxID[j].second = -1;
  }

  // Find true daughters of the colliding particles (necessary fix for missing links
  // between primary colliding particles and their daughters if LHE file used within Pythia)
  std::set<int> primary1Daughters;
  std::set<int> primary2Daughters;

  for(int j=0; j<Input->GetEntries(); j++) {

    auto cand = static_cast<Candidate *>(m_allPartOutArray->At(j));

    // Go through all not primary particles
    if (cand->M1!=-1) {
      for (int iMother=cand->M1; iMother<=cand->M2; iMother++) {

        if (iMother==0) primary1Daughters.insert(j);
        if (iMother==1) primary2Daughters.insert(j);
      }
    }
  } // Fix

  // Save MC particles and vertices
  for(int j=0; j<Input->GetEntries(); j++) {

    auto cand     = static_cast<Candidate *>(m_allPartOutArray->At(j));
    auto particle = colMCParticles->create();

    auto barePart     = fcc::BareParticle();
    barePart.Type     = cand->PID;
    barePart.Status   = cand->Status;
    barePart.P4.Px    = cand->Momentum.Px();
    barePart.P4.Py    = cand->Momentum.Py();
    barePart.P4.Pz    = cand->Momentum.Pz();
    barePart.P4.Mass  = cand->Momentum.M();
    barePart.Charge   = cand->Charge;
    barePart.Vertex.X = cand->Position.X();
    barePart.Vertex.Y = cand->Position.Y();
    barePart.Vertex.Z = cand->Position.Z();

    if (cand->M1==-1)      barePart.Bits = static_cast<unsigned>(ParticleStatus::kBeam);
    else if (cand->D1==-1) barePart.Bits = static_cast<unsigned>(ParticleStatus::kStable);
    else                   barePart.Bits = static_cast<unsigned>(ParticleStatus::kDecayed);

    particle.Core(barePart);

    // Mapping the vertices
    int& idPartStartVertex = vecPartProdVtxIDDecVtxID[j].first;
    int& idPartEndVertex   = vecPartProdVtxIDDecVtxID[j].second;

    // Production vertex
    if (cand->M1!=-1) {
      if (idPartStartVertex!=-1) {
        particle.StartVertex(colMCParticles->at(idPartStartVertex).EndVertex());
      }
      else {
        fcc::Point point;
        point.X = cand->Position.X();
        point.Y = cand->Position.Y();
        point.Z = cand->Position.Z();

        auto vertex = colGenVertices->create();
        vertex.Position(point);
        vertex.Ctau(cand->Position.T());
        particle.StartVertex(vertex);

        idPartStartVertex = j;
      }
      for (int iMother=cand->M1; iMother<=cand->M2; iMother++) {
        if (vecPartProdVtxIDDecVtxID[iMother].second==-1) vecPartProdVtxIDDecVtxID[iMother].second = j;
      }
    }
    // Decay vertex
    if (cand->D1!=-1) {
      Candidate* daughter  = static_cast<Candidate *>(Input->At(cand->D1));

      if (idPartEndVertex!=-1) {
        particle.EndVertex(colMCParticles->at(idPartEndVertex).StartVertex());
      }
      else {
        fcc::Point point;
        point.X  = daughter->Position.X();
        point.Y  = daughter->Position.Y();
        point.Z  = daughter->Position.Z();

        auto vertex = colGenVertices->create();
        vertex.Position(point);
        vertex.Ctau(cand->Position.T());
        particle.EndVertex(vertex);

        idPartEndVertex = cand->D1;
      }

      // Option for colliding particles -> broken daughters range -> use only D1, which is correctly set (D2 & D2-D1 is wrong!!!)
      if (cand->M1==-1) {

        // Primary particle 0 correction
        if (j==0) for (const int& iDaughter : primary1Daughters) {

          if (iDaughter>=0 && vecPartProdVtxIDDecVtxID[iDaughter].first==-1) vecPartProdVtxIDDecVtxID[iDaughter].first = j;
        }
        // Primary particle 1 correction
        else if (j==1) for (const int& iDaughter : primary2Daughters) {

          if (iDaughter>=0 && vecPartProdVtxIDDecVtxID[iDaughter].first==-1) vecPartProdVtxIDDecVtxID[iDaughter].first = j;
        }
      }
      // Option for all other particles
      else {
        for (int iDaughter=cand->D1; iDaughter<=cand->D2; iDaughter++) {

          if (iDaughter>=0 && vecPartProdVtxIDDecVtxID[iDaughter].first==-1) vecPartProdVtxIDDecVtxID[iDaughter].first = j;
        }
      }
    }

    // Debug: print FCC-EDM MCParticle and GenVertex
    if (msgLevel() <= MSG::DEBUG) {

      double partE = sqrt(particle.Core().P4.Px*particle.Core().P4.Px +
                          particle.Core().P4.Py*particle.Core().P4.Py +
                          particle.Core().P4.Pz*particle.Core().P4.Pz +
                          particle.Core().P4.Mass*particle.Core().P4.Mass);

      debug() << "MCParticle: "
              << " Id: "       << std::setw(3)  << j+1
              << " Pdg: "      << std::setw(5)  << particle.Core().Type
              << " Stat: "     << std::setw(2)  << particle.Core().Status
              << " Bits: "     << std::setw(2)  << particle.Core().Bits
              << std::scientific
              << " Px: "       << std::setprecision(2) << std::setw(9) << particle.Core().P4.Px
              << " Py: "       << std::setprecision(2) << std::setw(9) << particle.Core().P4.Py
              << " Pz: "       << std::setprecision(2) << std::setw(9) << particle.Core().P4.Pz
              << " E: "        << std::setprecision(2) << std::setw(9) << partE
              << " M: "        << std::setprecision(2) << std::setw(9) << particle.Core().P4.Mass;
      if (particle.StartVertex().isAvailable()) {
        debug() << " VSId: "     << std::setw(3)  << vecPartProdVtxIDDecVtxID[j].first+1;
                //<< " Vx: "       << std::setprecision(2) << std::setw(9) << particle.StartVertex().Position().X
                //<< " Vy: "       << std::setprecision(2) << std::setw(9) << particle.StartVertex().Position().Y
                //<< " Vz: "       << std::setprecision(2) << std::setw(9) << particle.StartVertex().Position().Z
                //<< " T: "        << std::setprecision(2) << std::setw(9) << particle.StartVertex().Ctau();
      }
      if (particle.EndVertex().isAvailable()) {
        debug() << " VEId: "     << std::setw(3)  << vecPartProdVtxIDDecVtxID[j].second+1;
                //<< " Vx: "       << std::setprecision(2) << std::setw(9) << particle.EndVertex().Position().X
                //<< " Vy: "       << std::setprecision(2) << std::setw(9) << particle.EndVertex().Position().Y
                //<< " Vz: "       << std::setprecision(2) << std::setw(9) << particle.EndVertex().Position().Z
                //<< " T: "        << std::setprecision(2) << std::setw(9) << particle.EndVertex().Ctau();
      }
      debug() << std::fixed << endmsg;

    } // Debug
  }
}   

//
// Convert internal Delphes objects: Muons, electrons, charged hadrons to FCC EDM: Particles & Particles<->MCParticles association
//
void DelphesSimulation::ConvertTracks(const TObjArray* Input,
                                      const fcc::MCParticleCollection* colMCParticles,
                                      fcc::ParticleCollection* colParticles,
                                      fcc::ParticleMCParticleAssociationCollection* ascColParticlesToMC) {

  for(int j=0; j<Input->GetEntries(); j++) {

    auto cand     = static_cast<Candidate *>(Input->At(j));
    auto particle = colParticles->create();

    auto barePart     = fcc::BareParticle();
    barePart.Type     = cand->PID;
    barePart.Status   = cand->Status;
    barePart.P4.Px    = cand->Momentum.Px();
    barePart.P4.Py    = cand->Momentum.Py();
    barePart.P4.Pz    = cand->Momentum.Pz();
    barePart.P4.Mass  = cand->Momentum.M();
    barePart.Charge   = cand->Charge;
    barePart.Vertex.X = cand->Position.X();
    barePart.Vertex.Y = cand->Position.Y();
    barePart.Vertex.Z = cand->Position.Z();

    // Reference to MC - Delphes holds references to all objects related to the <T> object, only one relates to MC particle
    auto relation   = ascColParticlesToMC->create();
    int idRefMCPart = -1;
    if (cand->GetCandidates()->GetEntries()>0) {

      auto refCand = static_cast<Candidate*>(cand->GetCandidates()->At(0));
      idRefMCPart  = refCand->GetUniqueID()-1;     // Use C numbering from 0
      if (idRefMCPart<colMCParticles->size()) {

        barePart.Bits = static_cast<unsigned>(ParticleStatus::kMatched);
        particle.Core(barePart);
        relation.Rec(particle);
        relation.Sim(colMCParticles->at(idRefMCPart));
      }
      else {
        barePart.Bits = static_cast<unsigned>(ParticleStatus::kUnmatched);
        particle.Core(barePart);
        warning() << "Can't build relation from Electron/Muon/ChHadron to MC particle!" << std::endl;
      }
    }
    else {
      barePart.Bits = static_cast<unsigned>(ParticleStatus::kUnmatched);
      particle.Core(barePart);
      warning() << "Can't build relation from Electron/Muon/ChHadron to MC particle!" << std::endl;
    }

    // Debug: print FCC-EDM track info
    if (msgLevel() <= MSG::DEBUG) {

      double energy = sqrt(particle.Core().P4.Px*particle.Core().P4.Px +
                           particle.Core().P4.Py*particle.Core().P4.Py +
                           particle.Core().P4.Pz*particle.Core().P4.Pz +
                           particle.Core().P4.Mass*particle.Core().P4.Mass);
      double recE   = sqrt(relation.Rec().Core().P4.Px*relation.Rec().Core().P4.Px +
                           relation.Rec().Core().P4.Py*relation.Rec().Core().P4.Py +
                           relation.Rec().Core().P4.Pz*relation.Rec().Core().P4.Pz +
                           relation.Rec().Core().P4.Mass*relation.Rec().Core().P4.Mass);
      double simE   = sqrt(relation.Sim().Core().P4.Px*relation.Sim().Core().P4.Px +
                           relation.Sim().Core().P4.Py*relation.Sim().Core().P4.Py +
                           relation.Sim().Core().P4.Pz*relation.Sim().Core().P4.Pz +
                           relation.Sim().Core().P4.Mass*relation.Sim().Core().P4.Mass);

      debug() << "Track: "
              << " Id: "       << std::setw(3)  << j+1
              << " Pdg: "      << std::setw(5)  << particle.Core().Type
              << " Stat: "     << std::setw(2)  << particle.Core().Status
              << " Bits: "     << std::setw(2)  << particle.Core().Bits
              << std::scientific
              << " Px: "       << std::setprecision(2) << std::setw(9) << particle.Core().P4.Px
              << " Py: "       << std::setprecision(2) << std::setw(9) << particle.Core().P4.Py
              << " Pz: "       << std::setprecision(2) << std::setw(9) << particle.Core().P4.Pz
              << " E: "        << std::setprecision(2) << std::setw(9) << energy
              << " M: "        << std::setprecision(2) << std::setw(9) << particle.Core().P4.Mass
              << " Vx: "       << std::setprecision(2) << std::setw(9) << particle.Core().Vertex.X
              << " Vy: "       << std::setprecision(2) << std::setw(9) << particle.Core().Vertex.Y
              << " Vz: "       << std::setprecision(2) << std::setw(9) << particle.Core().Vertex.Z
              << " RefId: "    << std::setw(3)  << idRefMCPart+1
              << " Rel E: "    << std::setprecision(2) << std::setw(9) << simE << " <-> " << std::setw(9) << recE
              << std::fixed
              << endmsg;
    } // Debug
  } // For - tracks
}

//
// Convert internal Delphes objects: Photons, neutral hadrons to FCC EDM: Particles & Particles<->MCParticles association
//
void DelphesSimulation::ConvertTowers(const TObjArray* Input,
                                      const fcc::MCParticleCollection* colMCParticles,
                                      fcc::ParticleCollection* colParticles,
                                      fcc::ParticleMCParticleAssociationCollection* ascColParticlesToMC) {

  for(int j=0; j<Input->GetEntries(); j++) {

    auto cand     = static_cast<Candidate *>(Input->At(j));
    auto particle = colParticles->create();

    auto barePart     = fcc::BareParticle();
    barePart.Type     = cand->PID;
    barePart.Status   = cand->Status;
    barePart.P4.Px    = cand->Momentum.Px();
    barePart.P4.Py    = cand->Momentum.Py();
    barePart.P4.Pz    = cand->Momentum.Pz();
    barePart.P4.Mass  = cand->Momentum.M();
    barePart.Charge   = cand->Charge;
    barePart.Vertex.X = cand->Position.X();
    barePart.Vertex.Y = cand->Position.Y();
    barePart.Vertex.Z = cand->Position.Z();
    particle.Core(barePart);

    // Debug: print FCC-EDM tower info
    if (msgLevel() <= MSG::DEBUG) {

      double energy = sqrt(particle.Core().P4.Px*particle.Core().P4.Px +
                           particle.Core().P4.Py*particle.Core().P4.Py +
                           particle.Core().P4.Pz*particle.Core().P4.Pz +
                           particle.Core().P4.Mass*particle.Core().P4.Mass);

      debug() << "Tower: "
              << " Id: "       << std::setw(3) << j+1
              << " Pdg: "      << std::setw(5) << particle.Core().Type
              << " Stat: "     << std::setw(2) << particle.Core().Status
              << " Bits: "     << std::setw(2) << particle.Core().Bits
              << std::scientific
              << " Px: "       << std::setprecision(2) << std::setw(9) << particle.Core().P4.Px
              << " Py: "       << std::setprecision(2) << std::setw(9) << particle.Core().P4.Py
              << " Pz: "       << std::setprecision(2) << std::setw(9) << particle.Core().P4.Pz
              << " E: "        << std::setprecision(2) << std::setw(9) << energy
              << " M: "        << std::setprecision(2) << std::setw(9) << particle.Core().P4.Mass
              << " Vx: "       << std::setprecision(2) << std::setw(9) << particle.Core().Vertex.X
              << " Vy: "       << std::setprecision(2) << std::setw(9) << particle.Core().Vertex.Y
              << " Vz: "       << std::setprecision(2) << std::setw(9) << particle.Core().Vertex.Z
              << std::fixed
              << std::endl;
    } // Debug

    // Reference to MC - Delphes holds references to all objects related to the Photon object, several relations might exist for gammas
    std::set<int> idRefMCPart; // Avoid double counting when referencingh MC particles

    // Get corresponding cluster in calorimeter
    for (auto itCand=cand->GetCandidates()->begin(); itCand!=cand->GetCandidates()->end(); ++itCand) {

      // Cluster in calorimeter
      Candidate* clsCand = static_cast<Candidate*>(*itCand);

      // Get corresponding MC particle
      for (auto itCls=clsCand->GetCandidates()->begin(); itCls!=clsCand->GetCandidates()->end(); ++itCls) {

        Candidate* refCand = static_cast<Candidate*>(*itCls);
        int id = refCand->GetUniqueID()-1;
        if (id<colMCParticles->size()) idRefMCPart.insert(id);
        else {
          warning() << "Can't build one of the relations from Photon/NHadron to MC particle!" << std::endl;
        }

      } // Iter MC particles
    } // Iter cluster

    // Debug: print variable
    double totSimE = 0;

    // Save relations
    for (auto id : idRefMCPart) {

      auto relation = ascColParticlesToMC->create();
      relation.Rec(particle);
      relation.Sim(colMCParticles->at(id));

      // Debug: print FCC-EDM tower relation info
      if (msgLevel() <= MSG::DEBUG) {
        double recE   = sqrt(relation.Rec().Core().P4.Px*relation.Rec().Core().P4.Px +
                             relation.Rec().Core().P4.Py*relation.Rec().Core().P4.Py +
                             relation.Rec().Core().P4.Pz*relation.Rec().Core().P4.Pz +
                             relation.Rec().Core().P4.Mass*relation.Rec().Core().P4.Mass);
        double simE   = sqrt(relation.Sim().Core().P4.Px*relation.Sim().Core().P4.Px +
                             relation.Sim().Core().P4.Py*relation.Sim().Core().P4.Py +
                             relation.Sim().Core().P4.Pz*relation.Sim().Core().P4.Pz +
                             relation.Sim().Core().P4.Mass*relation.Sim().Core().P4.Mass);

        totSimE += simE;
        debug() << " RefId: " << std::setw(3)            << id+1
                << " Rel E: " << std::setprecision(2)
                              << std::scientific
                              << std::setw(9) << simE    << " "
                              << std::setw(9) << totSimE << " <-> "
                              << std::setw(9) << recE
                              << std::fixed;
        if      (colMCParticles->at(id).Core().Type ==22) debug() << " Gamma";
        else if (colMCParticles->at(id).Core().Charge==0) debug() << " Neutral";
        debug() << std::endl;
      } // Debug
    }

    // Debug: print end-line
    if (msgLevel() <= MSG::DEBUG) debug() << endmsg;
  } // For - towers
}

//
// Convert internal Delphes objects: Jets to FCC EDM: GenJets & GenJets<->MCParticles association
//
void DelphesSimulation::ConvertJets(const TObjArray* Input,
                                    const fcc::MCParticleCollection* colMCParticles,
                                    fcc::GenJetCollection* colJets,
                                    fcc::GenJetParticleAssociationCollection* ascColJetsToMC,
                                    fcc::TagCollection* colBTags,
                                    fcc::GenJetTagAssociationCollection* ascColJetsToBTags,
                                    fcc::TagCollection* colTauTags,
                                    fcc::GenJetTagAssociationCollection* ascColJetsToTauTags) {

  for(int j = 0; j < Input->GetEntries(); ++j) {
      
    auto cand = static_cast<Candidate *>(Input->At(j));

    // Jet info
    auto jet         = colJets->create();
    auto bareJet     = fcc::BareJet();
    bareJet.Area     = -1;
    bareJet.P4.Px    = cand->Momentum.Px();
    bareJet.P4.Py    = cand->Momentum.Py();
    bareJet.P4.Pz    = cand->Momentum.Pz();
    bareJet.P4.Mass  = cand->Mass;
    jet.Core(bareJet);

    // B-tag info
    auto bTag             = colBTags->create();
    auto relationToBTag   = ascColJetsToBTags->create();
    bTag.Value(cand->BTag);
    relationToBTag.Jet(jet);
    relationToBTag.Tag(bTag);

    // Tau-tag info
    auto tauTag           = colTauTags->create();
    auto relationToTauTag = ascColJetsToTauTags->create();
    tauTag.Value(cand->TauTag);
    relationToTauTag.Jet(jet);
    relationToTauTag.Tag(tauTag);


    // Debug: print FCC-EDM jets info
    if (msgLevel() <= MSG::DEBUG) {

      double energy = sqrt(jet.Core().P4.Px*jet.Core().P4.Px +
                           jet.Core().P4.Py*jet.Core().P4.Py +
                           jet.Core().P4.Pz*jet.Core().P4.Pz +
                           jet.Core().P4.Mass*jet.Core().P4.Mass);

      debug() << "Jet: "
              << " Id: "       << std::setw(3)  << j+1
              << " BTag: "     << std::setprecision(1) << std::setw(3) << relationToBTag.Tag().Value()
              << " TauTag: "   << std::setprecision(1) << std::setw(3) << relationToTauTag.Tag().Value()
              << std::scientific
              << " Px: "       << std::setprecision(2) << std::setw(9) << jet.Core().P4.Px
              << " Py: "       << std::setprecision(2) << std::setw(9) << jet.Core().P4.Py
              << " Pz: "       << std::setprecision(2) << std::setw(9) << jet.Core().P4.Pz
              << " E: "        << std::setprecision(2) << std::setw(9) << energy
              << " M: "        << std::setprecision(2) << std::setw(9) << jet.Core().P4.Mass
              << std::fixed
              << std::endl;
    }

    // Reference to MC - Delphes holds references to all objects related to the Jet object,
    // several relations might exist -> find "recursively" in a tree history the MC particle
    std::set<int> idRefMCPart; // Avoid double counting when referencingh MC particles

    // Get corresponding jet constituents
    for (auto itCand=cand->GetCandidates()->begin(); itCand!=cand->GetCandidates()->end(); ++itCand) {

      // Jet constituent
      Candidate* jetPart = static_cast<Candidate*>(*itCand);

      // Get related MC particle recursively (different level of particle -> particle -> ... -> MC particle relations)
      // Add index to the reference index field to avoid double counting
      // Recursive procedure stops after the relation is to MC particle and not to a particle object or if particle not related to MC particle (<0 value)
      findJetPartMC(jetPart, colMCParticles->size(), idRefMCPart);
    } // Jet constituents

    // Debug: print variable
    double totSimE = 0;

    for (auto id : idRefMCPart) {

      auto relationToMC = ascColJetsToMC->create();
      relationToMC.Jet(jet);
      relationToMC.Particle(colMCParticles->at(id));

      // Debug: print FCC-EDM jet relation info
      if (msgLevel() <= MSG::DEBUG) {
        double recE   = sqrt(relationToMC.Jet().Core().P4.Px*relationToMC.Jet().Core().P4.Px +
                             relationToMC.Jet().Core().P4.Py*relationToMC.Jet().Core().P4.Py +
                             relationToMC.Jet().Core().P4.Pz*relationToMC.Jet().Core().P4.Pz +
                             relationToMC.Jet().Core().P4.Mass*relationToMC.Jet().Core().P4.Mass);
        double simE   = sqrt(relationToMC.Particle().Core().P4.Px*relationToMC.Particle().Core().P4.Px +
                             relationToMC.Particle().Core().P4.Py*relationToMC.Particle().Core().P4.Py +
                             relationToMC.Particle().Core().P4.Pz*relationToMC.Particle().Core().P4.Pz +
                             relationToMC.Particle().Core().P4.Mass*relationToMC.Particle().Core().P4.Mass);
        totSimE += simE;
        debug() << " RefId: " << std::setw(3)            << id+1
                << " Rel E: " << std::setprecision(2)
                              << std::scientific
                              << std::setw(9) << simE    << " "
                              << std::setw(9) << totSimE << " <-> "
                              << std::setw(9) << recE
                              << std::fixed
                              << std::endl;
      } // Debug
    }

    // Debug: print end-line
    if (msgLevel() <= MSG::DEBUG) debug() << endmsg;
  } // For - jets
}   

//
// Recursive method to find id of MCParticle related to the given jet Delphes Candidate object
//
void DelphesSimulation::findJetPartMC(Candidate* jetPart, int rangeMCPart, std::set<int>& idRefMCPart) {

  // Recursion depth - increase
  //static int depth = 0;
  //depth++;

  // Warning - no MC relation found
  if (jetPart->GetCandidates()->GetEntries()==0) {

    warning() << "Can't build one of the relations from Jet to MC particle!" << std::endl;
  }
  // Relation can be found
  else {

    for (auto itCand=jetPart->GetCandidates()->begin(); itCand!=jetPart->GetCandidates()->end(); ++itCand) {

      Candidate* refCand = static_cast<Candidate*>(*itCand);
      int id = refCand->GetUniqueID()-1;

      //std::cout << "Depth: " << depth << " " << id << std::endl;
      // Relation found
      if (id<rangeMCPart) {
        //std::cout << ">>> " << id << std::endl;
        idRefMCPart.insert(id);
      }
      // Not found -> step one level below
      else findJetPartMC(refCand, rangeMCPart, idRefMCPart);
    }
  }

  // Recursion depth - decrease
  //depth--;
}

//
// Convert internal Delphes objects: Missing ETs and scalar pT sums to FCC EDM: METs
//
void DelphesSimulation::ConvertMET(const TObjArray* InputMET,
                                   const TObjArray* InputSHT,
                                   fcc::METCollection* colMET) {

  bool saveSHT = true;
  if (InputMET->GetEntries()!=InputSHT->GetEntries()) {

    saveSHT = false;
    warning() << "Can't save in a common FCC-EDM MET object both information from Delphes MET & scalarHT. Only MET will be saved!" << std::endl;
  }

  for(int j = 0; j < InputMET->GetEntries(); ++j) {

    Candidate* candSHT = nullptr;

    auto candMET = static_cast<Candidate *>(InputMET->At(j));
    if (saveSHT) candSHT = static_cast<Candidate *>(InputSHT->At(j));

    auto met = colMET->create();

    met.Magnitude(candMET->Momentum.Pt());
    met.Phi((-(candMET->Momentum)).Phi());
    if (saveSHT) met.ScalarSum(candSHT->Momentum.Pt());
    else         met.ScalarSum(-1);

    // Debug: print FCC-EDM MET info
    if (msgLevel() <= MSG::DEBUG) {

      debug() << "MET Info: "
              << std::scientific
              << " MET: " << std::setprecision(2) << std::setw(9) << met.Magnitude()
              << " Phi: " << std::setprecision(2) << std::setw(9) << met.Phi()
              << " sHT: " << std::setprecision(2) << std::setw(9) << met.ScalarSum()
              << std::fixed
              << endmsg;
    } // Debug
  }
}   
