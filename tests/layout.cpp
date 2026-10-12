// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/layout.h"
#include <cassert>
#include <iostream>
int main(){
    auto normal=qa::PanelViewport(800,340,20);
    auto media=qa::PanelViewport(800,340,210);
    assert(normal==300 && media==210 && media<normal);
    assert(qa::PanelViewport(600,340,210)==10);
    assert(qa::PanelViewport(500,340,210)==0);
    assert(qa::PanelViewport(1600,340,20)==300);
    for(double work:{480.,600.,800.,1080.})
        for(double native:{240.,340.,420.})
            for(double cards:{0.,180.,250.}){
                auto budget=qa::PanelViewport(work,native,cards);
                assert(budget>=0 && budget<=300);
                if(work>=native+cards+40)assert(budget+native+cards+40<=work);
                assert(qa::PanelViewport(work,native,cards+30)<=budget);
            }
    // Convert work area pixels to DIPs before applying the same budget.
    assert(qa::PanelViewport(1200*96./144,340,210)==media);
    assert(qa::PanelViewport(1600*96./192,340,210)==media);
    // A 174-DIP tile viewport can have a second-row caption extending to 202.
    auto tiles=qa::NativeRowHeight(174,174,202);
    assert(tiles==202);
    assert(qa::NativeRowHeight(174,tiles,174)==202);
    assert(qa::NativeRowHeight(202,202,180)==202);
    auto before=qa::PanelViewport(800,340,210);
    auto after=qa::PanelViewport(800,340+(tiles-174),210);
    assert(before-after==28);
    assert(after+340+28+210+40==800);
    assert(qa::PanelViewport(600,340+28,210)==0);
    std::cout<<"Layout budgets passed\n";
}
