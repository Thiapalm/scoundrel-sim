#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <random>
#include <iomanip>
#include <algorithm>
#include <map>

#include "GameLogic.h"
#include "DefaultGameContextFactory.h"
#include "PlayerClassFactory.h"
#include "ui/UserInterface.h"
#include "gamecontext.h"
#include "actions/action.h"
#include "assets/room.h"
#include "assets/cards.h"

class SimulationUI : public UserInterface {
public:
    bool victory = false;
    int score = 0;
    bool game_ended = false;
    std::string defeat_reason;
    GameContext* m_ctx = nullptr;
    int m_last_action_idx = -1;

    std::optional<int> select_game_mode() override { return 1; } // Standard
    std::optional<int> select_player_class() override { return 0; } 
    
    void display_game_state(const GameContext& ctx) override {}
    void display_valid_actions(std::span<Action* const> actions) override {}
    
    std::optional<int> request_action_index(int max_index) override {
        if (!m_ctx) return 0;
        auto actions = m_ctx->get_turn_valid_actions();
        m_last_action_idx = pick_action(*m_ctx, actions);
        return m_last_action_idx;
    }

    std::optional<int> request_card_index(int max_cards) override {
        if (!m_ctx) return 1;
        return pick_card(*m_ctx, max_cards);
    }

    int pick_action(const GameContext& ctx, const std::vector<Action*>& actions) {
        auto* room = ctx.get_room();
        if (!room) return 0;

        auto can_actually_execute = [&](Action* action) -> bool {
            if (action->name() == "Skip Room" || action->name() == "New Room") {
                return action->can_execute(ctx, nullptr);
            }
            for (int i = 0; i < room->cards_in_room(); ++i) {
                if (action->can_execute(ctx, &room->look_card(i))) return true;
            }
            return false;
        };

        auto find_action = [&](const std::string& name) -> int {
            for (size_t i = 0; i < actions.size(); ++i) {
                if (actions[i]->name() == name && can_actually_execute(actions[i])) return static_cast<int>(i);
            }
            return -1;
        };

        int idx;
        if ((idx = find_action("New Room")) != -1) return idx;
        if (ctx.get_player()->current_life() < 12 && (idx = find_action("Drink Potion")) != -1) return idx;
        if (!ctx.get_weapon() && (idx = find_action("Attach Weapon")) != -1) return idx;
        if (ctx.get_weapon() && (idx = find_action("Attack Monster")) != -1) return idx;
        if ((idx = find_action("Skip Room")) != -1) return idx;
        if ((idx = find_action("Attack Monster No Weapon")) != -1) return idx;
        if ((idx = find_action("Hand Combat")) != -1) return idx;
        if ((idx = find_action("Attack Monster")) != -1) return idx;
        if ((idx = find_action("Attach Weapon")) != -1) return idx;
        if ((idx = find_action("Drink Potion")) != -1) return idx;
        
        return 0;
    }

    int pick_card(const GameContext& ctx, int count) {
        auto* room = ctx.get_room();
        if (!room) return 1;

        int best_idx = -1;
        int best_val = -1;
        
        Action* current_action = nullptr;
        auto valid = ctx.get_turn_valid_actions();
        if (m_last_action_idx >= 0 && m_last_action_idx < static_cast<int>(valid.size())) {
            current_action = valid[m_last_action_idx];
        }

        if (!current_action) return 1;

        if (current_action->name() == "Drink Potion") {
            for (int i = 0; i < room->cards_in_room(); ++i) {
                if (current_action->can_execute(ctx, &room->look_card(i))) {
                    int val = static_cast<int>(room->look_card(i).get_face()) + 2;
                    if (val > best_val) {
                        best_val = val;
                        best_idx = i + 1;
                    }
                }
            }
        } else if (current_action->name() == "Attach Weapon") {
            for (int i = 0; i < room->cards_in_room(); ++i) {
                if (current_action->can_execute(ctx, &room->look_card(i))) {
                    int val = static_cast<int>(room->look_card(i).get_face()) + 2;
                    if (val > best_val) {
                        best_val = val;
                        best_idx = i + 1;
                    }
                }
            }
        } else if (current_action->name() == "Attack Monster") {
            best_val = 100;
            for (int i = 0; i < room->cards_in_room(); ++i) {
                if (current_action->can_execute(ctx, &room->look_card(i))) {
                    int val = static_cast<int>(room->look_card(i).get_face()) + 2;
                    if (val < best_val) {
                        best_val = val;
                        best_idx = i + 1;
                    }
                }
            }
        } else if (current_action->name() == "Hand Combat" || current_action->name() == "Attack Monster No Weapon") {
            best_val = 100;
            for (int i = 0; i < room->cards_in_room(); ++i) {
                if (current_action->can_execute(ctx, &room->look_card(i))) {
                    int val = static_cast<int>(room->look_card(i).get_face()) + 2;
                    if (val < best_val) {
                        best_val = val;
                        best_idx = i + 1;
                    }
                }
            }
        }

        return (best_idx != -1) ? best_idx : 1;
    }
    
