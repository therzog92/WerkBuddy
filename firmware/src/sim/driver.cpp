#include "sim/driver.h"

#include "sim/playthrough.h"
#include "sim/screenshot.h"
#include "ui/nav.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_t = int;
#define DRIVE_CLOSE closesocket
#define DRIVE_IOCTL ioctlsocket
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SOCKET = int;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
#define DRIVE_CLOSE close
#define DRIVE_IOCTL(s, cmd, arg) fcntl(s, F_SETFL, *(arg) ? O_NONBLOCK : 0)
#endif

namespace wp {
namespace sim {
namespace {

constexpr int kDefaultPort = 9471;
constexpr int kMaxLine = 256;

struct PtrState {
  int16_t x = 0;
  int16_t y = 0;
  bool pressed = false;
};

struct Sample {
  int16_t x = 0;
  int16_t y = 0;
  bool pressed = false;
  uint32_t hold_ms = 0; /* keep this sample for at least this long */
};

bool g_enabled = false;
SOCKET g_listen = INVALID_SOCKET;
SOCKET g_client = INVALID_SOCKET;
PtrState g_ptr;
std::mutex g_mu;
std::vector<Sample> g_queue;
uint32_t g_sample_until = 0;
std::string g_line_buf;
std::string g_pending_reply;
bool g_have_reply = false;
bool g_quit = false;
bool g_async_play = false;

const char * screen_name(ui::Screen s) {
  switch (s) {
    case ui::Screen::Splash: return "splash";
    case ui::Screen::Hub: return "hub";
    case ui::Screen::Werk: return "werk";
    case ui::Screen::Compose: return "compose";
    case ui::Screen::Outgoing: return "outgoing";
    case ui::Screen::Incoming: return "incoming";
    case ui::Screen::GamesFolder: return "games";
    case ui::Screen::ActiveGames: return "active-games";
    case ui::Screen::UtilsFolder: return "utils";
    case ui::Screen::Ttt: return "ttt";
    case ui::Screen::Sttt: return "sttt";
    case ui::Screen::C4: return "c4";
    case ui::Screen::Bs: return "bs";
    case ui::Screen::Ck: return "ck";
    case ui::Screen::Mem: return "mem";
    case ui::Screen::Rv: return "rv";
    case ui::Screen::Db: return "db";
    case ui::Screen::Scoreboard: return "scoreboard";
    case ui::Screen::G2048: return "2048";
    case ui::Screen::Doodle: return "doodle";
    case ui::Screen::Settings: return "settings";
    case ui::Screen::Keyboard: return "keyboard";
    case ui::Screen::EmojiPicker: return "emoji";
    case ui::Screen::WifiScan: return "wifi";
    case ui::Screen::OtaReleases: return "ota";
    case ui::Screen::BgUpload: return "bg-upload";
    case ui::Screen::Timer: return "timer";
    case ui::Screen::Checklist: return "checklist";
    case ui::Screen::Calculator: return "calculator";
    case ui::Screen::PageHistory: return "page-history";
    case ui::Screen::Idle: return "idle";
    case ui::Screen::Setup: return "setup";
    default: return "unknown";
  }
}

void set_nonblock(SOCKET s) {
#ifdef _WIN32
  u_long mode = 1;
  DRIVE_IOCTL(s, FIONBIO, &mode);
#else
  int flags = fcntl(s, F_GETFL, 0);
  fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

void queue_sample(int16_t x, int16_t y, bool pressed, uint32_t hold_ms) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_queue.push_back(Sample{x, y, pressed, hold_ms});
}

void queue_tap(int x, int y) {
  queue_sample((int16_t)x, (int16_t)y, true, 40);
  queue_sample((int16_t)x, (int16_t)y, false, 40);
}

void queue_swipe(int x1, int y1, int x2, int y2, int ms) {
  if (ms < 80) ms = 80;
  const int steps = 8;
  queue_sample((int16_t)x1, (int16_t)y1, true, (uint32_t)(ms / steps));
  for (int i = 1; i <= steps; ++i) {
    const int x = x1 + (x2 - x1) * i / steps;
    const int y = y1 + (y2 - y1) * i / steps;
    queue_sample((int16_t)x, (int16_t)y, true, (uint32_t)(ms / steps));
  }
  queue_sample((int16_t)x2, (int16_t)y2, false, 40);
}

bool queue_empty() {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_queue.empty() && lv_tick_get() >= g_sample_until;
}

void reply(const char * msg) {
  g_pending_reply = msg;
  if (g_pending_reply.empty() || g_pending_reply.back() != '\n') g_pending_reply.push_back('\n');
  g_have_reply = true;
}

void handle_line(const char * line) {
  while (*line == ' ' || *line == '\t') ++line;
  if (!*line) {
    reply("OK");
    return;
  }

  char cmd[32] = {};
  if (std::sscanf(line, "%31s", cmd) != 1) {
    reply("FAIL bad-line");
    return;
  }

  if (!std::strcmp(cmd, "tap")) {
    int x = 0, y = 0;
    if (std::sscanf(line, "%*s %d %d", &x, &y) != 2) {
      reply("FAIL usage: tap X Y");
      return;
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > WP_HOR_RES - 1) x = WP_HOR_RES - 1;
    if (y > WP_VER_RES - 1) y = WP_VER_RES - 1;
    queue_tap(x, y);
    reply("OK");
    return;
  }

  if (!std::strcmp(cmd, "swipe")) {
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0, ms = 180;
    const int n = std::sscanf(line, "%*s %d %d %d %d %d", &x1, &y1, &x2, &y2, &ms);
    if (n < 4) {
      reply("FAIL usage: swipe X1 Y1 X2 Y2 [ms]");
      return;
    }
    queue_swipe(x1, y1, x2, y2, ms);
    reply("OK");
    return;
  }

  if (!std::strcmp(cmd, "wait")) {
    int ms = 0;
    if (std::sscanf(line, "%*s %d", &ms) != 1 || ms < 0) {
      reply("FAIL usage: wait MS");
      return;
    }
    /* Busy-wait via sample hold so command completes after MS. */
    queue_sample(g_ptr.x, g_ptr.y, false, (uint32_t)ms);
    reply("OK");
    return;
  }

  if (!std::strcmp(cmd, "shot")) {
    char name[64] = {};
    if (std::sscanf(line, "%*s %63s", name) == 1 && name[0])
      save_named_png(name);
    else
      save_preview_png();
    reply("OK");
    return;
  }

  if (!std::strcmp(cmd, "screen")) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "OK %s", screen_name(ui::current_screen()));
    reply(buf);
    return;
  }

