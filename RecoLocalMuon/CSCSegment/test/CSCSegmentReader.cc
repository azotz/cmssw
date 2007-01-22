/** \file CSCSegmentReader.cc
 *
 *  $Date: 2006/12/15 03:34:52 $
 *  $Revision: 1.9 $
 *  \author M. Sani
 *
 *  Modified by D. Fortin - UC Riverside
 */

#include <RecoLocalMuon/CSCSegment/test/CSCSegmentReader.h>

#include <FWCore/Framework/interface/MakerMacros.h>
#include <Geometry/Records/interface/MuonGeometryRecord.h>

#include <DataFormats/CSCRecHit/interface/CSCRecHit2DCollection.h>

#include <DataFormats/CSCRecHit/interface/CSCRangeMapAccessor.h>
#include <Geometry/CSCGeometry/interface/CSCChamber.h>

using namespace std;
using namespace edm;


// Constructor
CSCSegmentReader::CSCSegmentReader(const ParameterSet& pset) {

  simhit                    = 0;
  near_segment              = 0;
  filename                  = pset.getUntrackedParameter<string>("RootFileName");	
  minLayerWithRechitChamber = pset.getUntrackedParameter<int>("minLayerWithRechitPerChamber");
  minLayerWithSimhitChamber = pset.getUntrackedParameter<int>("minLayerWithSimhitPerChamber");
  minRechitSegment          = pset.getUntrackedParameter<int>("minRechitPerSegment");
  maxPhi                    = pset.getUntrackedParameter<double>("maxPhiSeparation");
  maxTheta                  = pset.getUntrackedParameter<double>("maxThetaSeparation");

  file = new TFile(filename.c_str(), "RECREATE");
    	
  if (file->IsOpen()) cout<<"file open!"<<endl;
  else cout<<"*** Error in opening file ***"<<endl;
   
  hchi2    = new TH1F("h4", "chi2", 120, 0, 30);    
  hrechit  = new TH1I("h5", "nrechit", 6, 2, 8);  
  hsegment = new TH1I("h6", "segments multiplicity", 20, 0, 20);   
  heta     = new TH1F("h7", "eta sim muons", 50, -2.5, 2.5);  
  hpt      = new TH1F("h8", "pT sim muons", 120, 0, 60);
  hx       = new TH1F("h9", "deltaX", 400, -100, +100);
  hy       = new TH1F("h10", "deltaY",400, -100, +100);
    
  char a[3];
  for (int i=0; i<4; i++) {
    sprintf(a, "h2%d", i);
    hphi[i]   = new TH1F(a, "reso phi", 150, -0.23, 0.23);
    sprintf(a, "h3%d", i);
    htheta[i] = new TH1F(a, "reso theta", 150, -0.45, 0.45);
  }    
}


