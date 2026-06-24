#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <random>
#include <iomanip>
#include <algorithm>
#include <map>
#include <sstream>

#include "GameLogic.h"
#include "DefaultGameContextFactory.h"
#include "PlayerClassFactory.h"
#include "ui/UserInterface.h"
#include "gamecontext.h"
#include "actions/action.h"
#include "assets/room.h"
#include "assets/cards.h"

struct SimCard
{
    int original_index;
    CardType type;
    int value;                     // raw monster damage / potion heal / weapon damage
    bool is_warlord = false;       // Joker Spades monster (Extended only)
    bool is_plague_doctor = false; // Joker Clubs  monster (Extended only)
};

struct SimState
{
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
    bool warlord_active = false;       // Extended: all non-Warlord monsters deal +2 damage
    bool plague_doctor_active = false; // Extended: all potion healing is halved
    SimCard room_cards[4];
    int num_cards;
};

struct SimMove
{
    std::string action_name;
    int card_idx; // 1-based index (or -1)
};

// Function to serialize SimState to a string for memoization key
std::string serialize_state(const SimState &state)
{
    std::ostringstream oss;
    oss << state.hp << "," << state.max_hp << "," << state.has_weapon << ","
        << state.weapon_damage << "," << state.weapon_last_monster_damage << ","
        << state.weapon_monsters_killed << "," << state.potion_used << ","
        << state.banished_this_room << "," << state.actions_taken << ","
        << state.room_skipped << "," << state.player_class_name << ","
        << state.warlord_active << "," << state.plague_doctor_active << ";";

    // Canonical representation of cards (sorted by original_index)
    std::vector<SimCard> cards;
    for (int i = 0; i < state.num_cards; ++i)
    {
        cards.push_back(state.room_cards[i]);
    }
    std::sort(cards.begin(), cards.end(), [](const SimCard &a, const SimCard &b)
              { return a.original_index < b.original_index; });

    for (const auto &card : cards)
    {
        oss << "c(" << card.original_index << "," << (int)card.type << "," << card.value << "," << card.is_warlord << "," << card.is_plague_doctor << ")";
    }
    return oss.str();
}

// Returns the score of a state, not considering future moves.
int evaluate_state(const SimState &state)
{
    int score = state.hp * 100;
    if (state.has_weapon)
    {
        score += state.weapon_damage * 15;
    }
    // Add penalties for remaining monsters if this is a final action
    if (state.actions_taken == 3)
    {
        for (int i = 0; i < state.num_cards; ++i)
        {
            const auto &card = state.room_cards[i];
            if (card.type == CardType::Monster)
            {
                int effective_val = card.value;
                if (state.warlord_active && !card.is_warlord)
                {
                    effective_val += 2;
                }
                score -= effective_val * 12;
            }
        }
    }
    return score;
}

// The result of a search from a given state.
struct SearchResult
{
    int score;
    std::vector<SimMove> path;
};

// Memoization cache
std::map<std::string, SearchResult> memo;

