// 注意：winsock2.h 必须在 windows.h 之前包含，否则与 winsock.h 冲突
#include <winsock2.h>
#include <ws2tcpip.h>
#include "http.hpp"
#include "pch.hpp"
#include "features/features.hpp"

#pragma comment(lib, "ws2_32.lib")

// ── 服务器状态 ──────────────────────────────────────────────────────────────
static SOCKET g_listen = INVALID_SOCKET;
static HANDLE g_thread = nullptr;
static volatile bool g_running = false;
static std::wstring g_exe_dir;   // exe 所在目录（用于定位 data/ 与 assets/）

// ── 内嵌雷达网页（原生 JS，参考原 React 版功能）─────────────────────────────
static const char* g_html = R"(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<title>CS2 雷达</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:TASAOrbiter,Segoe UI,Arial,sans-serif;color:#b1d0e7;height:100vh;overflow:hidden;background-size:cover;background-position:center;background-repeat:no-repeat}
#wrap{display:flex;align-items:center;justify-content:space-evenly;height:100%;width:100%;position:relative;background:radial-gradient(50% 50% at 50% 50%,rgba(20,40,55,.95) 0%,rgba(7,20,30,.95) 100%)}
#hud{position:fixed;right:12px;top:12px;z-index:99;display:flex;flex-direction:column;align-items:flex-end;gap:6px}
#hud .btn{background:rgba(30,58,84,.8);border:1px solid rgba(100,146,180,.3);color:#b1d0e7;padding:4px 10px;border-radius:8px;cursor:pointer;font-size:13px}
#hud .panel{display:none;background:rgba(16,26,38,.92);border:1px solid rgba(100,146,180,.25);border-radius:12px;padding:12px 16px;width:220px}
#hud .panel.open{display:block}
#hud .panel .row{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;font-size:13px}
#hud .panel input[type=range]{width:110px}
#bombTimer{position:fixed;left:50%;top:10px;transform:translateX(-50%);z-index:99;display:none;align-items:center;gap:8px;background:rgba(16,26,38,.85);padding:6px 14px;border-radius:10px;font-size:16px;font-weight:600}
#bombTimer .c4{width:26px;height:26px;background:#c90b0b;-webkit-mask:url(./assets/icons/c4_sml.png) no-repeat center/contain;mask:url(./assets/icons/c4_sml.png) no-repeat center/contain}
#radarWrap{position:relative;display:inline-block;z-index:1}
#radarWrap img{display:block;max-width:78vh;max-height:78vh;width:auto;height:auto}
.pcard{position:absolute;z-index:2;list-style:none;display:flex;flex-direction:column;gap:14px;top:50%;transform:translateY(-50%)}
.pcard.left{left:16px}
.pcard.right{right:16px}
.pcard li{display:flex;align-items:center;gap:10px;opacity:1;transition:opacity .2s}
.pcard li.dead{opacity:.5}
.pcard.right li{flex-direction:row-reverse}
.pcard .char{height:110px}
.pcard.right .char{transform:scaleX(-1)}
.pcard .info{display:flex;flex-direction:column;gap:4px;min-width:130px}
.pcard.right .info{align-items:flex-end;text-align:right}
.pcard .nm{font-size:13px;cursor:pointer}
.pcard .nm:hover{text-decoration:underline}
.pcard .tri{width:0;height:0;border-left:7px solid transparent;border-right:7px solid transparent;border-top:11px solid #fff}
.pcard .row1{display:flex;align-items:center;gap:8px;font-size:13px}
.pcard.right .row1{flex-direction:row-reverse}
.pcard .row1 .money{color:#7ee787;font-weight:600}
.pcard .ic{display:inline-flex;align-items:center;gap:3px;font-size:13px}
.pcard .ic .ico{width:16px;height:16px;background:#6492b4;-webkit-mask:no-repeat center/contain;mask:no-repeat center/contain}
.pcard .ic .ico.health{-webkit-mask-image:url(./assets/icons/health.svg);mask-image:url(./assets/icons/health.svg)}
.pcard .ic .ico.armor{-webkit-mask-image:url(./assets/icons/kevlar.svg);mask-image:url(./assets/icons/kevlar.svg)}
.pcard .ic .ico.armor.helmet{-webkit-mask-image:url(./assets/icons/kevlar_helmet.svg);mask-image:url(./assets/icons/kevlar_helmet.svg)}
.pcard .weps{display:flex;gap:6px;flex-wrap:wrap}
.pcard.right .weps{justify-content:flex-end}
.pcard .wep{width:26px;height:26px;background:#6492b4;-webkit-mask:no-repeat center/contain;mask:no-repeat center/contain}
.pcard .wep.active{background:#b1d0e7}
.pcard .util{width:24px;height:24px;background:#6492b4;-webkit-mask:no-repeat center/contain;mask:no-repeat center/contain}
#msg{position:fixed;left:50%;top:50%;transform:translate(-50%,-50%);font-size:18px;color:#b1d0e7;z-index:5;text-align:center}
#msg.error{color:#ff6b6b}
</style>
</head>
<body>
<div id="msg">等待数据...</div>
<div id="wrap">
  <ul id="leftCol" class="pcard left"></ul>
  <div id="radarWrap"></div>
  <ul id="rightCol" class="pcard right"></ul>
</div>
<div id="hud">
  <button class="btn" id="settingsBtn">⚙ 设置</button>
  <div class="panel" id="settingsPanel">
    <div class="row"><span>玩家大小</span><span id="dotVal">1x</span></div>
    <div class="row"><input type="range" id="dotSize" min="1" max="2" step="0.1" value="1"></div>
    <div class="row"><span>炸弹大小</span><span id="bombVal">0.5x</span></div>
    <div class="row"><input type="range" id="bombSize" min="0.5" max="1.5" step="0.1" value="0.5"></div>
  </div>
</div>
<div id="bombTimer"><span class="c4"></span><span id="bombTime"></span></div>
<script>
var settings={dotSize:1,bombSize:0.5};
try{var s=localStorage.getItem('radarSettings');if(s)settings=Object.assign(settings,JSON.parse(s));}catch(e){}
var mapData=null,mapName=null,players=[],localTeam=2,bombData=null;
var playerRotations={};
var modelCache={};
var el=function(t,c){var e=document.createElement(t);if(c)e.className=c;return e};
function saveSettings(){try{localStorage.setItem('radarSettings',JSON.stringify(settings));}catch(e){}}
document.getElementById('dotSize').oninput=function(){settings.dotSize=parseFloat(this.value);document.getElementById('dotVal').textContent=settings.dotSize+'x';saveSettings()};
document.getElementById('bombSize').oninput=function(){settings.bombSize=parseFloat(this.value);document.getElementById('bombVal').textContent=settings.bombSize+'x';saveSettings()};
document.getElementById('settingsBtn').onclick=function(){document.getElementById('settingsPanel').classList.toggle('open')};
document.getElementById('dotVal').textContent=settings.dotSize+'x';
document.getElementById('bombVal').textContent=settings.bombSize+'x';

function getRadarPosition(c){
  if(!c.x||!c.y||!mapData.x||!mapData.y)return{x:0,y:0};
  return{x:(c.x-mapData.x)/mapData.scale/1024,y:(((c.y-mapData.y)/mapData.scale)*-1)/1024};
}
var colors=['#84c8ed','#009a7d','#eadd40','#df7d29','#b72b92','#ffffff'];
function calcRotation(p){
  var target=270-p.m_eye_angle;
  var cur=playerRotations[p.m_idx]||0;
  playerRotations[p.m_idx]=cur+((((target-cur)+540)%360)-180);
  return playerRotations[p.m_idx];
}
function maskIcon(parent,path,size,active){
  var d=el('div',active?'wep active':'wep');
  d.style.WebkitMask='url('+path+') no-repeat center/contain';
  d.style.mask='url('+path+') no-repeat center/contain';
  d.style.width=size+'px';d.style.height=size+'px';
  parent.appendChild(d);
}
function renderPlayers(){
  var rw=document.getElementById('radarWrap');
  rw.innerHTML='';
  if(!mapData||!mapName)return;
  var img=el('img');img.src='./data/'+mapName+'/radar.png';
  rw.appendChild(img);
  var rect=function(){return rw.getBoundingClientRect()};
  players.forEach(function(p){
    var pos=getRadarPosition(p.m_position);
    var size=0.7*settings.dotSize;
    var w=size/100*innerWidth,h=size/100*innerWidth;
    var d=el('div');
    d.style.cssText='position:absolute;left:0;top:0;width:'+w+'px;height:'+h+'px;transform:translate('+(rect().width*pos.x-w/2)+'px,'+(rect().height*pos.y-h/2)+'px);transition:transform .1s linear;z-index:'+(p.m_is_dead?0:1);
    var rot=el('div');
    rot.style.cssText='width:100%;height:100%;transform:rotate('+(p.m_is_dead?0:calcRotation(p))+'deg);transition:transform .1s linear;opacity:'+(p.m_is_dead?0.8:1);
    var dot=el('div');
    if(p.m_is_dead){
      // 死亡：显示死亡图标（保留最后位置），不再显示三角朝向点
      dot.style.cssText='width:100%;height:100%;background:#fff;-webkit-mask:url(./assets/icons/icon-enemy-death_png.png) no-repeat center/contain;mask:url(./assets/icons/icon-enemy-death_png.png) no-repeat center/contain';
    }else{
      dot.style.cssText='width:100%;height:100%;border-radius:50% 50% 50% 0;transform:rotate(315deg);background:'+((p.m_team==localTeam&&colors[p.m_color])||'red');
    }
    rot.appendChild(dot);d.appendChild(rot);rw.appendChild(d);
  });
  if(bombData){
    var pos=getRadarPosition(bombData);
    var size=1.5*settings.bombSize;
    var w=size/100*innerWidth,h=size/100*innerWidth;
    var d=el('div');
    var col=(bombData.m_is_defused&&'#50904c')||(localTeam==3&&'#6492b4')||'#c90b0b';
    d.style.cssText='position:absolute;left:0;top:0;width:'+w+'px;height:'+h+'px;transform:translate('+(rect().width*pos.x-w/2)+'px,'+(rect().height*pos.y-h/2)+'px);background:'+col+';-webkit-mask:url(./assets/icons/c4_sml.png) no-repeat center/contain;mask:url(./assets/icons/c4_sml.png) no-repeat center/contain;z-index:1';
    rw.appendChild(d);
  }
}
function weaponPath(w){
  return './assets/icons/'+w+'.svg';
}
function renderCards(){
  var left=document.getElementById('leftCol'),right=document.getElementById('rightCol');
  left.innerHTML='';right.innerHTML='';
  players.forEach(function(p){
    var li=el('li');if(p.m_is_dead)li.className='dead';
    var col=document.getElementById(p.m_team==2?'leftCol':'rightCol');
    // 缓存最后已知模型名：死亡后 m_model_name 可能为空，头像仍保留
    var mn=p.m_model_name;
    if(mn)modelCache[p.m_idx]=mn;
    else if(modelCache[p.m_idx])mn=modelCache[p.m_idx];
    var img=el('img','char');
    if(mn){
      img.src='./assets/characters/'+mn+'.png';
      img.onerror=function(){this.style.visibility='hidden'};
    }else{
      img.style.visibility='hidden';
    }
    var info=el('div','info');
    var nm=el('div','nm');nm.textContent=p.m_name;
    nm.onclick=function(){window.open('https://steamcommunity.com/profiles/'+p.m_steam_id,'_blank')};
    var tri=el('div','tri');tri.style.borderTopColor=colors[p.m_color];
    var row1=el('div','row1');
    var money=el('span','money');money.textContent='$'+p.m_money;
    row1.appendChild(money);
    var hp=el('span','ic');var hi=el('i','ico health');hp.appendChild(hi);hp.appendChild(document.createTextNode(p.m_health));
    var ar=el('span','ic');var ai=el('i','ico armor'+(p.m_has_helmet?' helmet':''));ar.appendChild(ai);ar.appendChild(document.createTextNode(p.m_armor));
    row1.appendChild(hp);row1.appendChild(ar);
    info.appendChild(nm);info.appendChild(tri);info.appendChild(row1);
    var weps=el('div','weps');
    var wd=p.m_weapons||{};
    if(wd.m_primary){var d=el('div',wd.m_active==wd.m_primary?'wep active':'wep');d.style.WebkitMask='url('+weaponPath(wd.m_primary)+') no-repeat center/contain';d.style.mask='url('+weaponPath(wd.m_primary)+') no-repeat center/contain';weps.appendChild(d)}
    if(wd.m_secondary){var d=el('div',wd.m_active==wd.m_secondary?'wep active':'wep');d.style.WebkitMask='url('+weaponPath(wd.m_secondary)+') no-repeat center/contain';d.style.mask='url('+weaponPath(wd.m_secondary)+') no-repeat center/contain';weps.appendChild(d)}
    (wd.m_melee||[]).forEach(function(m){var d=el('div',wd.m_active==m?'wep active':'wep');d.style.WebkitMask='url('+weaponPath(m)+') no-repeat center/contain';d.style.mask='url('+weaponPath(m)+') no-repeat center/contain';weps.appendChild(d)});
    info.appendChild(weps);
    var row2=el('div','weps');
    (wd.m_utilities||[]).forEach(function(u){var d=el('div','util');d.style.WebkitMask='url('+weaponPath(u)+') no-repeat center/contain';d.style.mask='url('+weaponPath(u)+') no-repeat center/contain';row2.appendChild(d)});
    var dots=4-(wd.m_utilities||[]).length;if(dots<0)dots=0;
    for(var i=0;i<dots;i++){var dot=el('div');dot.style.cssText='width:6px;height:6px;border-radius:50%;background:#b1d0e7';row2.appendChild(dot)}
    if(p.m_team==3&&p.m_has_defuser){var d=el('div','util');d.style.WebkitMask='url(./assets/icons/defuser.svg) no-repeat center/contain';d.style.mask='url(./assets/icons/defuser.svg) no-repeat center/contain';row2.appendChild(d)}
    if(p.m_team==2&&p.m_has_bomb){var d=el('div','util');d.style.WebkitMask='url(./assets/icons/c4.svg) no-repeat center/contain';d.style.mask='url(./assets/icons/c4.svg) no-repeat center/contain';row2.appendChild(d)}
    info.appendChild(row2);
    li.appendChild(img);li.appendChild(info);
    col.appendChild(li);
  });
}
function updateBombTimer(){
  var t=document.getElementById('bombTimer');
  if(bombData&&bombData.m_blow_time>0&&!bombData.m_is_defused){
    t.style.display='flex';
    var txt=bombData.m_blow_time.toFixed(1)+'s';
    if(bombData.m_is_defusing&&bombData.m_blow_time-bombData.m_defuse_time>0)
      txt+=' ('+bombData.m_defuse_time.toFixed(1)+'s)';
    document.getElementById('bombTime').textContent=txt;
  }else t.style.display='none';
}
function fetchData(){
  fetch('/api/radar').then(function(r){return r.json()}).then(function(d){
    if(!d||!d.m_local_team)return;
    players=d.m_players||[];
    localTeam=d.m_local_team;
    bombData=d.m_bomb;
    if(d.m_map&&d.m_map!=='invalid'&&d.m_map!==mapName){
      mapName=d.m_map;
      fetch('./data/'+mapName+'/data.json').then(function(r){return r.json()}).then(function(md){
        mapData=md;
        document.body.style.backgroundImage='url(./data/'+mapName+'/background.png)';
        document.getElementById('msg').style.display='none';
        renderPlayers();renderCards();
      }).catch(function(){});
    }else{
      renderPlayers();renderCards();
    }
    updateBombTimer();
  }).catch(function(){});
}
setInterval(fetchData,33);fetchData();
</script>
</body>
</html>)";

// ── HTTP 响应发送 ───────────────────────────────────────────────────────────
static void SendHead(SOCKET s, const char* contentType, size_t len, const char* extra = "")
{
    char head[512];
    int hl = snprintf(head, sizeof(head),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n%s\r\n",
        contentType, len, extra);
    send(s, head, hl, 0);
}

static void Send404(SOCKET s)
{
    const char* body = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    send(s, body, static_cast<int>(strlen(body)), 0);
}

static void SendString(SOCKET s, const char* contentType, const std::string& body)
{
    SendHead(s, contentType, body.size());
    send(s, body.data(), static_cast<int>(body.size()), 0);
}

// ── 静态文件（data/ 与 assets/）─────────────────────────────────────────────
static bool IsSafePath(const std::string& path)
{
    // 仅允许 data/ 与 assets/ 前缀的常规文件
    if (path.find("/data/") == 0 || path.find("/assets/") == 0)
        return path.find("..") == std::string::npos && path.find(":") == std::string::npos;
    return false;
}

static std::string GetMime(const std::string& path)
{
    if (path.ends_with(".png")) return "image/png";
    if (path.ends_with(".svg")) return "image/svg+xml";
    if (path.ends_with(".jpg") || path.ends_with(".jpeg")) return "image/jpeg";
    if (path.ends_with(".json")) return "application/json";
    if (path.ends_with(".woff2")) return "font/woff2";
    return "application/octet-stream";
}

static void SendFile(SOCKET s, const std::string& path)
{
    // 将 URL 路径映射到 exe 同目录
    std::wstring full = g_exe_dir;
    for (char c : path)
        full += static_cast<wchar_t>(c);

    HANDLE h = CreateFileW(full.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { Send404(s); return; }

    LARGE_INTEGER sz;
    GetFileSizeEx(h, &sz);
    if (sz.QuadPart <= 0 || sz.QuadPart > 64 * 1024 * 1024) { CloseHandle(h); Send404(s); return; }

    std::string body(static_cast<size_t>(sz.QuadPart), '\0');
    DWORD read = 0;
    if (!ReadFile(h, body.data(), static_cast<DWORD>(body.size()), &read, nullptr) || read != body.size())
    {
        CloseHandle(h);
        Send404(s);
        return;
    }
    CloseHandle(h);

    SendHead(s, GetMime(path).c_str(), body.size());
    send(s, body.data(), static_cast<int>(body.size()), 0);
}

// ── 请求处理 ────────────────────────────────────────────────────────────────
static void HandleClient(SOCKET client)
{
    char req[2048] = {};
    int n = 0;
    while (n < static_cast<int>(sizeof(req)) - 1)
    {
        int r = recv(client, req + n, 1, 0);
        if (r <= 0) break;
        n += r;
        if (n >= 4 && memcmp(req + n - 4, "\r\n\r\n", 4) == 0) break;
    }
    if (n == 0) { closesocket(client); return; }
    req[n] = '\0';

    char method[8] = {}, path[1024] = {};
    if (sscanf_s(req, "%7s %1023s", method, static_cast<unsigned>(sizeof(method)),
                 path, static_cast<unsigned>(sizeof(path))) < 2)
    {
        closesocket(client);
        return;
    }
    if (strcmp(method, "GET") != 0) { Send404(client); closesocket(client); return; }

    if (strcmp(path, "/api/radar") == 0)
    {
        // 读取共享数据前加读锁，避免与主循环写入竞争
        AcquireSRWLockShared(&f::m_lock);
        std::string body = f::m_data.dump();
        ReleaseSRWLockShared(&f::m_lock);
        SendString(client, "application/json", body);
    }
    else if (strcmp(path, "/") == 0 || strncmp(path, "/?", 2) == 0)
    {
        SendString(client, "text/html; charset=utf-8", g_html);
    }
    else if (IsSafePath(path))
    {
        SendFile(client, path);
    }
    else
    {
        Send404(client);
    }
    closesocket(client);
}

// ── 服务器线程 ──────────────────────────────────────────────────────────────
static unsigned __stdcall ClientThread(void* arg)
{
    SOCKET client = static_cast<SOCKET>(reinterpret_cast<uintptr_t>(arg));
    HandleClient(client);
    return 0;
}

static unsigned __stdcall ServerThread(void*)
{
    while (g_running)
    {
        SOCKET client = accept(g_listen, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;

        // 每连接一个线程，避免 API 轮询阻塞静态资源请求
        HANDLE h = reinterpret_cast<HANDLE>(
            _beginthreadex(nullptr, 0, ClientThread,
                reinterpret_cast<void*>(static_cast<uintptr_t>(client)), 0, nullptr));
        if (h)
            CloseHandle(h);
        else
            closesocket(client);
    }
    return 0;
}

namespace http
{
    bool init(int port)
    {
        // 记录 exe 目录
        wchar_t buf[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        g_exe_dir = buf;
        auto pos = g_exe_dir.find_last_of(L"\\/");
        if (pos != std::wstring::npos)
            g_exe_dir = g_exe_dir.substr(0, pos + 1);

        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

        g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (g_listen == INVALID_SOCKET) { WSACleanup(); return false; }

        int opt = 1;
        setsockopt(g_listen, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<u_short>(port));
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (bind(g_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR ||
            listen(g_listen, 8) == SOCKET_ERROR)
        {
            closesocket(g_listen);
            g_listen = INVALID_SOCKET;
            WSACleanup();
            return false;
        }

        g_running = true;
        g_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, ServerThread, nullptr, 0, nullptr));
        return g_thread != nullptr;
    }

    void shutdown()
    {
        g_running = false;
        if (g_listen != INVALID_SOCKET)
        {
            closesocket(g_listen);
            g_listen = INVALID_SOCKET;
        }
        if (g_thread)
        {
            WaitForSingleObject(g_thread, 2000);
            CloseHandle(g_thread);
            g_thread = nullptr;
        }
        WSACleanup();
    }
}