  if (!std::strcmp(cmd, "quit")) {
    g_quit = true;
    reply("OK");
    return;
  }

  if (!std::strcmp(cmd, "ping")) {
    reply("OK pong");
    return;
  }

  if (!std::strcmp(cmd, "playthrough")) {
    char which[32] = "all";
    std::sscanf(line, "%*s %31s", which);
    if (!playthrough_start(which)) {
      reply("FAIL playthrough");
      return;
    }
    g_async_play = true; /* reply when finished */
    return;
  }

  /* Escape / seed helpers when tap coords are wrong — still prefer tap for real UX QA. */
  if (!std::strcmp(cmd, "nav")) {
    char where[32] = {};
    if (std::sscanf(line, "%*s %31s", where) != 1) {
      reply("FAIL usage: nav hub|games|utils|settings|werk|2048|...");
      return;
    }
    using namespace ui;
    if (!std::strcmp(where, "hub") || !std::strcmp(where, "home")) go_hub();
    else if (!std::strcmp(where, "games")) go_games_folder();
    else if (!std::strcmp(where, "utils")) go_utils_folder();
    else if (!std::strcmp(where, "settings")) go_settings();
    else if (!std::strcmp(where, "werk")) go_werk();
    else if (!std::strcmp(where, "2048") || !std::strcmp(where, "g2048")) go_g2048();
    else if (!std::strcmp(where, "doodle")) go_doodle();
    else if (!std::strcmp(where, "timer")) go_timer();
    else if (!std::strcmp(where, "checklist")) go_checklist();
    else if (!std::strcmp(where, "calculator") || !std::strcmp(where, "calc")) go_calculator();
    else if (!std::strcmp(where, "scoreboard")) go_scoreboard();
    else if (!std::strcmp(where, "active") || !std::strcmp(where, "active-games")) go_active_games();
    else if (!std::strcmp(where, "ttt")) go_ttt();
    else {
      reply("FAIL unknown-nav");
      return;
    }
    reply("OK");
    return;
  }