    void show_message(const std::string& message) override {}
    void show_error(const std::string& error) override {}
    
    void show_victory(int s) override {
        victory = true;
        score = s;
        game_ended = true;
    }
    void show_defeat(int s, const std::string& reason) override {
        victory = false;
        score = s;
        game_ended = true;
        defeat_reason = reason;
    }
    bool ask_restart() override { return false; }
    void reset() override {
        victory = false;
        score = 0;
        game_ended = false;
        defeat_reason = "";
        m_ctx = nullptr;
        m_last_action_idx = -1;
    }
};

struct Stats {
    int games = 0;
    int wins = 0;
    int total_score = 0;
    int max_score = 0;
    std::map<std::string, int> death_reasons;

    void add_game(bool victory, int score, const std::string& reason) {
        games++;
        if (victory) wins++;
        total_score += score;
        if (score > max_score) max_score = score;
        if (!victory) death_reasons[reason]++;
    }

    double win_rate() const { return games == 0 ? 0 : (double)wins / games * 100.0; }
    double avg_score() const { return games == 0 ? 0 : (double)total_score / games; }
};

void run_simulations(PlayerClassType class_type, int iterations, const std::string& label) {
    Stats stats;
    auto ui = std::make_unique<SimulationUI>();
    auto factory = std::make_unique<DefaultGameContextFactory>();
    std::random_device rd;
    
    for (int i = 0; i < iterations; ++i) {
        ui->reset();
        auto game = GameLogic::create(GameType::DEFAULT);
        factory->set_player(create_player(class_type));
        auto ctx = factory->create_game_context(rd());
        ui->m_ctx = ctx.get();
        game->set_context(std::move(ctx));
        game->set_interface(ui.get());
        game->run_game_loop();
        stats.add_game(ui->victory, ui->score, ui->defeat_reason);
    }

    std::cout << std::left << std::setw(15) << label 
              << std::right << std::setw(10) << stats.games 
              << std::setw(12) << std::fixed << std::setprecision(2) << stats.win_rate() << "%"
              << std::setw(12) << stats.avg_score() 
              << std::setw(10) << stats.max_score << std::endl;

    // Print top 3 death reasons
    std::vector<std::pair<std::string, int>> reasons(stats.death_reasons.begin(), stats.death_reasons.end());
    std::sort(reasons.begin(), reasons.end(), [](auto& a, auto& b) { return a.second > b.second; });
    
    int count = 0;
    for (auto& r : reasons) {
        if (count++ >= 3) break;
        std::string display_reason = r.first.empty() ? "Death by Damage" : r.first;
        std::cout << "  - " << display_reason << ": " << r.second << std::endl;
    }
}

int main(int argc, char** argv) {
    int iterations = 1000;
    if (argc > 1) {
        try {
            iterations = std::stoi(argv[1]);
        } catch (...) {}
    }

    std::cout << "Running Scoundrel Balance Simulation (" << iterations << " games per class)..." << std::endl;
    std::cout << std::string(65, '-') << std::endl;
    std::cout << std::left << std::setw(15) << "Class" 
              << std::right << std::setw(10) << "Games" 
              << std::setw(13) << "Win Rate" 
              << std::setw(12) << "Avg Score" 
              << std::setw(10) << "Max Score" << std::endl;
    std::cout << std::string(65, '-') << std::endl;

    run_simulations(PlayerClassType::PEASANT, iterations, "Peasant");
    run_simulations(PlayerClassType::WARRIOR, iterations, "Warrior");
    run_simulations(PlayerClassType::HEALER, iterations, "Healer");

    std::cout << std::string(65, '-') << std::endl;

    return 0;
}