SearchResult solve_dfs_iterative(const SimState &initial_state)
{
    SearchResult best_result = {-1000000, {}};

    for (int depth_limit = 1; depth_limit <= 5; ++depth_limit)
    {
        memo.clear();

        std::function<SearchResult(const SimState &, int)> solve =
            [&](const SimState &state, int depth) -> SearchResult
        {
            if (state.hp <= 0)
            {
                return {-100000 + state.actions_taken, {}};
            }

            std::string state_key = serialize_state(state);
            if (memo.count(state_key))
            {
                return memo[state_key];
            }

            if (depth == 0)
            {
                return {evaluate_state(state), {}};
            }

            SearchResult best_res = {evaluate_state(state), {}};

            // Option 1: New Room
            if (state.actions_taken == 3 || state.num_cards == 0)
            {
                int score = evaluate_state(state);
                if (score > best_res.score)
                {
                    best_res = {score, {{"New Room", -1}}};
                }
            }

            // Option 2: Skip Room
            if (state.actions_taken == 0 && !state.room_skipped)
            {
                int score = evaluate_state(state);
                if (score > best_res.score)
                {
                    best_res = {score, {{"Skip Room", -1}}};
                }
            }

            if (state.num_cards > 0 && state.actions_taken < 3)
            {
                for (int i = 0; i < state.num_cards; ++i)
                {
                    const auto &card = state.room_cards[i];

                    auto apply_next_cards = [&](SimState &next_state, int remove_idx)
                    {
                        next_state.num_cards = 0;
                        for (int j = 0; j < state.num_cards; ++j)
                        {
                            if (j != remove_idx)
                            {
                                next_state.room_cards[next_state.num_cards++] = state.room_cards[j];
                            }
                        }
                    };

                    // Explore moves for the current card
                    if (card.type == CardType::Potion)
                    {
                        bool can_heal = !state.potion_used || (state.player_class_name == "Wizard");
                        if (can_heal)
                        {
                            SimState next_state = state;
                            apply_next_cards(next_state, i);
                            next_state.actions_taken++;
                            int heal_value = state.plague_doctor_active ? card.value / 2 : card.value;
                            next_state.hp = std::min(state.max_hp, state.hp + heal_value);
                            next_state.potion_used = true;

                            SearchResult res = solve(next_state, depth - 1);
                            if (res.score > best_res.score)
                            {
                                best_res = res;
                                best_res.path.insert(best_res.path.begin(), {"Drink Potion", card.original_index + 1});
                            }
                        }
                    }
                    else if (card.type == CardType::Weapon)
                    {
                        SimState next_state = state;
                        apply_next_cards(next_state, i);
                        next_state.actions_taken++;
                        next_state.has_weapon = true;
                        next_state.weapon_damage = card.value;
                        next_state.weapon_last_monster_damage = 0;
                        next_state.weapon_monsters_killed = 0;

                        SearchResult res = solve(next_state, depth - 1);
                        if (res.score > best_res.score)
                        {
                            best_res = res;
                            best_res.path.insert(best_res.path.begin(), {"Attach Weapon", card.original_index + 1});
                        }
                    }
                    else if (card.type == CardType::Monster)
                    {
                        int effective_damage = card.value;
                        if (state.warlord_active && !card.is_warlord)
                            effective_damage += 2;

                        // Banish
                        if (state.player_class_name == "Wizard" && !state.banished_this_room)
                        {
                            SimState next_state = state;
                            apply_next_cards(next_state, i);
                            next_state.actions_taken++;
                            next_state.banished_this_room = true;

                            SearchResult res = solve(next_state, depth - 1);
                            if (res.score > best_res.score)
                            {
                                best_res = res;
                                best_res.path.insert(best_res.path.begin(), {"Banish Monster", card.original_index + 1});
                            }
                        }

                        // Weapon Attack
                        if (state.has_weapon)
                        {
                            bool can_attack = (state.weapon_monsters_killed == 0) || (card.value < state.weapon_last_monster_damage);
                            if (can_attack)
                            {
                                SimState next_state = state;
                                apply_next_cards(next_state, i);
                                next_state.actions_taken++;
                                int damage_taken = std::max(0, effective_damage - state.weapon_damage);
                                next_state.hp = state.hp - damage_taken;
                                next_state.weapon_last_monster_damage = card.value;
                                next_state.weapon_monsters_killed++;
                                if (card.is_warlord)
                                    next_state.warlord_active = false;
                                if (card.is_plague_doctor)
                                    next_state.plague_doctor_active = false;

                                SearchResult res = solve(next_state, depth - 1);
                                if (res.score > best_res.score)
                                {
                                    best_res = res;
                                    best_res.path.insert(best_res.path.begin(), {"Attack Monster", card.original_index + 1});
                                }
                            }
                        }

                        // Hand Combat
                        {
                            SimState next_state = state;
                            apply_next_cards(next_state, i);
                            next_state.actions_taken++;
                            next_state.hp = state.hp - effective_damage;
                            if (card.is_warlord)
                                next_state.warlord_active = false;
                            if (card.is_plague_doctor)
                                next_state.plague_doctor_active = false;

                            SearchResult res = solve(next_state, depth - 1);
                            if (res.score > best_res.score)
                            {
                                best_res = res;
                                best_res.path.insert(best_res.path.begin(), {"Hand Combat", card.original_index + 1});
                            }
                        }
                    }
                }
            }

            memo[state_key] = best_res;
            return best_res;
        };

        SearchResult current_run_result = solve(initial_state, depth_limit);
        if (current_run_result.score > best_result.score)
        {
            best_result = current_run_result;
        }
    }
    return best_result;
}

