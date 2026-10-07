#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
namespace b8::view {
struct Color { std::uint8_t r,g,b,a=255; };
struct Point { double x,y; };
struct Rect { double x,y,w,h; [[nodiscard]] bool contains(Point p)const noexcept{return p.x>=x&&p.x<x+w&&p.y>=y&&p.y<y+h;} };
// C++ software renderer. All pixels are generated here, natively or in Wasm.
// Browser glue may only present this RGBA frame; it has no scene drawing logic.
class Canvas final {
public:
    static constexpr double logical_width=1280, logical_height=900;
    Canvas(unsigned width=1280,unsigned height=900);
    void resize(unsigned width,unsigned height);
    void clear(Color color);
    void rect(Rect r,Color color,double radius=0);
    void line(Point a,Point b,Color color,double width=1);
    void ellipse(Point center,double rx,double ry,Color color);
    void polygon(std::span<const Point> points,Color color);
    void text(std::string_view text,Point at,double size,Color color);
    [[nodiscard]] Point logical(Point physical)const noexcept;
    [[nodiscard]] unsigned width()const noexcept{return width_;}
    [[nodiscard]] unsigned height()const noexcept{return height_;}
    [[nodiscard]] const std::vector<std::uint8_t>& pixels()const noexcept{return pixels_;}
    [[nodiscard]] std::uint64_t hash()const noexcept;
private:
    void blend(int x,int y,Color color);
    [[nodiscard]] Point device(Point p)const noexcept;
    unsigned width_=0,height_=0; double scale_=1,ox_=0,oy_=0;
    std::vector<std::uint8_t> pixels_;
};
}
