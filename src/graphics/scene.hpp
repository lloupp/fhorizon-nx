#pragma once
// Original bounded geometry/matrix utilities for native graphics clients.
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "commands.hpp"
namespace fhnx::scene {
struct Vec3 {float x,y,z;};
inline Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Vec3 norm(Vec3 a){float n=std::sqrt(dot(a,a));gpu::require(n>0,"zero camera direction");return {a.x/n,a.y/n,a.z/n};}
using Matrix=std::array<float,16>;
inline Matrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
inline Matrix multiply(const Matrix& a,const Matrix& b){Matrix m{};for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)m[c*4+r]+=a[k*4+r]*b[c*4+k];return m;}
inline Matrix translate(float x,float y,float z){auto m=identity();m[12]=x;m[13]=y;m[14]=z;return m;}
inline Matrix perspective(float aspect,float near,float far){gpu::require(aspect>0 && near>0 && far>near,"perspective range");float f=1/std::tan(.85f/2);Matrix m{};m[0]=f/aspect;m[5]=f;m[10]=far/(near-far);m[11]=-1;m[14]=near*far/(near-far);return m;}
inline Matrix lookAt(Vec3 eye,Vec3 target){auto f=norm(sub(target,eye)),r=norm(cross(f,{0,1,0})),u=cross(r,f);return {r.x,u.x,-f.x,0,r.y,u.y,-f.y,0,r.z,u.z,-f.z,0,-dot(r,eye),-dot(u,eye),dot(f,eye),1};}
struct Vertex {float position[3],color[4],uv[2];};
struct alignas(16) Constants {Matrix transform;float tint[4]{1,1,1,1};};
inline gpu::VertexLayout layout(){return {sizeof(Vertex),3,{{{0,3},{12,4},{28,2}}}};}
struct Mesh {
 std::vector<Vertex> vertices;std::vector<uint16_t> indices;
 void quad(Vec3 a,Vec3 b,Vec3 c,Vec3 d,std::array<float,4> color,float repeat=1){
  gpu::require(vertices.size()+4<=65536,"mesh uint16 vertex budget");uint16_t first=uint16_t(vertices.size());
  Vec3 points[]={a,b,c,d};const float uv[4][2]={{0,0},{repeat,0},{repeat,repeat},{0,repeat}};
  for(unsigned i=0;i<4;++i)vertices.push_back({{points[i].x,points[i].y,points[i].z},{color[0],color[1],color[2],color[3]},{uv[i][0],uv[i][1]}});
  for(uint16_t i:{0,1,2,0,2,3})indices.push_back(first+i);
 }
 void plane(float x0,float z0,float x1,float z1,float y,std::array<float,4> color,float repeat=1){quad({x0,y,z0},{x0,y,z1},{x1,y,z1},{x1,y,z0},color,repeat);}
 void box(Vec3 center,Vec3 half,std::array<float,4> color){
  float x=center.x,y=center.y,z=center.z,a=half.x,b=half.y,c=half.z;
  quad({x-a,y-b,z-c},{x+a,y-b,z-c},{x+a,y+b,z-c},{x-a,y+b,z-c},color);
  quad({x-a,y-b,z+c},{x+a,y-b,z+c},{x+a,y+b,z+c},{x-a,y+b,z+c},color);
  quad({x-a,y-b,z-c},{x-a,y-b,z+c},{x-a,y+b,z+c},{x-a,y+b,z-c},color);
  quad({x+a,y-b,z-c},{x+a,y-b,z+c},{x+a,y+b,z+c},{x+a,y+b,z-c},color);
  plane(x-a,z-c,x+a,z+c,y+b,color);plane(x-a,z-c,x+a,z+c,y-b,color);
 }
};
}