class SimulationUI : public UserInterface
{
public:
    bool victory = false;
    int score = 0;
    bool game_ended = false;
    std::string defeat_reason;
    GameContext *m_ctx = nullptr;
    int m_last_action_idx = -1;
    const Room *m_last_room = nullptr;
    bool m_banished_this_room = false;
    bool m_use_lookahead = false;
    int m_next_card_idx = -1;

    SimulationUI(bool use_lookahead = false) : m_use_lookahead(use_lookahead) {}

    std::optional<int> select_game_mode() override { return 1; } // Standard
    std::optional<int> select_player_class() override { return 0; }

    void display_game_state(const GameContext &ctx) override {}
    void display_valid_actions(std::span<Action *const> actions) override {}

    std::optional<int> request_action_index(int max_index) override
    {
        if (!m_ctx)
            return 0;

        auto *room = m_ctx->get_room();
        if (room && room != m_last_room)
        {
            m_last_room = room;
            m_banished_this_room = false;
        }

        auto actions = m_ctx->get_turn_valid_actions();

        if (m_use_lookahead)
        {
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
            state.warlord_active = m_ctx->is_warlord_active();
            state.plague_doctor_active = m_ctx->is_plague_doctor_active();

            auto *room = m_ctx->get_room();
            if (room)
            {
                state.num_cards = 0;
                for (int i = 0; i < room->cards_in_room(); ++i)
                {
                    const auto &card = room->look_card(i);
                    SimCard sc;
                    sc.original_index = i;
                    sc.type = card.getType();
                    // Joker monsters have damage 12, NOT static_cast<int>(face::_JK)+2 = 15
                    sc.value = (card.get_face() == face::_JK)
                                   ? 12
                                   : static_cast<int>(card.get_face()) + 2;
                    sc.is_warlord = (card.get_face() == face::_JK && card.get_suit() == suit::Spades);
                    sc.is_plague_doctor = (card.get_face() == face::_JK && card.get_suit() == suit::Clubs);
                    state.room_cards[state.num_cards++] = sc;
                }
            }

            SearchResult result = solve_dfs_iterative(state);

            if (!result.path.empty())
            {
                std::string chosen_action_name = result.path[0].action_name;
                m_next_card_idx = result.path[0].card_idx;

                if (chosen_action_name == "Banish Monster")
                {
                    m_banished_this_room = true;
                }

                int idx = -1;
                for (size_t i = 0; i < actions.size(); ++i)
                {
                    if (actions[i]->name() == chosen_action_name)
                    {
                        idx = static_cast<int>(i);
                        break;
                    }
                }
                if (idx == -1)
                {
                    if (chosen_action_name == "Hand Combat")
                    {
                        for (size_t i = 0; i < actions.size(); ++i)
                        {
                            if (actions[i]->name() == "Attack Monster No Weapon")
                            {
                                idx = static_cast<int>(i);
                                break;
                            }
                        }
                    }
                    else if (chosen_action_name == "Attack Monster No Weapon")
                    {
                        for (size_t i = 0; i < actions.size(); ++i)
                        {
                            if (actions[i]->name() == "Hand Combat")
                            {
                                idx = static_cast<int>(i);
                                break;
                            }
                        }
                    }
                }

                if (idx != -1)
                {
                    m_last_action_idx = idx;
                    return m_last_action_idx;
                }
            }
        }

        m_last_action_idx = pick_action(*m_ctx, actions);
        return m_last_action_idx;
    }

    std::optional<int> request_card_index(int max_cards) override
    {
        if (!m_ctx)
            return 1;
        if (m_use_lookahead && m_next_card_idx != -1)
        {
            int card_idx = m_next_card_idx;
            m_next_card_idx = -1;
            return card_idx;
        }
        return pick_card(*m_ctx, max_cards);
    }

