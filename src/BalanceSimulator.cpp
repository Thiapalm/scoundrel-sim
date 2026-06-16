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

struct SimCard {
    int original_index;
    CardType type;
    int value;
};

struct SimState {
    int hp;
    int max_hp;
    bool has_weapon;
    int weapon_damage;
    int weapon_last_monster_damage;
    int weapon_monsters_killed;
    bool potion_used;
    bool banished_this_room;
    int actions_taken;
    bool room_skipped;
    std::string player_class_name;
    SimCard room_cards[4];
    int num_cards;
};

struct SimMove {
    std::string action_name;
    int card_idx; // 1-based index (or -1)
};

inline void solve_dfs(const SimState& state, 
                      std::vector<SimMove>& path, 
                      std::vector<SimMove>& best_path, 
                      int& best_score) {
    if (state.hp <= 0) {
        int score = -100000 + state.actions_taken;
        if (score > best_score) {
            best_score = score;
            best_path = path;
        }
        return;
    }

    // Check if we can do New Room
    if (state.actions_taken == 3 || state.num_cards == 0) {
        int score = state.hp * 100;
        if (state.has_weapon) {
            score += state.weapon_damage * 15;
        }
        if (state.actions_taken == 3) {
            for (int i = 0; i < state.num_cards; ++i) {
                const auto& card = state.room_cards[i];
                if (card.type == CardType::Monster) {
                    score -= card.value * 12;
                } else if (card.type == CardType::Potion) {
                    score += card.value * 4;
                } else if (card.type == CardType::Weapon) {
                    score += card.value * 6;
                }
            }
        }
        
        path.push_back({"New Room", -1});
        if (score > best_score) {
            best_score = score;
            best_path = path;
        }
        path.pop_back();
    }

    // Check if we can do Skip Room
    if (state.actions_taken == 0 && !state.room_skipped) {
        int score = state.hp * 100;
        if (state.has_weapon) {
            score += state.weapon_damage * 15;
        }
        
        path.push_back({"Skip Room", -1});
        if (score > best_score) {
            best_score = score;
            best_path = path;
        }
        path.pop_back();
    }

    if (state.num_cards == 0) {
        return;
    }

    for (int i = 0; i < state.num_cards; ++i) {
        const auto& card = state.room_cards[i];
        
        auto apply_next_cards = [&](SimState& next_state, int remove_idx) {
            next_state.num_cards = 0;
            for (int j = 0; j < state.num_cards; ++j) {
                if (j != remove_idx) {
                    next_state.room_cards[next_state.num_cards++] = state.room_cards[j];
                }
            }
        };

        if (card.type == CardType::Potion) {
            SimState next_state = state;
            apply_next_cards(next_state, i);
            next_state.actions_taken++;
            
            bool can_heal = !state.potion_used || (state.player_class_name == "Wizard");
            if (can_heal) {
                next_state.hp = std::min(state.max_hp, state.hp + card.value);
            }
            next_state.potion_used = true;
            
            path.push_back({"Drink Potion", card.original_index + 1});
            solve_dfs(next_state, path, best_path, best_score);
            path.pop_back();
        }
        else if (card.type == CardType::Weapon) {
            SimState next_state = state;
            apply_next_cards(next_state, i);
            next_state.actions_taken++;
            
            next_state.has_weapon = true;
            next_state.weapon_damage = card.value;
            next_state.weapon_last_monster_damage = 0;
            next_state.weapon_monsters_killed = 0;
            
            path.push_back({"Attach Weapon", card.original_index + 1});
            solve_dfs(next_state, path, best_path, best_score);
            path.pop_back();
        }
        else if (card.type == CardType::Monster) {
            if (state.player_class_name == "Wizard" && !state.banished_this_room) {
                SimState next_state = state;
                apply_next_cards(next_state, i);
                next_state.actions_taken++;
                next_state.banished_this_room = true;
                
                path.push_back({"Banish Monster", card.original_index + 1});
                solve_dfs(next_state, path, best_path, best_score);
                path.pop_back();
            }

            if (state.has_weapon) {
                bool can_attack = (state.weapon_monsters_killed == 0) || 
                                  (card.value < state.weapon_last_monster_damage);
                if (can_attack) {
                    SimState next_state = state;
                    apply_next_cards(next_state, i);
                    next_state.actions_taken++;
                    
                    int damage_taken = std::max(0, card.value - state.weapon_damage);
                    next_state.hp = state.hp - damage_taken;
                    next_state.weapon_last_monster_damage = card.value;
                    next_state.weapon_monsters_killed++;
                    
                    path.push_back({"Attack Monster", card.original_index + 1});
                    solve_dfs(next_state, path, best_path, best_score);
                    path.pop_back();
                }
            }

            // Hand Combat / Attack Monster No Weapon
            {
                SimState next_state = state;
                apply_next_cards(next_state, i);
                next_state.actions_taken++;
                next_state.hp = state.hp - card.value;
                
                path.push_back({"Hand Combat", card.original_index + 1});
                solve_dfs(next_state, path, best_path, best_score);
                path.pop_back();

                path.push_back({"Attack Monster No Weapon", card.original_index + 1});
                solve_dfs(next_state, path, best_path, best_score);
                path.pop_back();
            }
        }
    }
}

