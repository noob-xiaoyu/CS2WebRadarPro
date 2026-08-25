#include "pch.hpp"
#include "http.hpp"
#include "core/sdk.hpp"
#include "features/features.hpp"

bool main()
{
	SetConsoleTitleW(L"CS2 Web Radar");

	LOG_INFO("waiting for Counter-Strike 2...");
	while (!m_memory->setup())
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	LOG_INFO("memory initialization completed");
	if (!i::setup())
	{
		LOG_ERROR("interfaces setup failed");
		return {};
	}
	LOG_INFO("interfaces initialization completed");
	if (!schema::setup())
	{
		LOG_ERROR("schema setup failed");
		return {};
	}
	LOG_INFO("schema initialization completed");

	if (!http::init(8080))
	{
		LOG_ERROR("failed to start http server on port 8080 (port in use?)");
		return {};
	}
	LOG_INFO("web radar available at http://127.0.0.1:8080");

	for (;;)
	{
		sdk::update();
		f::run();

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	return true;
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	return main();
}
