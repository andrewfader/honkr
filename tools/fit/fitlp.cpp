// Fitting tool behind docs/PHYSICS.md. Build and run from the repository root:
//   g++ -O2 -std=c++20 -o build/fitlp tools/fit/fitlp.cpp -lpthread && build/fitlp "Alto sax"
#include "../../tests/reference_match.hpp"
#include <cstdio>
// fitlp "instrument" : grid over lp_pp, lp_ff, order; prints scores sorted by objective
using namespace honkr::dsp::winds;
int main(int argc,char**argv){
  auto all=reference::load_features("references/features.tsv"); std::vector<reference::Note> refs;
  for(auto&n:all) if(n.instrument==argv[1]) refs.push_back(n);
  struct R{double pp,ff;int o;reference::Score s;double obj;}; std::vector<R> rs;
  auto run=[&](double pp,double ff,int o){ g_dyn_lp_pp=pp; g_dyn_lp_ff=ff; g_dyn_lp_order=o; auto s=reference::score(refs,48000).begin()->second;
    double obj=s.profile_db+10*s.brightness+10*s.dynamic_bright+0.3*s.odd_even_db; rs.push_back({pp,ff,o,s,obj}); };
  run(0,0,2);
  double pps[]={1.5,2,3,4,6,9}, ffs[]={3,4,6,9,13,20,30};
  for(int o:{2,4}) for(double pp:pps) for(double ff:ffs) if(ff>=pp) run(pp,ff,o);
  std::sort(rs.begin()+1,rs.end(),[](auto&a,auto&b){return a.obj<b.obj;});
  for(size_t i=0;i<std::min<size_t>(rs.size(),8);++i){auto&r=rs[i]; std::printf("%-12s pp %4.1f ff %4.1f o%d  obj %6.2f profile %5.2f bright %.3f oe %5.2f noise %5.2f dyn %5.2f dynb %.3f\n",argv[1],r.pp,r.ff,r.o,r.obj,r.s.profile_db,r.s.brightness,r.s.odd_even_db,r.s.noise_db,r.s.dynamic_db,r.s.dynamic_bright);}
}
