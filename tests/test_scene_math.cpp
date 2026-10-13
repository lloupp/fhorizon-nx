#include "scene.hpp"
#include <cstdlib>
#include <cmath>
#include <cstdio>
using namespace fhnx::scene;
static void check(bool b){if(!b)std::abort();}
static void close(float a,float b){check(std::abs(a-b)<.0001f);}
static std::array<float,4> point(Matrix m,std::array<float,4> p){std::array<float,4> o{};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)o[r]+=m[c*4+r]*p[c];return o;}
int main(){
 auto p=point(multiply(translate(3,4,5),translate(-1,2,0)),{1,2,3,1});close(p[0],3);close(p[1],8);close(p[2],8);close(p[3],1);
 auto view=lookAt({0,0,5},{0,0,0});auto origin=point(view,{0,0,0,1});close(origin[0],0);close(origin[1],0);close(origin[2],-5);
 auto projection=perspective(1,.1f,60);auto near=point(projection,{0,0,-.1f,1}),far=point(projection,{0,0,-60,1});close(near[2]/near[3],0);close(far[2]/far[3],1);
 Mesh box;box.box({0,0,0},{1,1,1},{1,1,1,1});check(box.vertices.size()==24 && box.indices.size()==36);for(auto i:box.indices)check(i<box.vertices.size());
 Mesh ground;ground.plane(-1,-1,1,1,0,{1,1,1,1});
 auto a=ground.vertices[0].position,b=ground.vertices[1].position,c=ground.vertices[2].position;
 check(cross({b[0]-a[0],b[1]-a[1],b[2]-a[2]},{c[0]-a[0],c[1]-a[1],c[2]-a[2]}).y>0);
 std::puts("scene math: matrix order, look-at, near/far depth and mesh indices PASS");
}
