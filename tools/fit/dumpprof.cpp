// Every reference note's harmonic profile, HONKR's (without the fitted body response) and the recording's,
// for tools/fit/fiteq.cpp. Run from the repository root.
#include "../../tests/reference_match.hpp"
#include <cstdio>
int main(){
  honkr::dsp::winds::g_body_eq_off = true;
  auto refs=reference::load_features("references/features.tsv");
  std::vector<reference::Played> d; reference::score(refs,48000,&d);
  for(auto&p:d){ std::printf("%s\t%s\t%d\t%.3f",p.reference.instrument.c_str(),p.reference.dynamic.c_str(),p.reference.key,p.reference.f.f0);
    for(int h=0;h<10;++h) std::printf("\t%.2f",p.model.profile[h]); for(int h=0;h<10;++h) std::printf("\t%.2f",p.reference.f.profile[h]); std::printf("\n"); }
}