    int pick_action(const GameContext &ctx, const std::vector<Action *> &actions)
    {
        auto *room = ctx.get_room();
        if (!room)
            return 0;

        if (room != m_last_room)
        {
            m_last_room = room;
            m_banished_this_room = false;
        }

        auto can_actually_execute = [&](Action *action) -> bool
        {
            if (action->name() == "Skip Room" || action->name() == "New Room")
            {
                return action->can_execute(ctx, nullptr);
            }
            for (int i = 0; i < room->cards_in_room(); ++i)
            {
                if (action->can_execute(ctx, &room->look_card(i)))
                    return true;
            }
            return false;
        };

        auto find_action = [&](const std::string &name) -> int
        {
            for (size_t i = 0; i < actions.size(); ++i)
            {
                if (actions[i]->name() == name && can_actually_execute(actions[i]))
                    return static_cast<int>(i);
            }
            return -1;
        };

        int idx;
        if ((idx = find_action("New Room")) != -1)
            return idx;

        // 1. Prioritize attaching a weapon if we don't have one
        if (!ctx.get_weapon() && (idx = find_action("Attach Weapon")) != -1)
            return idx;

        // 2. Prioritize attacking a monster if we have a weapon
        if (ctx.get_weapon() && (idx = find_action("Attack Monster")) != -1)
            return idx;

        // 3. Wizard Banish Monster logic:
        if (!m_banished_this_room && (idx = find_action("Banish Monster")) != -1)
        {
            bool should_banish = false;
            // Banish if we don't have a weapon, or if there's a strong monster in the room
            if (!ctx.get_weapon())
            {
                should_banish = true;
            }
            else
            {
                for (int i = 0; i < room->cards_in_room(); ++i)
                {
                    const auto &card = room->look_card(i);
                    if (card.getType() == CardType::Monster)
                    {
                        int val = static_cast<int>(card.get_face()) + 2;
                        if (val >= 8)
                        {
                            should_banish = true;
                            break;
                        }
                    }
                }
            }
            if (should_banish)
            {
                m_banished_this_room = true;
                return idx;
            }
        }

        // 4. Drink potion if we are damaged and can/should heal
        bool wants_potion = false;
        int max_hp = ctx.get_player()->get_max_health();
        int current_hp = ctx.get_player()->current_life();
        if (current_hp < max_hp)
        {
            bool is_wizard = (ctx.get_player()->getName() == "Wizard");
            // When Plague Doctor is active, healing is halved — lower the threshold so
            // we still use potions but only when the gain justifies it.
            int threshold = ctx.is_plague_doctor_active() ? 6 : 4;
            if (is_wizard)
            {
                wants_potion = (current_hp <= max_hp - 3);
            }
            else
            {
                wants_potion = !ctx.is_potion_used() && (current_hp <= max_hp - threshold);
            }
        }
        if (wants_potion && (idx = find_action("Drink Potion")) != -1)
            return idx;

        // 5. Skip Room if valid and we think it's a good idea (e.g. room has monsters but no weapon)
        if ((idx = find_action("Skip Room")) != -1)
        {
            bool has_monster = false;
            for (int i = 0; i < room->cards_in_room(); ++i)
            {
                if (room->look_card(i).getType() == CardType::Monster)
                {
                    has_monster = true;
                    break;
                }
            }
            if (has_monster && !ctx.get_weapon())
            {
                return idx;
            }
        }

        // Fallbacks:
        if ((idx = find_action("Skip Room")) != -1)
            return idx;
        if (!m_banished_this_room && (idx = find_action("Banish Monster")) != -1)
        {
            m_banished_this_room = true;
            return idx;
        }
        if ((idx = find_action("Attack Monster No Weapon")) != -1)
            return idx;
        if ((idx = find_action("Hand Combat")) != -1)
            return idx;
        if ((idx = find_action("Attack Monster")) != -1)
            return idx;
        if ((idx = find_action("Attach Weapon")) != -1)
            return idx;
        if ((idx = find_action("Drink Potion")) != -1)
            return idx;

        return 0;
    }

