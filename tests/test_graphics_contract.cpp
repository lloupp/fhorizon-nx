#include "commands.hpp"
#include <cstdio>
#include <cstdlib>
using namespace fhnx::gpu;
static void check(bool b) { if(!b) std::abort(); }
template<class F> static void rejects(F f) { bool caught=false; try { f(); } catch(const std::runtime_error&) {caught=true;} check(caught); }
int main() {
    Images m(8192);
    rejects([&]{ m.insert({0,1},4); });
    rejects([&]{ m.insert({16,16,2},1024); });
    auto c=m.insert({16,16,4},4096);
    auto z=m.insert({16,16,4,Format::z24s8},2048);
    auto t=m.insert({16,16},1024);
    rejects([&]{m.insert({16,16},4096);});
    rejects([&]{m.targets(c,z);});
    m.transition(c,Usage::undefined,Usage::target);
    m.transition(z,Usage::undefined,Usage::target); m.targets(c,z);
    rejects([&]{m.transition(c,Usage::undefined,Usage::sampled);});
    rejects([&]{m.transition(c,Usage::target,Usage::sampled);});
    m.transition(c,Usage::target,Usage::resolve_source);
    m.transition(t,Usage::undefined,Usage::resolve_dest); m.resolve(c,t);
    rejects([&]{m.resolve(t,c);});
    m.transition(t,Usage::resolve_dest,Usage::sampled);
    rejects([&]{m.transition(t,Usage::sampled,Usage::present);});
    auto old=t; m.erase(t); rejects([&]{m.get(old);});
    auto fresh=m.insert({16,16},1024); check(fresh.slot==old.slot && fresh.generation!=old.generation);
    rejects([&]{m.erase(old);});
    m.erase(fresh); m.erase(c); m.erase(z); check(m.used==0);
    std::puts("graphics contract: budget, states, resolve, stale handles PASS");
}
