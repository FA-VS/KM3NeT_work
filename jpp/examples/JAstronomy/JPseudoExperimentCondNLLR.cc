#include <string>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <map>
#include <exception>
#include <algorithm>

#include "TROOT.h"
#include "TFile.h"
#include "TH2D.h"
#include "TRandom3.h"

#include "JAstronomy/JPseudoExperiment.hh"
#include "JAstronomy/JAspera.hh"
#include "JAstronomy/JAstronomyToolkit.hh"

#include "JGizmo/JGizmoToolkit.hh"
#include "JTools/JAbstractHistogram.hh"
#include "JROOT/JRandom.hh"

#include "Jeep/JContainer.hh"
#include "Jeep/JParser.hh"


namespace {

  int                 debug   =   2;

  /**
   * Auxiliary data structure for experiment.
   */
  struct experiment_type {

    JGIZMO::JRootObjectID HS;                   //!< signal for generation
    JGIZMO::JRootObjectID HB;                   //!< background for generation
    JGIZMO::JRootObjectID Hs;                   //!< signal for evaluation
    JGIZMO::JRootObjectID Hb;                   //!< background for evaluation
    double boost;                               //!< boosting factor for signal and background

    JASTRONOMY::JPseudoExperiment::parameters_type nuisance;

    /**
     * Read experiment from input stream.
     *
     * \param  in                           input stream
     * \param  experiment           experiment
     * \return                              input stream
     */
    friend inline std::istream& operator>>(std::istream& in, experiment_type& experiment)
    {
      return in >> experiment.HS
                >> experiment.HB
                >> experiment.Hs
                >> experiment.Hb
                >> experiment.boost
                >> experiment.nuisance;
    }


    /**
     * Write experiment to output stream.
     *
     * \param  out                      output stream
     * \param  experiment           experiment
     * \return                              output stream
     */
    friend inline std::ostream& operator<<(std::ostream& out, const experiment_type& experiment)
    {
      return out << experiment.HS << ' '
                 << experiment.HB << ' '
                 << experiment.Hs << ' '
                 << experiment.Hb << ' '
                 << experiment.boost << ' '
                 << experiment.nuisance;
    }
  };


  /**
   * Auxiliary data structure for printing.
   */
  struct printer {
    /**
     * Constructor.
     *
     * \param  title                    title
     * \param  ps                           pointer to object
     */
    printer(const char* const title,
            const TObject*      ps) :
      title(title),
      ps(ps)
    {}

    /**
     * Write printer to output stream.
     *
     * \param  out                      output stream
     * \param  printer              printer
     * \return                              output stream
     */
    friend inline std::ostream& operator<<(std::ostream& out, const printer& printer)
    {
      using namespace std;
      using namespace JPP;

      out << setw(16) << left << printer.title << right;

      if (printer.ps != NULL) {

        out << ' ' << setw(16) << left << printer.ps->GetName() << right;

        const TH1* h1 = dynamic_cast<const TH1*>(printer.ps);

        if (h1 != NULL) {
          out << ' ' << FIXED(10,3) << h1->GetSumOfWeights();
        }
      }

      return out;
    }

  private:
    const char* const title;
    const TObject*      ps;
  };


  /**
   * Auxiliary data structure for grouping experiments.
   */
  struct experiment_group {
    std::vector<JASTRONOMY::JPseudoExperiment> px_vec;
    double signal_weight;

    experiment_group(const std::vector<experiment_type>& setup_vec,
                     double Fs,
                     double Fb,
                     size_t M)
    {
      signal_weight = 0;

      for ( const auto& setup : setup_vec) {
        TObject* pS  =  getObject(setup.HS);
        TObject* pB  =  getObject(setup.HB);
        TObject* ps  =  getObject(setup.Hs);
        TObject* pb  =  getObject(setup.Hb);

        if (setup.boost != 1.) {
          std::cout << "Boosting background and signal for following dataset by factor " << setup.boost << std::endl; 
          dynamic_cast<TH1*>(pS)->Scale(setup.boost);
          dynamic_cast<TH1*>(pB)->Scale(setup.boost);
          dynamic_cast<TH1*>(ps)->Scale(setup.boost);
          dynamic_cast<TH1*>(pb)->Scale(setup.boost);
        }

        signal_weight += dynamic_cast<const TH1*>(ps)->GetSumOfWeights();

        STATUS(printer("Signal for generation:", pS) << std::endl);
        STATUS(printer("Background for generation:", pB) << std::endl);

        JASTRONOMY::JPseudoExperiment pi(pS, pB, ps, pb);

        pi.nuisance = setup.nuisance;
        pi.set(Fs, Fb); // Set the strength of signal and background for generation

        if (M != 0) {
          pi.configure(M);
        }

        px_vec.push_back(pi);
      }
    }
  };
}


/**
 * \file
 * Application for generating conditional Likelihood histograms
 * of part of a dataset compared to the complete dataset.
 * Used to generate trial factors with JTrialFactorsFromCondNLLR.cc
 *
 * \author mdejong fvazquezdesola
 */
