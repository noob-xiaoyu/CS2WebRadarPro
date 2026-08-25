#ifndef PCH_HPP
#define PCH_HPP

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <cstdint>
#include <cassert>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <thread>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <optional>
#include <memory>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <format>

#include "common.hpp"

/* utils */
#include "utils/address.hpp"
#include "utils/memory.hpp"
#include "utils/fnv1a.hpp"

/* core */
#include "core/interfaces.hpp"
#include "core/schema.hpp"

/* datatypes */
#include "sdk/datatypes/utl_ts_hash.hpp"
#include "sdk/datatypes/utl_vector.hpp"
#include "sdk/datatypes/vector.hpp"

/* sdk */
#include "sdk/entity_handle.hpp"
#include "sdk/entity.hpp"

/* sdk/interfaces */
#include "sdk/interfaces/game_entity_system.hpp"
#include "sdk/interfaces/schema_system.hpp"
#include "sdk/interfaces/global_vars.hpp"

#include "core/sdk.hpp"

/* features */
#include "features/features.hpp"

#endif //PCH_HPP