// Destructor
CSCSegmentReader::~CSCSegmentReader() {

  int ibin = 0;
  heff0    = new TH1F("h0", "efficiency", segMap1.size()*2 + 2, 0, segMap1.size()*2 + 2); 
  heff1    = new TH1F("h1", "efficiency", segMap1.size()*2 + 2, 0, segMap1.size()*2 + 2); 
  heff2    = new TH1F("h2", "efficiency", segMap1.size()*2 + 2, 0, segMap1.size()*2 + 2); 
  heff3    = new TH1F("h3", "efficiency", segMap1.size()*2 + 2, 0, segMap1.size()*2 + 2); 

  cout << "Raw reco efficiency for 6-hit simulated segment" << endl;        
  for (map<string,int>::const_iterator it = segMap1.begin(); it != segMap1.end(); it++) {
    ibin++;
    float eff = (float)it->second/(float)chaMap1[it->first]; 
    heff0->SetBinContent(ibin*2, eff);
    heff0->GetXaxis()->SetBinLabel(ibin*2, (it->first).c_str());
    cout << it->first << ": " << it->second << " " << chaMap1[it->first] 
         << "  "      << eff  << endl;
  }
  ibin = 0;
  cout << "Raw reco efficiency for chamber with 6 layers with rechits" << endl;        
  for (map<string,int>::const_iterator it = segMap2.begin(); it != segMap2.end(); it++) {
    ibin++;
    float eff = (float)it->second/(float)chaMap2[it->first]; 
    heff1->SetBinContent(ibin*2, eff);
    heff1->GetXaxis()->SetBinLabel(ibin*2, (it->first).c_str());
    cout << it->first << ": " << it->second << " " << chaMap2[it->first] 
         << "  "      << eff << endl;
  }
  ibin = 0;
  cout << "Reco efficiency for building 6-hit segment for 6-hit simulated segment" << endl;        
  for (map<string,int>::const_iterator it = segMap3.begin(); it != segMap3.end(); it++) {
    ibin++;
    float eff = (float)it->second/(float)chaMap1[it->first]; 
    heff2->SetBinContent(ibin*2, eff);
    heff2->GetXaxis()->SetBinLabel(ibin*2, (it->first).c_str());
    cout << it->first << ": " << it->second << " " << chaMap1[it->first] 
         << "  "      << eff  << endl;
  }
  ibin = 0;
  cout << "Reco efficiency for chamber with 6 layers with rechits" << endl;        
  for (map<string,int>::const_iterator it = segMap3.begin(); it != segMap3.end(); it++) {
    ibin++;
    float eff = (float)it->second/(float)chaMap2[it->first]; 
    heff3->SetBinContent(ibin*2, eff);
    heff3->GetXaxis()->SetBinLabel(ibin*2, (it->first).c_str());
    cout << it->first << ": " << it->second << " " << chaMap2[it->first] 
         << "  "      << eff  << endl;
  }

  file->cd();
  heff1->Write();
  heff2->Write();
  heff3->Write();
  hchi2->Write();
  hrechit ->Write();
  hsegment->Write();
  hpt->Write();
  heta->Write();
  hx->Write();
  hy->Write();

  for (int i=0; i<4; i++) {        
    hphi[i]->Write();
    htheta[i]->Write();
  }    
  file->Close();
}


// The real analysis
void CSCSegmentReader::analyze(const Event& event, const EventSetup& eventSetup) {
    
    edm::ESHandle<CSCGeometry> h;
    eventSetup.get<MuonGeometryRecord>().get(h);
    const CSCGeometry* geom = &*h;
    
    Handle<SimTrackContainer> simTracks;
    event.getByLabel("g4SimHits",simTracks);
    
    Handle<PSimHitContainer> simHits; 
    event.getByLabel("g4SimHits","MuonCSCHits",simHits);    
    
    Handle<CSCRecHit2DCollection> recHits; 
    event.getByLabel("csc2DRecHits", recHits);   
    
    Handle<CSCSegmentCollection> cscSegments;
    event.getByLabel("cscSegments", cscSegments);
    
    simInfo(simTracks);
    resolution(simHits, cscSegments, geom);
    recInfo(simHits, recHits, cscSegments, geom);
}