  reply("FAIL unknown-cmd");
}

void accept_clients() {
  if (g_listen == INVALID_SOCKET) return;
  if (g_client != INVALID_SOCKET) return;
  sockaddr_in addr{};
  socklen_t len = sizeof(addr);
  SOCKET c = accept(g_listen, (sockaddr *)&addr, &len);
  if (c == INVALID_SOCKET) return;
  set_nonblock(c);
  g_client = c;
  g_line_buf.clear();
  std::printf("[drive] client connected\n");
  const char * hello = "OK drive-ready\n";
  send(g_client, hello, (int)std::strlen(hello), 0);
}

void flush_reply() {
  if (!g_have_reply || g_client == INVALID_SOCKET) return;
  if (!queue_empty()) return; /* wait until pointer sequence finished */
  const int n = send(g_client, g_pending_reply.c_str(), (int)g_pending_reply.size(), 0);
  if (n == SOCKET_ERROR) {
    DRIVE_CLOSE(g_client);
    g_client = INVALID_SOCKET;
  }
  g_have_reply = false;
  g_pending_reply.clear();
  if (g_quit) std::exit(0);
}

void read_client() {
  if (g_client == INVALID_SOCKET) return;
  if (g_have_reply) return; /* one command at a time */

  char buf[128];
  const int n = recv(g_client, buf, sizeof(buf), 0);
  if (n == 0) {
    DRIVE_CLOSE(g_client);
    g_client = INVALID_SOCKET;
    return;
  }
  if (n < 0) return;

  g_line_buf.append(buf, buf + n);
  for (;;) {
    const auto pos = g_line_buf.find('\n');
    if (pos == std::string::npos) break;
    std::string line = g_line_buf.substr(0, pos);
    g_line_buf.erase(0, pos + 1);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    handle_line(line.c_str());
    break; /* one command; wait for ACK before next */
  }
}

void poll_net(lv_timer_t * /*t*/) {
  accept_clients();
  if (g_async_play) {
    if (playthrough_tick()) {
      reply(playthrough_reply());
      g_async_play = false;
    }
    flush_reply();
    return;
  }
  read_client();
  flush_reply();
}

void read_cb(lv_indev_t * /*indev*/, lv_indev_data_t * data) {
  const uint32_t now = lv_tick_get();
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_queue.empty() && now >= g_sample_until) {
    const Sample s = g_queue.front();
    g_queue.erase(g_queue.begin());
    g_ptr.x = s.x;
    g_ptr.y = s.y;
    g_ptr.pressed = s.pressed;
    g_sample_until = now + (s.hold_ms ? s.hold_ms : 1);
  }
  data->point.x = g_ptr.x;
  data->point.y = g_ptr.y;
  data->state = g_ptr.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->continue_reading = false;
}

bool start_listen(int port) {
#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
#endif
  g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (g_listen == INVALID_SOCKET) return false;

  int yes = 1;
  setsockopt(g_listen, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
  set_nonblock(g_listen);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons((uint16_t)port);
  if (bind(g_listen, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
    DRIVE_CLOSE(g_listen);
    g_listen = INVALID_SOCKET;
    return false;
  }
  if (listen(g_listen, 1) == SOCKET_ERROR) {
    DRIVE_CLOSE(g_listen);
    g_listen = INVALID_SOCKET;
    return false;
  }
  std::printf("[drive] listening on 127.0.0.1:%d\n", port);
  return true;
}

}  // namespace

void driver_init(lv_display_t * disp) {
  const char * en = std::getenv("WERKPAGER_DRIVE");
  if (!en || en[0] != '1') return;

  int port = kDefaultPort;
  if (const char * p = std::getenv("WERKPAGER_DRIVE_PORT"); p && p[0]) port = std::atoi(p);
  if (!start_listen(port)) {
    std::fprintf(stderr, "[drive] failed to bind port %d\n", port);
    return;
  }

  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, read_cb);
  if (disp) lv_indev_set_display(indev, disp);

  lv_timer_create(poll_net, 10, nullptr);
  g_enabled = true;
}

bool driver_enabled() { return g_enabled; }

}  // namespace sim
}  // namespace wp
