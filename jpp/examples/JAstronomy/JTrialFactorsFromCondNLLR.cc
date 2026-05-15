#include <string>
#include <iostream>
#include <unistd.h> //getopt
#include <filesystem>
#include <vector>
#include <map>
#include <exception>
#include <algorithm>

#include "TROOT.h"
#include "TFile.h"
#include "TF1.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TGraph.h"
#include "TMath.h"

#include "JTools/JAbstractHistogram.hh"
#include "Jeep/JParser.hh"


/**
 * \file
 * Application that goes from the TH2D of NLLRs of multiple candidates, generate by
 * JGen2_condNLLR, and computes the trial factor for their joint analysis.
 * You can also duplicate candidates with duplicatesPerCandidate (for testing).
 *
 * \author fvazquez
 */
int main(int argc, char **argv)
{
    using namespace std;
    using namespace JPP;
    namespace fs = std::filesystem;

    typedef JAbstractHistogram<double> histogram_type;

    fs::path            outputFilepath;
    fs::path            candidateNllrListFilepath;
    size_t              duplicatesPerCandidate;
    histogram_type      X;
    int                 debug;

    try {

        JParser<> zap;

        zap['o'] = make_field(outputFilepath,               "output file with histograms");
        zap['i'] = make_field(candidateNllrListFilepath,    "file with list of H2D NLLR files to use, each one an output from JPseudoExperimentCondNLLR");
        zap['n'] = make_field(duplicatesPerCandidate,       "number of times to duplicate candidates")          = 1;
        zap['x'] = make_field(X,                            "x-axis of -log(p) histogram")                      = histogram_type(80, 0., +8);
        zap['d'] = make_field(debug)                        = 1;

        zap(argc, argv);
    }
    catch(const exception& error) {
        FATAL(error.what() << endl);
    }


    // Read list of files to open
    vector<fs::path> candidateNllrFilepath_vec;
    ifstream file( candidateNllrListFilepath );
    string line;
    //if (file.is_open()) {
    if (true) {
        while (getline(file, line)) {
            if ( fs::is_regular_file(line) ) {
                candidateNllrFilepath_vec.push_back(line);
            } else {
                cout << " Skipping file '" << line << "' : it does not exist" << endl;
            }
        }
        file.close();
    }
    else {
        cerr << "Unable to open file: " << candidateNllrListFilepath << endl;
    }
    size_t numberOfCandidates = candidateNllrFilepath_vec.size();
    size_t numberOfCandidates_eff = numberOfCandidates * duplicatesPerCandidate;

    // Prepare output objects
    size_t nbins_pT = X.getNumberOfBins();  // Note that these are actually -log10(pT) values, to help with histogram presentation
    size_t min_pT = X.getLowerLimit();
    size_t max_pT = X.getUpperLimit();
    TFile outfile(outputFilepath.c_str(), "recreate");
    TH1D h1d_pT_rcdf_bestof( "h1d_pT_rcdf_bestofcandidate", "Distribution of p_{local} over complete dataset (best 1st period candidate); -log_{10}(p_{local}); Reverse CDF",
        nbins_pT, min_pT, max_pT);
    TH1D h1d_pT_rcdf_constr( "h1d_pT_rcdf_constrcandidate", "Distribution of p_{local} over complete dataset (best filtered candidate); -log_{10}(p_{local}); Reverse CDF",
        nbins_pT, min_pT, max_pT);
    TH1D h1d_pT_trialfactor_bestof( "h1d_pT_trialfactor_bestofcandidate", "Trial factor for p_{local} over complete dataset (best 1st period candidate); -log_{10}(p_{local}); Trial factor",
        nbins_pT, min_pT, max_pT);
    TH1D h1d_pT_trialfactor_constr( "h1d_pT_trialfactor_constrcandidate", "Trial factor for p_{local} over complete dataset (best filtered candidate); -log_{10}(p_{local}); Trial factor",
        nbins_pT, min_pT, max_pT);

    for (size_t pT_bin = 1; pT_bin != nbins_pT+1; ++pT_bin) {
        h1d_pT_rcdf_bestof.SetBinContent(pT_bin, 0.); // Will be filled out with sum terms
        h1d_pT_rcdf_constr.SetBinContent(pT_bin, 1.); // Will be filled out with product terms
    }


    // Loop over candidates
    double numberOfCandidates_constr_eff = 0;
    cout << "Looping over " << numberOfCandidates << " candidates, duplicated " << duplicatesPerCandidate << " times" << endl;
    for (size_t i_cand = 0; i_cand != numberOfCandidates; ++i_cand) {
        // Load inputs
        TFile infile( ( candidateNllrFilepath_vec[i_cand] ).c_str(), "read");
        TH2D* h2d_nllr = infile.Get<TH2D>("h2d_nllr");
        TH2D* h2d_nllr_constr = infile.Get<TH2D>("h2d_nllr_constr");

        size_t nbins_L1 = h2d_nllr->GetNbinsX();
        size_t nbins_LT = h2d_nllr->GetNbinsY();
        TH1D* h1d_LT = h2d_nllr->ProjectionY();
        h1d_LT->GetXaxis()->SetRange(0,nbins_LT+1); // So cumulative includes under/overflow
        TH1D* h1d_LT_rcdf = (TH1D*) h1d_LT->GetCumulative(false);
        h1d_LT->Delete();


        // *** Fill out "Selected" distribution contribution ***
        // Assume h2d_nllr axes have binning starting at zero, that it represents a 2D pdf (i.e. integral == 1),
        // This would work differently if they were actual functions, or if the histograms didn't start at zero...

        TH1D* h1d_L1 = h2d_nllr->ProjectionX();
        h1d_L1->GetXaxis()->SetRange(0,nbins_L1+1); // So cumulative includes under/overflow
        TH1D* h1d_L1_cdf = (TH1D*) h1d_L1->GetCumulative(); // We use the fact that hist_cumul.Bin(n) = hist.Bin(1)+ ... hist.Bin(n)
        h1d_L1->Delete();

        // Prepare the projections of NLLR_all (LT) into histograms, per value of NLLR_1st (L1)
        vector<TH1D*> h1d_LT_cdf_perL1_vec = {};
        for (size_t L1_bin = 0; L1_bin != nbins_L1+1; ++L1_bin) {
            TH1D* h1d_LT_L1slice = h2d_nllr->ProjectionY( ("_py_" + to_string(L1_bin)).c_str(), L1_bin, L1_bin ) ;
            h1d_LT_cdf_perL1_vec.push_back( (TH1D*) h1d_LT_L1slice->GetCumulative() );
            h1d_LT_L1slice->Delete();
            if (h1d_LT_cdf_perL1_vec.back()->GetBinContent(nbins_LT)>0) {
                h1d_LT_cdf_perL1_vec.back()->Scale(1./h1d_LT_cdf_perL1_vec.back()->GetBinContent(nbins_LT));
            } // else { what? Replace all bins by 0.5? }
        }

        // Compute the contribution to the final CDF from this candidate (boosted by Duplicates term)
        TGraph gpT_rcdf_bestof_sumcontr;
        for (size_t LT_bin = 1; LT_bin != nbins_LT+1; ++LT_bin) {
            double rcdf_sumcontr = 1./numberOfCandidates - TMath::Power( h1d_L1_cdf->GetBinContent(0), int(numberOfCandidates_eff) )/numberOfCandidates * h1d_LT_cdf_perL1_vec[0]->GetBinContent(LT_bin-1);
            for (size_t L1_bin = 1; L1_bin != nbins_L1+1; ++L1_bin) {
                rcdf_sumcontr -= ( TMath::Power( h1d_L1_cdf->GetBinContent(L1_bin), int(numberOfCandidates_eff) ) - TMath::Power( h1d_L1_cdf->GetBinContent(L1_bin-1), int(numberOfCandidates_eff)) )/numberOfCandidates * h1d_LT_cdf_perL1_vec[L1_bin]->GetBinContent(LT_bin-1) ;
            }
            double pT = h1d_LT_rcdf->GetBinContent(LT_bin);
            double neglog10pT = ( pT > 0 ? -TMath::Log10(pT) : 100);
            gpT_rcdf_bestof_sumcontr.AddPoint( neglog10pT,  rcdf_sumcontr );
        }
        //TODO: Sanitize TGraph against multiple points in a row with same X value

        // Fill final histogram with reverse CDF contribution
        for (size_t pT_bin = 1; pT_bin != nbins_pT+1; ++pT_bin) {
            double rcdf_sumcontr = 0;
            double neglog10pT = h1d_pT_rcdf_bestof.GetBinLowEdge(pT_bin);
            if (pT_bin == 1) {
                rcdf_sumcontr = 1./numberOfCandidates;
            } else if ( neglog10pT <= gpT_rcdf_bestof_sumcontr.GetPointX(nbins_LT-1) ) {
                rcdf_sumcontr = gpT_rcdf_bestof_sumcontr.Eval( neglog10pT );    //TODO: Do better than Eval
            }
            h1d_pT_rcdf_bestof.AddBinContent(pT_bin, rcdf_sumcontr);
        }

        // Clean-up
        h1d_L1_cdf->Delete();
        for (size_t L1_bin = 0; L1_bin != nbins_L1+1; ++L1_bin) {
            h1d_LT_cdf_perL1_vec[L1_bin]->Delete();
        }


        // *** Fill out "Filtered" distribution contribution ***
        // Assume h2d_nllr_constr has same weighting as h2d_nllr, where h2d_nllr->Integral() == 1.
        // TODO : Double-check calculations for last bin...

        TH1D* h1d_LT_constr = h2d_nllr_constr->ProjectionY();
        h1d_LT_constr->GetXaxis()->SetRange(0,nbins_LT+1); // So cumulative includes under/overflow
        double P_passfilter = h1d_LT_constr->Integral(0, nbins_LT+1);
        TH1D* h1d_LT_cdf_constr = (TH1D*) h1d_LT_constr->GetCumulative();
        h1d_LT_cdf_constr->Scale( 1./P_passfilter );
        h1d_LT_constr->Delete();
        numberOfCandidates_constr_eff += P_passfilter * duplicatesPerCandidate;

        // Compute the contribution to the final CDF from this candidate (boosted by Duplicates term)
        TGraph gpT_cdf_constr_prodcontr;
        for (size_t LT_bin = 1; LT_bin != nbins_LT+1; ++LT_bin) {
            double cdf_prodcontr = TMath::Power( (1.-P_passfilter) + P_passfilter * h1d_LT_cdf_constr->GetBinContent(LT_bin-1), int(duplicatesPerCandidate)) ;
            double pT = h1d_LT_rcdf->GetBinContent(LT_bin);
            double neglog10pT = ( pT > 0 ? -TMath::Log10(pT) : 100);
            gpT_cdf_constr_prodcontr.AddPoint( neglog10pT,  cdf_prodcontr );
        }
        //TODO: Sanitize TGraph against multiple points in a row with same X value

        // Fill final histogram with CDF contribution (to be turned into a reverse CDF at the end)
        for (size_t pT_bin = 1; pT_bin != nbins_pT+1; ++pT_bin) {
            double cdf_prodcontr = 1.;
            double neglog10pT = h1d_pT_rcdf_constr.GetBinLowEdge(pT_bin);
            if (pT_bin == 1) {
                cdf_prodcontr = 0;
            } else if ( neglog10pT <= gpT_cdf_constr_prodcontr.GetPointX(nbins_LT-1) ) {
                cdf_prodcontr = gpT_cdf_constr_prodcontr.Eval( neglog10pT );    //TODO: Do better than Eval
            }
            h1d_pT_rcdf_constr.SetBinContent(pT_bin, 
                    h1d_pT_rcdf_constr.GetBinContent(pT_bin) * cdf_prodcontr);
        }

        // Clean-up
        h1d_LT_cdf_constr->Delete();

        h2d_nllr->Delete();
        h2d_nllr_constr->Delete();

    } // End loop on candidates


    // Turn output histograms from a CDF into a reverse CDF (p-values),
    // unset errors, and fill out trial factors
    for (size_t pT_bin = 1; pT_bin != nbins_pT+1; ++pT_bin) {
        h1d_pT_rcdf_bestof.SetBinError(pT_bin,0);
        h1d_pT_rcdf_constr.SetBinContent(pT_bin, 1- h1d_pT_rcdf_constr.GetBinContent(pT_bin) );
        h1d_pT_rcdf_constr.SetBinError(pT_bin,0);

        h1d_pT_trialfactor_bestof.SetBinContent(pT_bin, h1d_pT_rcdf_bestof.GetBinContent(pT_bin) / TMath::Power(10, - h1d_pT_rcdf_bestof.GetBinLowEdge(pT_bin) ) );
        h1d_pT_trialfactor_bestof.SetBinError(pT_bin,0);
        h1d_pT_trialfactor_constr.SetBinContent(pT_bin, h1d_pT_rcdf_constr.GetBinContent(pT_bin) / TMath::Power(10, - h1d_pT_rcdf_constr.GetBinLowEdge(pT_bin) ) );
        h1d_pT_trialfactor_constr.SetBinError(pT_bin,0);
    }
    h1d_pT_rcdf_bestof.SetStats(0);
    h1d_pT_rcdf_constr.SetStats(0);
    h1d_pT_trialfactor_bestof.SetStats(0);
    h1d_pT_trialfactor_constr.SetStats(0);

    // Make naive trial factor functions
    TF1 f1_pT_trialfactor_all("f1_pT_trialfactor_allcandidates", ( "(1 - TMath::Power(1-TMath::Power(10,-x), " + to_string(numberOfCandidates_eff) + " ) ) / TMath::Power(10,-x)" ).c_str(), min_pT, max_pT);
    TF1 f1_pT_trialfactor_constr("f1_pT_trialfactor_constrcandidates", ( "(1 - TMath::Power(1-TMath::Power(10,-x), " + to_string(numberOfCandidates_constr_eff) + " ) ) / TMath::Power(10,-x)" ).c_str(), min_pT, max_pT);
    f1_pT_trialfactor_all.SetLineWidth(1);
    f1_pT_trialfactor_all.SetLineColor(1);
    f1_pT_trialfactor_constr.SetLineWidth(1);
    f1_pT_trialfactor_constr.SetLineColor(1);

    // Save results
    outfile.cd();
    h1d_pT_rcdf_bestof.Write();
    h1d_pT_rcdf_constr.Write();
    h1d_pT_trialfactor_bestof.Write();
    h1d_pT_trialfactor_constr.Write();
    f1_pT_trialfactor_all.Write();
    f1_pT_trialfactor_constr.Write();
    outfile.Write();
    outfile.Close();


}