class SimulationUI : public UserInterface {
public:
    bool victory = false;
    int score = 0;
    bool game_ended = false;
    std::string defeat_reason;
    GameContext* m_ctx = nullptr;
    int m_last_action_idx = -1;
    const Room* m_last_room = nullptr;
    bool m_banished_this_room = false;
    bool m_use_lookahead = false;
    int m_next_card_idx = -1;

    SimulationUI(bool use_lookahead = false) : m_use_lookahead(use_lookahead) {}

    std::optional<int> select_game_mode() override { return 1; } // Standard
    std::optional<int> select_player_class() override { return 0; } 
    
    void display_game_state(const GameContext& ctx) override {}
    void display_valid_actions(std::span<Action* const> actions) override {}
    
    std::optional<int> request_action_index(int max_index) override {
        if (!m_ctx) return 0;
        
        auto* room = m_ctx->get_room();
        if (room && room != m_last_room) {
            m_last_room = room;
            m_banished_this_room = false;
        }

        auto actions = m_ctx->get_turn_valid_actions();
        
        if (m_use_lookahead) {
            SimState state;
            state.hp = m_ctx->get_player()->current_life();
            state.max_hp = m_ctx->get_player()->get_max_health();
            state.has_weapon = m_ctx->get_weapon() != nullptr;
            state.weapon_damage = state.has_weapon ? m_ctx->get_weapon()->weapon_damage() : 0;
            state.weapon_last_monster_damage = state.has_weapon ? m_ctx->get_weapon()->last_monster_damage() : 0;
            state.weapon_monsters_killed = state.has_weapon ? m_ctx->get_weapon()->number_of_monsters() : 0;
            state.potion_used = m_ctx->is_potion_used();
            state.banished_this_room = m_banished_this_room;
            state.actions_taken = m_ctx->get_actions_taken();
            state.room_skipped = m_ctx->is_room_skipped();
            state.player_class_name = m_ctx->get_player()->getName();
            
            auto* room = m_ctx->get_room();
            if (room) {
                state.num_cards = 0;
                for (int i = 0; i < room->cards_in_room(); ++i) {
                    const auto& card = room->look_card(i);
                    SimCard sc;
                    sc.original_index = i;
                    sc.type = card.getType();
                    sc.value = static_cast<int>(card.get_face()) + 2;
                    state.room_cards[state.num_cards++] = sc;
                }
            }
            
            std::vector<SimMove> path;
            std::vector<SimMove> best_path;
            int best_score = -1000000;
            
            solve_dfs(state, path, best_path, best_score);
            
            if (!best_path.empty()) {
                std::string chosen_action_name = best_path[0].action_name;
                m_next_card_idx = best_path[0].card_idx;
                
                if (chosen_action_name == "Banish Monster") {
                    m_banished_this_room = true;
                }
                
                int idx = -1;
                for (size_t i = 0; i < actions.size(); ++i) {
                    if (actions[i]->name() == chosen_action_name) {
                        idx = static_cast<int>(i);
                        break;
                    }
                }
                if (idx == -1) {
                    if (chosen_action_name == "Hand Combat") {
                        for (size_t i = 0; i < actions.size(); ++i) {
                            if (actions[i]->name() == "Attack Monster No Weapon") {
                                idx = static_cast<int>(i);
                                break;
                            }
                        }
                    } else if (chosen_action_name == "Attack Monster No Weapon") {
                        for (size_t i = 0; i < actions.size(); ++i) {
                            if (actions[i]->name() == "Hand Combat") {
                                idx = static_cast<int>(i);
                                break;
                            }
                        }
                    }
                }
                
                if (idx != -1) {
                    m_last_action_idx = idx;
                    return m_last_action_idx;
                }
            }
        }
        
        m_last_action_idx = pick_action(*m_ctx, actions);
        return m_last_action_idx;
    }

    std::optional<int> request_card_index(int max_cards) override {
        if (!m_ctx) return 1;
        if (m_use_lookahead && m_next_card_idx != -1) {
            int card_idx = m_next_card_idx;
            m_next_card_idx = -1;
            return card_idx;
        }
        return pick_card(*m_ctx, max_cards);
    }