    int pick_card(const GameContext &ctx, int count)
    {
        auto *room = ctx.get_room();
        if (!room)
            return 1;

        int best_idx = -1;
        int best_val = -1;

        Action *current_action = nullptr;
        auto valid = ctx.get_turn_valid_actions();
        if (m_last_action_idx >= 0 && m_last_action_idx < static_cast<int>(valid.size()))
        {
            current_action = valid[m_last_action_idx];
        }

        if (!current_action)
            return 1;

        if (current_action->name() == "Drink Potion")
        {
            int max_hp = ctx.get_player()->get_max_health();
            int current_hp = ctx.get_player()->current_life();
            int deficit = max_hp - current_hp;
            int best_score = -1000;

            for (int i = 0; i < room->cards_in_room(); ++i)
            {
                if (current_action->can_execute(ctx, &room->look_card(i)))
                {
                    int val = static_cast<int>(room->look_card(i).get_face()) + 2;
                    int score;
                    if (val <= deficit)
                    {
                        score = val; // No wasted healing, larger is better
                    }
                    else
                    {
                        score = deficit - (val - deficit); // Wasted healing penalizes score
                    }
                    if (score > best_score)
                    {
                        best_score = score;
                        best_idx = i + 1;
                    }
                }
            }
        }
        else if (current_action->name() == "Attach Weapon")
        {
            for (int i = 0; i < room->cards_in_room(); ++i)
            {
                if (current_action->can_execute(ctx, &room->look_card(i)))
                {
                    int val = static_cast<int>(room->look_card(i).get_face()) + 2;
                    if (val > best_val)
                    {
                        best_val = val;
                        best_idx = i + 1;
                    }
                }
            }
        }
        else if (current_action->name() == "Attack Monster")
        {
            // Attack the HIGHEST raw-value valid monster first to preserve weapon chain.
            // With an active Warlord the effective damage of others is +2, but weapon
            // eligibility is still based on raw values, so raw-value ordering is correct.
            best_val = -1;
            for (int i = 0; i < room->cards_in_room(); ++i)
            {
                if (current_action->can_execute(ctx, &room->look_card(i)))
                {
                    const auto &c = room->look_card(i);
                    int val = (c.get_face() == face::_JK) ? 12 : static_cast<int>(c.get_face()) + 2;
                    if (val > best_val)
                    {
                        best_val = val;
                        best_idx = i + 1;
                    }
                }
            }
        }
        else if (current_action->name() == "Hand Combat" || current_action->name() == "Attack Monster No Weapon")
        {
            // When the Warlord is alive, killing it removes the +2 buff from all others,
            // so prefer it even though it's the highest-damage enemy.
            // Otherwise fight the lowest-effective-damage monster to minimise HP loss.
            bool warlord_alive = ctx.is_warlord_active();
            best_val = warlord_alive ? -1 : 100;
            for (int i = 0; i < room->cards_in_room(); ++i)
            {
                if (current_action->can_execute(ctx, &room->look_card(i)))
                {
                    const auto &c = room->look_card(i);
                    bool is_warlord = (c.get_face() == face::_JK && c.get_suit() == suit::Spades);
                    int raw_val = (c.get_face() == face::_JK) ? 12 : static_cast<int>(c.get_face()) + 2;
                    int eff_val = (warlord_alive && !is_warlord) ? raw_val + 2 : raw_val;
                    if (warlord_alive)
                    {
                        // Prefer the Warlord first; among others pick highest effective damage
                        if (is_warlord || eff_val > best_val)
                        {
                            best_val = is_warlord ? 999 : eff_val;
                            best_idx = i + 1;
                        }
                    }
                    else
                    {
                        // No Warlord: fight lowest effective damage first
                        if (eff_val < best_val)
                        {
                            best_val = eff_val;
                            best_idx = i + 1;
                        }
                    }
                }
            }
        }
        else if (current_action->name() == "Banish Monster")
        {
            // Banish the highest-effective-damage monster (Warlord first if active)
            best_val = -1;
            bool warlord_alive = ctx.is_warlord_active();
            for (int i = 0; i < room->cards_in_room(); ++i)
            {
                if (current_action->can_execute(ctx, &room->look_card(i)))
                {
                    const auto &c = room->look_card(i);
                    bool is_warlord = (c.get_face() == face::_JK && c.get_suit() == suit::Spades);
                    int raw_val = (c.get_face() == face::_JK) ? 12 : static_cast<int>(c.get_face()) + 2;
                    int eff_val = (warlord_alive && !is_warlord) ? raw_val + 2 : raw_val;
                    if (eff_val > best_val)
                    {
                        best_val = eff_val;
                        best_idx = i + 1;
                    }
                }
            }
        }

        return (best_idx != -1) ? best_idx : 1;
    }

    void show_message(const std::string &message) override {}
    void show_error(const std::string &error) override {}