void CSCSegmentReader::recInfo(const edm::Handle<edm::PSimHitContainer> simHits, 
                               const edm::Handle<CSCRecHit2DCollection> recHits, 
                               const edm::Handle<CSCSegmentCollection> cscSegments,
                               const CSCGeometry* geom) {
    
  hsegment->Fill(cscSegments->end() - cscSegments->begin());


  std::vector<CSCDetId> cscChambers;
  for (PSimHitContainer::const_iterator simIt = simHits->begin(); simIt != simHits->end(); simIt++) {
        
    CSCDetId simId = (CSCDetId)(*simIt).detUnitId();

    double g6 = geom->chamber(simId)->layer(6)->surface().toGlobal(LocalPoint(0,0,0)).z();	
    double g1 = geom->chamber(simId)->layer(1)->surface().toGlobal(LocalPoint(0,0,0)).z();	
        
    int firstLayer = 1;
    if (fabs(g1) > fabs(g6)) firstLayer = 6;

    const CSCChamber* chamber = geom->chamber(simId); 

    if (simId.layer() == firstLayer) {

      // Test if have 6 layers with simhits in chamber
      int ith_layer = 0;
      int nLayerWithSimhitsInChamber = 0; 

      for (PSimHitContainer::const_iterator it2 = simHits->begin(); it2 != simHits->end(); it2++) {        
        CSCDetId simId2 = (CSCDetId)(*it2).detUnitId();
        if ((simId2.chamber() == simId.chamber()) &&
            (simId2.station() == simId.station()) &&
            (simId2.ring()    == simId.ring())    &&
            (simId2.endcap()  == simId.endcap())  &&
            (simId2.layer()   != ith_layer     )) {
          nLayerWithSimhitsInChamber++;
          ith_layer = simId2.layer();
        }
      }

      if (nLayerWithSimhitsInChamber < minLayerWithSimhitChamber) continue;

      chaMap1[chamber->specs()->chamberTypeName()]++;                

      bool satisfied0 = false;
      ith_layer = 0;
      int nLayerWithRechitsInChamber = 0; 

      // Test if have 6 layers with rechits in chamber
      for (CSCRecHit2DCollection::const_iterator recIt = recHits->begin(); recIt != recHits->end(); recIt++) {
	CSCDetId idrec = (CSCDetId)(*recIt).cscDetId();	
        if ((idrec.endcap()  == simId.endcap())  &&
            (idrec.ring()    == simId.ring())    &&
            (idrec.station() == simId.station()) &&
            (idrec.chamber() == simId.chamber()) &&
            (idrec.layer()   != ith_layer     )) {
          nLayerWithRechitsInChamber++;
          ith_layer = idrec.layer();
        }
      }

      if (nLayerWithRechitsInChamber >= minLayerWithRechitChamber) {
        chaMap2[chamber->specs()->chamberTypeName()]++;
        satisfied0 = true;
      }

      bool satisfied1 = false;
      bool satisfied2 = false;

      for (CSCSegmentCollection::const_iterator segIt=cscSegments->begin(); segIt!=cscSegments->end(); ++segIt) {

        CSCDetId id = (*segIt).cscDetId();
        if ((simId.endcap()  == id.endcap())  &&
            (simId.ring()    == id.ring())    &&
            (simId.station() == id.station()) &&
            (simId.chamber() == id.chamber())) {              

          satisfied1 = true;

          hrechit->Fill((*segIt).nRecHits());        

          if ( (*segIt).nRecHits() >= minRechitSegment ) {
            hchi2->Fill(((*segIt).chi2()/(2*(*segIt).nRecHits()-4)));
            satisfied2 = true;
            break;
          }
        }    
      }
      if (satisfied1) segMap1[chamber->specs()->chamberTypeName()]++;
      if (satisfied1 && satisfied0) segMap2[chamber->specs()->chamberTypeName()]++;
      if (satisfied2 && satisfied0) segMap3[chamber->specs()->chamberTypeName()]++;
    }   
  }
}


