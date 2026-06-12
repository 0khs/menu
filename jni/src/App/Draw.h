#pragma once

#include "TouchHelperA.h"
#include "Menu.h"

inline void DrawFrame() {
    CPU::Tick();
    DrawMenu();
    Touch::SetMenuBounds(
        LastCoordinate.Pos_x,
        LastCoordinate.Pos_y,
        LastCoordinate.Size_x,
        LastCoordinate.Size_y
    );
}