    void show_victory(int s) override
    {
        victory = true;
        score = s;
        game_ended = true;
    }
    void show_defeat(int s, const std::string &reason) override
    {
        victory = false;
        score = s;
        game_ended = true;
        defeat_reason = reason;
    }
    bool ask_restart() override { return false; }
    void reset() override
    {
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

struct Stats
{
    int games = 0;
    int wins = 0;
    int total_score = 0;
    int max_score = 0;
    std::map<std::string, int> death_reasons;

    void add_game(bool victory, int score, const std::string &reason)
    {
        games++;
        if (victory)
            wins++;
        total_score += score;
        if (score > max_score)
            max_score = score;
        if (!victory)
            death_reasons[reason]++;
    }

    double win_rate() const { return games == 0 ? 0 : (double)wins / games * 100.0; }
    double avg_score() const { return games == 0 ? 0 : (double)total_score / games; }
};

void run_simulations(PlayerClassType class_type, int iterations, const std::string &label,
                     bool use_lookahead, GameType game_type = GameType::DEFAULT)
{
    Stats stats;
    auto ui = std::make_unique<SimulationUI>(use_lookahead);
    auto factory = std::make_unique<DefaultGameContextFactory>();
    std::random_device rd;

    for (int i = 0; i < iterations; ++i)
    {
        ui->reset();
        auto game = GameLogic::create(game_type);
        factory->set_player(create_player(class_type));
        auto ctx = factory->create_game_context(rd(), game_type);
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
    std::sort(reasons.begin(), reasons.end(), [](auto &a, auto &b)
              { return a.second > b.second; });

    int count = 0;
    for (auto &r : reasons)
    {
        if (count++ >= 3)
            break;
        std::string display_reason = r.first.empty() ? "Death by Damage" : r.first;
        std::cout << "  - " << display_reason << ": " << r.second << std::endl;
    }
}

int main(int argc, char **argv)
{
    int iterations = 1000;
    bool use_lookahead = false;
    // mode: "default" | "extended" | "both"  (default: "default")
    std::string mode = "default";

    if (argc > 1)
    {
        try
        {
            iterations = std::stoi(argv[1]);
        }
        catch (...)
        {
        }
    }
    if (argc > 2)
    {
        std::string algo = argv[2];
        if (algo == "lookahead" || algo == "dfs")
        {
            use_lookahead = true;
        }
    }
    if (argc > 3)
    {
        mode = argv[3];
    }

    bool run_default = (mode == "default" || mode == "both");
    bool run_extended = (mode == "extended" || mode == "both");

    std::string algo_label = use_lookahead ? "Room-Level Lookahead (DFS)" : "Optimized Greedy";

    auto print_header = [&](const std::string &section_label)
    {
        std::cout << "\n"
                  << section_label << std::endl;
        std::cout << std::string(65, '-') << std::endl;
        std::cout << std::left << std::setw(15) << "Class"
                  << std::right << std::setw(10) << "Games"
                  << std::setw(13) << "Win Rate"
                  << std::setw(12) << "Avg Score"
                  << std::setw(10) << "Max Score" << std::endl;
        std::cout << std::string(65, '-') << std::endl;
    };

    std::cout << "Running Scoundrel Balance Simulation (" << iterations << " games per class)" << std::endl;
    std::cout << "Algorithm : " << algo_label << std::endl;
    std::cout << "Mode      : " << mode << std::endl;

    if (run_default)
    {
        print_header("[ Default Mode ]");
        run_simulations(PlayerClassType::PEASANT, iterations, "Peasant", use_lookahead, GameType::DEFAULT);
        run_simulations(PlayerClassType::WARRIOR, iterations, "Warrior", use_lookahead, GameType::DEFAULT);
        run_simulations(PlayerClassType::HEALER, iterations, "Healer", use_lookahead, GameType::DEFAULT);
        run_simulations(PlayerClassType::WIZARD, iterations, "Wizard", use_lookahead, GameType::DEFAULT);
        std::cout << std::string(65, '-') << std::endl;
    }

    if (run_extended)
    {
        print_header("[ Extended Mode ] (Warlord + Plague Doctor active)");
        run_simulations(PlayerClassType::PEASANT, iterations, "Peasant", use_lookahead, GameType::EXTENDED);
        run_simulations(PlayerClassType::WARRIOR, iterations, "Warrior", use_lookahead, GameType::EXTENDED);
        run_simulations(PlayerClassType::HEALER, iterations, "Healer", use_lookahead, GameType::EXTENDED);
        run_simulations(PlayerClassType::WIZARD, iterations, "Wizard", use_lookahead, GameType::EXTENDED);
        std::cout << std::string(65, '-') << std::endl;
    }

    return 0;
}
