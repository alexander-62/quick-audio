// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
namespace qa {
// All sizes are XAML DIPs. Reserve native controls, sibling cards and outer spacing.
inline double PanelViewport(double workHeight,double nativeHeight,double surroundingHeight){
    return std::clamp(workHeight-nativeHeight-surroundingHeight-40.0,0.0,300.0);
}
}
