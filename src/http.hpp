#pragma once

#include <cstdint>

// 内嵌 HTTP 服务器：提供 /（网页）、/api/radar（JSON）、/data/* 与 /assets/*（静态资源）
namespace http
{
	bool init(int port);
	void shutdown();
}