int main(int argc, char **argv)
{
  using namespace std;
  using namespace JPP;

  typedef JAbstractHistogram<double> histogram_type;

  JContainer< vector<experiment_type> >  setup_vec1;
  JContainer< vector<experiment_type> >  setup_vec2;
  string              outputFile;
  size_t              numberOfTests;
  double              Fs;
  double              Fb;
  size_t              M;
  double              SNR;
  histogram_type      X;
  JRandom             seed;
  int                 debug;

  try { 

    JParser<> zap;
    JPseudoExperiment px;

    zap['E'] = make_field(setup_vec1,
                          "inputs for 1st data period: signal and background histograms for generation and evaluation,\n"
                          << "\texposure boost, and signal and background nuisances, provided as\n"
                          << "\t'<file>:<hgen_s name> <file>:<hgen_b name> <file>:<heval_s name> <file>:<heval_b name> <boost> <type_s> (values) <type_b> (values)' \n"
                          << "\twhere <type> can be:" << get_keys(nuisance_helper) <<"\n."
                          << "\t Can be called repeatedly to add multiple datasets to this period");
    zap['F'] = make_field(setup_vec2,
                          "inputs for 2nd data period: signal and background histograms for generation and evaluation,\n"
                          << "\texposure boost, and signal and background nuisances, provided as\n"
                          << "\t'<file>:<hgen_s name> <file>:<hgen_b name> <file>:<heval_s name> <file>:<heval_b name> <boost> <type_s> (values) <type_b> (values)' \n"
                          << "\twhere <type> can be:" << get_keys(nuisance_helper) <<"\n."
                          << "\t Can be called repeatedly to add multiple datasets to this period");
    zap['o'] = make_field(outputFile,       "output file with histograms")                  = "CondNLLR.root";
    zap['s'] = make_field(Fs,               "signal strength");
    zap['b'] = make_field(Fb,               "background strength")                          = 1.0;
    zap['M'] = make_field(M,                "lookup table for CDFs")                        = 0;
    zap['R'] = make_field(SNR,              "signal-to-noise ratio")                        = 0.0;
    zap['n'] = make_field(numberOfTests,    "number of tests / PEs");
    zap['x'] = make_field(X,                "x-axis for likelihood histogram")              = histogram_type(200, 0, +20.0);
    zap['S'] = make_field(seed)             = 0;
    zap['d'] = make_field(debug)            = 1;

    zap(argc, argv);
  }
  catch(const exception& error) {
    FATAL(error.what() << endl);
  }


  seed.set(gRandom);

  JExperiment::setSNR(SNR);

  cout << "**Setting up 1st period**" << endl;
  experiment_group pxg_1(setup_vec1, Fs, Fb, M);
  cout << "**Setting up 2nd period**" << endl;
  experiment_group pxg_2(setup_vec2, Fs, Fb, M);

  TFile out(outputFile.c_str(), "recreate");

  TH2D h2d_nllr("h2d_nllr", "Negative Log-Likelihood Ratio; NLLR 1st period; NLLR total",
                X.getNumberOfBins(), X.getLowerLimit(), X.getUpperLimit(),
                X.getNumberOfBins(), X.getLowerLimit(), X.getUpperLimit());
  TH2D h2d_nllr_constr("h2d_nllr_constr", "Negative Log-Likelihood Ratio (filtered); NLLR 1st period; NLLR total",
                       X.getNumberOfBins(), X.getLowerLimit(), X.getUpperLimit(),
                       X.getNumberOfBins(), X.getLowerLimit(), X.getUpperLimit());

  size_t nexp_passfilter = 0;

  for (size_t nexp =0; nexp != numberOfTests; ++nexp) {
    if (nexp%100000 == 0) {
      cout << "Done " << nexp << " pseudo-experiments out of " << numberOfTests << endl;
    }

    JAspera aspera;
    JAspera::fit_type result_1;
    JAspera::fit_type result_A;

    for (const auto& px : pxg_1.px_vec) { px.run(aspera); }
    result_1 = aspera();

    for (const auto& px : pxg_2.px_vec) { px.run(aspera); }
    result_A = aspera();

    auto NLLR_1 = result_1.likelihood;
    auto NLLR_A = result_A.likelihood;

    if (NLLR_1 < 0) {
      NLLR_1 = 0;
    }

    if (NLLR_A < 0) {
      NLLR_A = 0;
    }

    h2d_nllr.Fill(NLLR_1, NLLR_A, 1./numberOfTests);

    if( result_1.signal * pxg_1.signal_weight > 1) {
      nexp_passfilter++;
      h2d_nllr_constr.Fill(NLLR_1, NLLR_A, 1./numberOfTests);
    }
  }

  cout << nexp_passfilter << " PEs pass the first period filter, out of " << numberOfTests << endl;

  h2d_nllr.Write();
  h2d_nllr_constr.Write();

  out.Write();
  out.Close();
}

