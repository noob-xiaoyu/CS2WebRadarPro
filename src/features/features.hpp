#pragma once

#include <windows.h>
#include "utils/json.hpp"

namespace f::players
{
	bool get_data(int32_t idx, c_cs_player_controller* player, c_cs_player_pawn* player_pawn);
	void get_weapons(c_cs_player_pawn* player_pawn);
	void get_active_weapon(c_cs_player_pawn* player_pawn);
}

namespace f::bomb
{
	void get_carried_bomb(c_base_entity* bomb);
	void get_planted_bomb(c_planted_c4* planted_c4);
}

namespace f
{
	void run();
	void get_map();
	void get_player_info();

	// 主循环线程写、HTTP 线程读，用读写锁保护共享数据
	inline SRWLOCK m_lock = SRWLOCK_INIT;
	inline json_t m_data = json_t::object();
	inline json_t m_player_data = json_t::object();
	inline uint32_t m_bomb_idx = 0;
}
