// Quick test: parse a .gplayer file and print summary
// Build: g++ -std=c++17 -o /tmp/swordigo-save-lab/test_save test_save_editor.cpp save_editor.cpp -I../../ -I../
// Run:   /tmp/swordigo-save-lab/test_save /tmp/swordigo-save-lab/7percentcomplete.gplayer

#include "platform/save_editor.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.gplayer>" << std::endl;
        return 1;
    }
    
    SaveFile sf;
    if (!save_load(argv[1], sf)) {
        std::cerr << "Failed to load save file!" << std::endl;
        return 1;
    }
    
    // Print summary
    std::cout << save_summary(sf) << std::endl;
    
    // Test round-trip: write to temp, re-read, compare
    std::string test_path = std::string(argv[1]) + ".roundtrip";
    if (save_write(test_path, sf)) {
        SaveFile sf2;
        if (save_load(test_path, sf2)) {
            std::cout << "\n=== ROUND-TRIP TEST ===" << std::endl;
            std::cout << "Original items: " << sf.game_state.character.items.size() << std::endl;
            std::cout << "Roundtrip items: " << sf2.game_state.character.items.size() << std::endl;
            std::cout << "Health: " << sf.game_state.character.health << " -> " << sf2.game_state.character.health << std::endl;
            std::cout << "Coins: " << sf.game_state.character.coins << " -> " << sf2.game_state.character.coins << std::endl;
            std::cout << "Levels: " << sf.game_state.levels.size() << " -> " << sf2.game_state.levels.size() << std::endl;
            
            bool match = (sf.game_state.character.health == sf2.game_state.character.health &&
                         sf.game_state.character.coins == sf2.game_state.character.coins &&
                         sf.game_state.character.items.size() == sf2.game_state.character.items.size() &&
                         sf.game_state.levels.size() == sf2.game_state.levels.size());
            std::cout << (match ? "✅ PASS" : "❌ FAIL") << std::endl;
        }
    }
    
    // Test modify: give 9999 coins
    sf.game_state.character.coins = 9999;
    sf.game_state.character.health = 999;
    std::string mod_path = std::string(argv[1]) + ".modded";
    if (save_write(mod_path, sf)) {
        SaveFile sf3;
        save_load(mod_path, sf3);
        std::cout << "\n=== MODIFY TEST ===" << std::endl;
        std::cout << "Coins: " << sf3.game_state.character.coins << " (expected 9999)" << std::endl;
        std::cout << "Health: " << sf3.game_state.character.health << " (expected 999)" << std::endl;
        std::cout << (sf3.game_state.character.coins == 9999 ? "✅ PASS" : "❌ FAIL") << std::endl;
    }
    
    return 0;
}
