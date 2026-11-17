#include "phais.hpp"

#include <cstdlib>

int main() {
    std::srand(static_cast<unsigned>(phais::get_seed()));
    for (;;) {
        for (int i = 0; i < 8; ++i) {
            const int x = phais::get_bot_x(0, i);
            const int y = phais::get_bot_y(0, i);
            if (x < 0 || y < 0) {
                continue;
            }
            const auto direction = static_cast<phais::Direction>(std::rand() % 4);
            // Drag on enemy or neutral territory, but leave friendly cows alone.
            const int drag_index = phais::get_productivity(x, y) <= 0 ? 0 : -1;
            phais::submit_move(i, direction, drag_index);
        }
        phais::end_turn();
    }
}