void CSCSegmentReader::simInfo(const edm::Handle<SimTrackContainer> simTracks) {

  for (SimTrackContainer::const_iterator it = simTracks->begin(); it != simTracks->end(); it++) {
        
    if (abs((*it).type()) == 13) {
      hpt->Fill((*it).momentum().perp());
      heta->Fill((*it).momentum().eta());
    }    
  }    
}
  
  
void CSCSegmentReader::resolution(const Handle<PSimHitContainer> simHits, 
                                  const Handle<CSCSegmentCollection> cscSegments, 
                                  const CSCGeometry* geom) {


  for (CSCSegmentCollection::const_iterator its = cscSegments->begin(); its != cscSegments->end(); its++) {
        
    double segX=-99999., segY=-99999.;
    double sim1X = 100., sim1Y = 100.;
    double sim2X = 0.,   sim2Y = 0.;
    double resoPhi = 1., resoTheta = 1.;
    double minPhi = 1, minTheta = 1;
    unsigned int simTrack = 0;
        
    const CSCChamber* chamber = 0;
    const CSCChamber* ch = geom->chamber((*its).cscDetId());
       
    GlobalPoint gpn = ch->layer(6)->surface().toGlobal(LocalPoint(0,0,0)); 	 
    GlobalPoint gpf = ch->layer(1)->surface().toGlobal(LocalPoint(0,0,0));
    int firstLayer = 6;
    int lastLayer = 1;
        
    if (fabs(gpn.z()) > fabs(gpf.z())) {
      firstLayer = 1;
      lastLayer = 6;
    }
        
    for (PSimHitContainer::const_iterator ith = simHits->begin(); ith != simHits->end(); ith++) {
        
      CSCDetId simId = (CSCDetId)(*ith).detUnitId(); 

      if ((simId.layer() == firstLayer)) {

        // Test if have 6 layers with simhits in chamber
        std::vector<CSCDetId> chambers;
        int ith_layer = 0;
        int nLayerWithSimhitsInChamber = 0; 

        for (PSimHitContainer::const_iterator it2 = simHits->begin(); it2 != simHits->end(); it2++) {        
          CSCDetId simId2 = (CSCDetId)(*it2).detUnitId();
          if ((simId2.chamber() == simId.chamber()) &&
              (simId2.station() == simId.station()) &&
              (simId2.ring()    == simId.ring())    &&
              (simId2.endcap()  == simId.endcap())  &&
              (simId2.layer()   != ith_layer     )) {
            nLayerWithSimhitsInChamber++;
            ith_layer = simId2.layer();
          }
        }

        if (nLayerWithSimhitsInChamber < minLayerWithSimhitChamber) continue;
            
        if (ch == geom->chamber(simId)) {

          LocalVector segDir = (*its).localDirection();
          LocalVector simDir = (*ith).momentumAtEntry().unit();
                    
          double deltaTheta = fabs(segDir.theta()-simDir.theta());
          double deltaPhi = fabs(segDir.phi()-simDir.phi());
            
          if ((deltaPhi < minPhi) && (deltaTheta < minTheta)) {
                
            minPhi = deltaPhi;
            minTheta = deltaTheta;
            resoTheta = (segDir.theta()-simDir.theta());
            resoPhi = (segDir.phi()-simDir.phi());
            chamber = ch;
            segX = (*its).localPosition().x();
            sim1X =(*ith).localPosition().x();
            sim1Y =(*ith).localPosition().y();
            segY = (*its).localPosition().y();
            simTrack = (*ith).trackId();
          }
        }
      }    
    }
         
    for (PSimHitContainer::const_iterator ith = simHits->begin(); ith != simHits->end(); ith++) {
        
      CSCDetId simId = (CSCDetId)(*ith).detUnitId();
            
      if ((simId.layer() == lastLayer) && (simTrack = (*ith).trackId())) {
        sim2X =(*ith).localPosition().x();
        sim2Y =(*ith).localPosition().y();
      }
    }   

    if (chamber != 0 && sim1X != 100. ) {
    
      string s = chamber->specs()->chamberTypeName();

      int indice = 3;
      if (s == "ME1/b") indice = 0;
      if ((s == "ME1/2") || (s == "ME1/3")) indice = 1;
      if ((s == "ME2/1") || (s == "ME3/1") || (s == "ME4/1")) indice = 2;
       
      if (s != "ME1/a") {
        hphi[indice]->Fill(resoPhi);
        htheta[indice]->Fill(resoTheta);   
      }
                
      if ((sim2Y < 100.) && (s == "ME1/a")) {
        hx->Fill(segX - (sim1X+sim2X)/2.);
        hy->Fill(segY - (sim1Y+sim2Y)/2.);
      }                
    }      
  }  
}



DEFINE_FWK_MODULE(CSCSegmentReader);