    int pick_action(const GameContext& ctx, const std::vector<Action*>& actions) {
        auto* room = ctx.get_room();
        if (!room) return 0;

        if (room != m_last_room) {
            m_last_room = room;
            m_banished_this_room = false;
        }

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

        // 1. Prioritize attaching a weapon if we don't have one
        if (!ctx.get_weapon() && (idx = find_action("Attach Weapon")) != -1) return idx;

        // 2. Prioritize attacking a monster if we have a weapon
        if (ctx.get_weapon() && (idx = find_action("Attack Monster")) != -1) return idx;

        // 3. Wizard Banish Monster logic:
        if (!m_banished_this_room && (idx = find_action("Banish Monster")) != -1) {
            bool should_banish = false;
            // Banish if we don't have a weapon, or if there's a strong monster in the room
            if (!ctx.get_weapon()) {
                should_banish = true;
            } else {
                for (int i = 0; i < room->cards_in_room(); ++i) {
                    const auto& card = room->look_card(i);
                    if (card.getType() == CardType::Monster) {
                        int val = static_cast<int>(card.get_face()) + 2;
                        if (val >= 8) {
                            should_banish = true;
                            break;
                        }
                    }
                }
            }
            if (should_banish) {
                m_banished_this_room = true;
                return idx;
            }
        }

        // 4. Drink potion if we are damaged and can/should heal
        bool wants_potion = false;
        int max_hp = ctx.get_player()->get_max_health();
        int current_hp = ctx.get_player()->current_life();
        if (current_hp < max_hp) {
            bool is_wizard = (ctx.get_player()->getName() == "Wizard");
            if (is_wizard) {
                wants_potion = (current_hp <= max_hp - 3); // Wizard heals 3+ face value (minimum 2+2=4 HP)
            } else {
                wants_potion = !ctx.is_potion_used() && (current_hp <= max_hp - 4);
            }
        }
        if (wants_potion && (idx = find_action("Drink Potion")) != -1) return idx;

        // 5. Skip Room if valid and we think it's a good idea (e.g. room has monsters but no weapon)
        if ((idx = find_action("Skip Room")) != -1) {
            bool has_monster = false;
            for (int i = 0; i < room->cards_in_room(); ++i) {
                if (room->look_card(i).getType() == CardType::Monster) {
                    has_monster = true;
                    break;
                }
            }
            if (has_monster && !ctx.get_weapon()) {
                return idx;
            }
        }

        // Fallbacks:
        if ((idx = find_action("Skip Room")) != -1) return idx;
        if (!m_banished_this_room && (idx = find_action("Banish Monster")) != -1) {
            m_banished_this_room = true;
            return idx;
        }
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
            int max_hp = ctx.get_player()->get_max_health();
            int current_hp = ctx.get_player()->current_life();
            int deficit = max_hp - current_hp;
            int best_score = -1000;
            
            for (int i = 0; i < room->cards_in_room(); ++i) {
                if (current_action->can_execute(ctx, &room->look_card(i))) {
                    int val = static_cast<int>(room->look_card(i).get_face()) + 2;
                    int score;
                    if (val <= deficit) {
                        score = val; // No wasted healing, larger is better
                    } else {
                        score = deficit - (val - deficit); // Wasted healing penalizes score
                    }
                    if (score > best_score) {
                        best_score = score;
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
            // Attack the HIGHEST value valid monster first to leave room for lower ones later
            best_val = -1;
            for (int i = 0; i < room->cards_in_room(); ++i) {
                if (current_action->can_execute(ctx, &room->look_card(i))) {
                    int val = static_cast<int>(room->look_card(i).get_face()) + 2;
                    if (val > best_val) {
                        best_val = val;
                        best_idx = i + 1;
                    }
                }
            }
        } else if (current_action->name() == "Hand Combat" || current_action->name() == "Attack Monster No Weapon") {
            // Fight the LOWEST value monster to minimize damage
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
        } else if (current_action->name() == "Banish Monster") {
            // Banish the HIGHEST value monster to avoid the most damage
            best_val = -1;
            for (int i = 0; i < room->cards_in_room(); ++i) {
                if (current_action->can_execute(ctx, &room->look_card(i))) {
                    int val = static_cast<int>(room->look_card(i).get_face()) + 2;
                    if (val > best_val) {
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
        m_last_room = nullptr;
        m_banished_this_room = false;
        m_next_card_idx = -1;
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

void run_simulations(PlayerClassType class_type, int iterations, const std::string& label, bool use_lookahead) {
    Stats stats;
    auto ui = std::make_unique<SimulationUI>(use_lookahead);
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
    bool use_lookahead = false;
    if (argc > 1) {
        try {
            iterations = std::stoi(argv[1]);
        } catch (...) {}
    }
    if (argc > 2) {
        std::string algo = argv[2];
        if (algo == "lookahead" || algo == "dfs") {
            use_lookahead = true;
        }
    }

    std::cout << "Running Scoundrel Balance Simulation (" << iterations << " games per class)..." << std::endl;
    std::cout << "Algorithm Mode: " << (use_lookahead ? "Room-Level Lookahead (DFS)" : "Optimized Greedy") << std::endl;
    std::cout << std::string(65, '-') << std::endl;
    std::cout << std::left << std::setw(15) << "Class" 
              << std::right << std::setw(10) << "Games" 
              << std::setw(13) << "Win Rate" 
              << std::setw(12) << "Avg Score" 
              << std::setw(10) << "Max Score" << std::endl;
    std::cout << std::string(65, '-') << std::endl;

    run_simulations(PlayerClassType::PEASANT, iterations, "Peasant", use_lookahead);
    run_simulations(PlayerClassType::WARRIOR, iterations, "Warrior", use_lookahead);
    run_simulations(PlayerClassType::HEALER, iterations, "Healer", use_lookahead);
    run_simulations(PlayerClassType::WIZARD, iterations, "Wizard", use_lookahead);

    std::cout << std::string(65, '-') << std::endl;

    return 0;
}
