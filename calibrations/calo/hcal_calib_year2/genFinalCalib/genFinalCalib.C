#include <string>
#include"tsc_cos_merge.C"

struct TowerData {
    int tower_ieta;
    int tower_iphi;
    float calib_factor;
    int run_range_low;
    int run_range_high;
};
void loadCSV(const std::string& filename, std::vector<TowerData>& data);
std::vector<std::vector<float>> getTowersForRun(const std::vector<TowerData>& data, int run_number);


//make sure TS_list has relative or full paths to actual root files
void genFinalCalib(const char *runs = "cos_runs.txt")
{

  ///////////////////////////////////
  // Load the half hieght data from the CSV file
  ///////////////////////////////////////
  std::vector<TowerData> hh_data;
  loadCSV("halfHieghtList.csv", hh_data);


  /////////////////////////////////////////////////
  // Get cosmic calibrations
  ///////////////////////////////////////
  ifstream runlist(runs);
  std::string line;
  vector<int> cosmic_runnumbers;
  while(getline(runlist,line))
  {
    cosmic_runnumbers.push_back(stoi(line));

    //std::string oh_out = "fitResults/oh_cosmicsMerged_fitted_" + line + ".root";
    std::string oh_out = "../tsc_cos_comb/fitResults/avgCorr.root";
    std::string ih_out = "fitResults/ih_cosmicsMerged_fitted_" + line + ".root";

    string cosmic_CDB_file_oh = Form("/sphenix/u/bseidlitz/work/macros/calibrations/calo/hcal_towerSlope_y2/cosmicCalibFiles/ohcal_cosmic_calibration_%s.root",line.c_str());

    tsc_cos_merge(oh_out,cosmic_CDB_file_oh,Form("cdbFiles/ohcal_cdb_tsc_cos_calib_%s.root",line.c_str()),0);

  }


  ///////////////////////////////////////
  // temperature correction run-by-run
  ///////////////////////////////////////
  ifstream runlistBeam("runList.txt");
  set<int> tempset;
  while(getline(runlistBeam,line)){
    int runnumber = stoi(line);
    tempset.insert(runnumber);
  }
  vector<int> runnumbersBeam(tempset.begin(),tempset.end());
   
  TFile* ftemp = new TFile("../fileQA/output/anaOut.root");

  TH1F* h_runnumbers_temp = (TH1F*) ftemp->Get("runNumberHist");
  TH1F* h_temp_run = (TH1F*) ftemp->Get("h_temp_run");


  ///////////////////////////
  // get cosmic run temps
  ///////////////////////////
  // get the temperature of the cosmics runs
  // could be improved, right now it uses the cosest beam run
  vector<float> cosmic_temps;
  for (int cos_run : cosmic_runnumbers){
    int minDelta = 1000000;
    int minRun = 0;
    float cos_temp = 0;
    for( int it=0; it<h_runnumbers_temp->GetNbinsX(); it++){   
      int delta = abs(cos_run - h_runnumbers_temp->GetBinContent(it));
      if (delta < minDelta  && h_temp_run->GetBinContent(it) > 10){
        minRun = h_runnumbers_temp->GetBinContent(it);
        minDelta = delta;
        cos_temp =h_temp_run->GetBinContent(it);
      }
    }
    cosmic_temps.push_back(cos_temp);
    cout << "cosmics run " << cos_run << "  temp="  << cos_temp << " size=" << cosmic_temps.size() << endl;
  }


  ///////////////////////////////////////
  // Generate the run-by-run calibrations
  ///////////////////////////////////////
  // This follows the logic laid out in the note
  // cosmics calibration is the base
  // the tsc is the used to adjust the cosmic calib
  // then the temperture difference between the cosmic
  // run and the run of interest to correct the 
  // adjusted cosmics for the rinal calibration
  for(int irb=0; irb<runnumbersBeam.size(); irb++)
  {
    int runnumber = runnumbersBeam[irb];  

    int runnumber_last;
    if (irb+1  < runnumbersBeam.size() ) runnumber_last = runnumbersBeam[irb+1]-1;
    else runnumber_last = 53880;

    cout << "----------------------------------" << endl;
    cout << "starting run " << runnumber << endl;

    int asoCosmic = 0;
    int minCosDelta = 1000000;
    float asoCosmicTemp = 0;
    for(int ic=0; ic< cosmic_runnumbers.size(); ic++){
       int cos_run = cosmic_runnumbers[ic]; 
       int delta = runnumber - cos_run;
       if (minCosDelta  > delta && delta > 0){
         asoCosmic     = cos_run; 
         asoCosmicTemp =  cosmic_temps[ic]; 
         minCosDelta = delta;
       }
    }
    cout << "closeset cosmic run " << asoCosmic << endl;  


    for( int it=0; it<h_runnumbers_temp->GetNbinsX(); it++){   
      if(runnumber == h_runnumbers_temp->GetBinContent(it)){
         float temp = h_temp_run->GetBinContent(it);
         float alpha = 0.045;
         float deltaT =  temp - asoCosmicTemp; 

         cout << "of this run = " << temp << "   temp of cosmic run =" << asoCosmicTemp << endl;

         float correction = 1+ deltaT*alpha;
         if (temp < 10) continue;
         cout << "correction=" << correction << endl;

         string cdbFileName = Form("cdbFiles/cdb_ohcal_tsc_temp_cosmic_%d.root",runnumber);
         //string cosmic_CDB_file_oh = Form("/sphenix/u/bseidlitz/work/macros/calibrations/calo/hcal_towerSlope_y2/cosmicCalibFiles/ohcal_cosmic_calibration_%d.root",asoCosmic);
         string cosmic_CDB_file_oh = Form("cdbFiles/ohcal_cdb_tsc_cos_calib_%d.root",asoCosmic);

          // gen calibration file with cosmics, tsc and temp correction
          // but no half hieght corrections
         //genCdbCorr(correction,cosmic_CDB_file_oh,cdbFileName,0); 

         // with half hieght correction
         std::vector<std::vector<float>> hh_towersForRun = getTowersForRun(hh_data, runnumber);
         string cdbFileName_hh = Form("final_ohcal_calib_y2_namceChange/cdb_ohcal_tsc_temp_cosmic_hh_%d_%d.root",runnumber,runnumber_last);
         genCdbCorr_HH(correction,cosmic_CDB_file_oh,cdbFileName_hh,0,hh_towersForRun); 
      }
    }
  }



}//end macro



void loadCSV(const std::string& filename, std::vector<TowerData>& data) {
    std::ifstream file(filename);
    std::string line;
    if(!file) {cout << endl << "ccan't find file" << endl; exit; }

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        TowerData towerData;

        // Parse the line and fill the struct
        char comma;  // To consume the commas between values
        ss >> towerData.tower_ieta >> comma
           >> towerData.tower_iphi >> comma
           >> towerData.calib_factor >> comma
           >> towerData.run_range_low >> comma
           >> towerData.run_range_high;

        // Store the parsed data
        data.push_back(towerData);
    }
}


std::vector<std::vector<float>> getTowersForRun(const std::vector<TowerData>& data, int run_number) {
    std::vector<std::vector<float>> towersForRun;

    // Loop through all the entries and check if the run_number is within the range
    for (const auto& towerData : data) {
        if (run_number > towerData.run_range_low && run_number < towerData.run_range_high) {
            // If it matches the range, store the tower data
            towersForRun.push_back({static_cast<float>(towerData.tower_ieta),
                                    static_cast<float>(towerData.tower_iphi),
                                    towerData.calib_factor});
        }
    }

    return towersForRun;
}

