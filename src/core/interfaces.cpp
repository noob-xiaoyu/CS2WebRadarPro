#include "pch.hpp"

bool i::setup()
{
	LOG_INFO("interfaces setup begin");
	bool success = true;

	const auto [client_base, client_size] = m_memory->get_module_info(CLIENT_DLL);
	LOG_INFO("client module base=0x%llx size=0x%llx",
		static_cast<unsigned long long>(client_base.value_or(0)),
		static_cast<unsigned long long>(client_size.value_or(0)));
	if (!client_base.has_value() || !client_size.has_value())
		return {};

	const auto schema_pattern = m_memory->find_pattern(SCHEMASYSTEM_DLL, GET_SCHEMA_SYSTEM);
	if (!schema_pattern.has_value())
	{
		LOG_ERROR("failed to find schema system pattern in '%s'", SCHEMASYSTEM_DLL);
		return {};
	}
	m_schema_system = schema_pattern->rip().as<c_schema_system*>();
	success &= (m_schema_system != nullptr);

	const auto global_vars_pattern = m_memory->find_pattern(CLIENT_DLL, GET_GLOBAL_VARS);
	if (!global_vars_pattern.has_value())
	{
		LOG_ERROR("failed to find global vars pattern in '%s'", CLIENT_DLL);
		return {};
	}
	m_global_vars = m_memory->read_t<c_global_vars*>(global_vars_pattern->rip().as<c_global_vars*>());
	success &= (m_global_vars != nullptr);

	const auto entity_list_pattern = m_memory->find_pattern(CLIENT_DLL, GET_ENTITY_LIST);
	if (!entity_list_pattern.has_value())
	{
		LOG_ERROR("failed to find entity list pattern in '%s'", CLIENT_DLL);
		return {};
	}
	m_game_entity_system = m_memory->read_t<c_game_entity_system*>(entity_list_pattern->rip().as<c_game_entity_system*>());
	success &= (m_game_entity_system != nullptr);

	return success;
}
