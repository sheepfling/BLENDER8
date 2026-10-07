#include "blender8/view/canvas.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <stdexcept>
namespace b8::view {
namespace {
// Original 5x7 bitmap alphabet stored as rows. No external font assets or downloads.
std::array<unsigned,7> glyph(char c){
#define G(a,b,c,d,e,f,g) return {a,b,c,d,e,f,g}
 switch(static_cast<char>(std::toupper(static_cast<unsigned char>(c)))){
 case 'A':G(14,17,17,31,17,17,17);case 'B':G(30,17,17,30,17,17,30);case 'C':G(14,17,16,16,16,17,14);
 case 'D':G(30,17,17,17,17,17,30);case 'E':G(31,16,16,30,16,16,31);case 'F':G(31,16,16,30,16,16,16);
 case 'G':G(14,17,16,23,17,17,15);case 'H':G(17,17,17,31,17,17,17);case 'I':G(14,4,4,4,4,4,14);
 case 'J':G(7,2,2,2,18,18,12);case 'K':G(17,18,20,24,20,18,17);case 'L':G(16,16,16,16,16,16,31);
 case 'M':G(17,27,21,21,17,17,17);case 'N':G(17,25,21,19,17,17,17);case 'O':G(14,17,17,17,17,17,14);
 case 'P':G(30,17,17,30,16,16,16);case 'Q':G(14,17,17,17,21,18,13);case 'R':G(30,17,17,30,20,18,17);
 case 'S':G(15,16,16,14,1,1,30);case 'T':G(31,4,4,4,4,4,4);case 'U':G(17,17,17,17,17,17,14);
 case 'V':G(17,17,17,17,17,10,4);case 'W':G(17,17,17,21,21,27,17);case 'X':G(17,17,10,4,10,17,17);
 case 'Y':G(17,17,10,4,4,4,4);case 'Z':G(31,1,2,4,8,16,31);
 case '0':G(14,17,19,21,25,17,14);case '1':G(4,12,4,4,4,4,14);case '2':G(14,17,1,2,4,8,31);
 case '3':G(30,1,1,14,1,1,30);case '4':G(2,6,10,18,31,2,2);case '5':G(31,16,16,30,1,1,30);
 case '6':G(14,16,16,30,17,17,14);case '7':G(31,1,2,4,8,8,8);case '8':G(14,17,17,14,17,17,14);
 case '9':G(14,17,17,15,1,1,14);case '.':G(0,0,0,0,0,12,12);case ':':G(0,12,12,0,12,12,0);
 case '-':G(0,0,0,31,0,0,0);case '+':G(0,4,4,31,4,4,0);case '/':G(1,2,2,4,8,8,16);
 case '%':G(17,2,4,4,8,16,17);case '=':G(0,0,31,0,31,0,0);case '_':G(0,0,0,0,0,0,31);
 case '>':G(16,8,4,2,4,8,16);case '<':G(1,2,4,8,4,2,1);case '[':G(14,8,8,8,8,8,14);
 case ']':G(14,2,2,2,2,2,14);case '(':G(2,4,8,8,8,4,2);case ')':G(8,4,2,2,2,4,8);
 case '&':G(12,18,20,8,21,18,13);case '?':G(14,17,1,2,4,0,4);case '!':G(4,4,4,4,4,0,4);case '|':G(4,4,4,4,4,4,4);
 default:G(0,0,0,0,0,0,0);
 }
#undef G
}
}
Canvas::Canvas(unsigned w,unsigned h){resize(w,h);}
void Canvas::resize(unsigned w,unsigned h){
 if(w<320||h<240||w>1920||h>1440)throw std::out_of_range("canvas bounds: 320..1920 by 240..1440");
 width_=w;height_=h;scale_=std::min(w/logical_width,h/logical_height);ox_=(w-logical_width*scale_)/2;oy_=(h-logical_height*scale_)/2;
 pixels_.resize(static_cast<std::size_t>(w)*h*4);
}
Point Canvas::device(Point p)const noexcept{return {p.x*scale_+ox_,p.y*scale_+oy_};}
Point Canvas::logical(Point p)const noexcept{return {(p.x-ox_)/scale_,(p.y-oy_)/scale_};}
void Canvas::blend(int x,int y,Color c){
 if(x<0||y<0||x>=static_cast<int>(width_)||y>=static_cast<int>(height_))return;
 auto* p=pixels_.data()+(static_cast<std::size_t>(y)*width_+static_cast<unsigned>(x))*4;
 if(c.a==255){p[0]=c.r;p[1]=c.g;p[2]=c.b;p[3]=255;return;}
 const unsigned a=c.a,b=255-a;
 p[0]=static_cast<std::uint8_t>((c.r*a+p[0]*b+127)/255);p[1]=static_cast<std::uint8_t>((c.g*a+p[1]*b+127)/255);
 p[2]=static_cast<std::uint8_t>((c.b*a+p[2]*b+127)/255);p[3]=255;
}
void Canvas::clear(Color c){for(std::size_t i=0;i<pixels_.size();i+=4){pixels_[i]=c.r;pixels_[i+1]=c.g;pixels_[i+2]=c.b;pixels_[i+3]=255;}}
void Canvas::rect(Rect r,Color c,double radius){
 auto p=device({r.x,r.y});const double w=r.w*scale_,h=r.h*scale_,rad=std::clamp(radius*scale_,0.0,std::min(w,h)/2);
 int x0=std::max(0,static_cast<int>(std::floor(p.x))),y0=std::max(0,static_cast<int>(std::floor(p.y)));
 int x1=std::min(static_cast<int>(width_),static_cast<int>(std::ceil(p.x+w))),y1=std::min(static_cast<int>(height_),static_cast<int>(std::ceil(p.y+h)));
 for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
   const double dx=std::max({p.x+rad-(x+.5),(x+.5)-(p.x+w-rad),0.0}),dy=std::max({p.y+rad-(y+.5),(y+.5)-(p.y+h-rad),0.0});
   if(dx*dx+dy*dy<=rad*rad)blend(x,y,c);
 }
}
void Canvas::ellipse(Point center,double rx,double ry,Color c){
 auto p=device(center);rx*=scale_;ry*=scale_;if(rx<=0||ry<=0)return;
 const int y0=std::max(0,static_cast<int>(std::floor(p.y-ry))),y1=std::min(static_cast<int>(height_),static_cast<int>(std::ceil(p.y+ry)));
 for(int y=y0;y<y1;++y){const double dy=(y+.5-p.y)/ry;if(std::abs(dy)>1)continue;const double extent=rx*std::sqrt(1-dy*dy);
 const int x0=std::max(0,static_cast<int>(std::ceil(p.x-extent-.5))),x1=std::min(static_cast<int>(width_),static_cast<int>(std::ceil(p.x+extent-.5)));
 for(int x=x0;x<x1;++x)blend(x,y,c);}
}
void Canvas::line(Point a,Point b,Color c,double w){
 const double dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);if(len<1e-9){ellipse(a,w/2,w/2,c);return;}
 const double nx=-dy/len*w/2,ny=dx/len*w/2;
 const std::array<Point,4> q{{{a.x+nx,a.y+ny},{b.x+nx,b.y+ny},{b.x-nx,b.y-ny},{a.x-nx,a.y-ny}}};polygon(q,c);
}
void Canvas::polygon(std::span<const Point> points,Color c){
 if(points.size()<3||points.size()>128)return;
 std::vector<Point> p;double lo=static_cast<double>(height_),hi=0;
 for(auto v:points){v=device(v);lo=std::min(lo,v.y);hi=std::max(hi,v.y);p.push_back(v);}
 std::vector<double> hits;hits.reserve(p.size());
 for(int y=std::max(0,static_cast<int>(std::floor(lo)));y<std::min(static_cast<int>(height_),static_cast<int>(std::ceil(hi)));++y){
 hits.clear();double scan=y+.5;
 for(std::size_t i=0;i<p.size();++i){auto a=p[i],b=p[(i+1)%p.size()];if((a.y<=scan&&b.y>scan)||(b.y<=scan&&a.y>scan))hits.push_back(a.x+(scan-a.y)*(b.x-a.x)/(b.y-a.y));}
 std::sort(hits.begin(),hits.end());for(std::size_t k=1;k<hits.size();k+=2)for(int x=std::max(0,static_cast<int>(std::ceil(hits[k-1]-.5)));x<std::min(static_cast<int>(width_),static_cast<int>(std::ceil(hits[k]-.5)));++x)blend(x,y,c);
 }
}
void Canvas::text(std::string_view str,Point at,double size,Color c){
 const double start=at.x;
 for(char ch:str){if(ch=='\n'){at.x=start;at.y+=size*9;continue;}const auto rows=glyph(ch);
 for(unsigned y=0;y<7;++y)for(unsigned x=0;x<5;++x)if(rows[y]&(1u<<(4-x)))rect({at.x+x*size,at.y+y*size,size,size},c);
 at.x+=size*6;}
}
std::uint64_t Canvas::hash()const noexcept{std::uint64_t h=1469598103934665603ull;for(auto x:pixels_){h^=x;h*=1099511628211ull;}return h;}
}
