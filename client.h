// core/include/net/client.h
#pragma once
#include <string>
#include <functional>
#include "samp/protocol.h"

// Forward declarations
namespace RakNet { class RakPeerInterface; class BitStream; struct SystemAddress; }

namespace opensamp {

struct BotConfig {
    std::string nick;
    std::string server_ip;
    unsigned short port;
    std::string password;
};

// Callback типы
using OnConnectCallback    = std::function<void()>;
using OnDisconnectCallback = std::function<void(const std::string& reason)>;
using OnChatCallback       = std::function<void(int player_id, const std::string& message)>;
using OnDialogCallback     = std::function<void(int dialog_id, int type,
                                                const std::string& title,
                                                const std::string& content)>;

class SAMPClient {
public:
    explicit SAMPClient(const BotConfig& config);
    ~SAMPClient();

    // Управление подключением
    bool connect();
    void disconnect();
    void tick();  // Вызывать в game loop (~100ms)

    // Отправка пакетов
    void sendChat(const std::string& message);
    void sendDialogResponse(int dialog_id, int button, int list_item,
                            const std::string& input);
    void sendSync();  // Синхронизация позиции (keep-alive)

    // Коллбэки
    void onConnect(OnConnectCallback cb)    { on_connect_ = cb; }
    void onDisconnect(OnDisconnectCallback cb) { on_disconnect_ = cb; }
    void onChat(OnChatCallback cb)          { on_chat_ = cb; }
    void onDialog(OnDialogCallback cb)      { on_dialog_ = cb; }

    bool isConnected() const { return connected_; }

private:
    void handlePacket(unsigned char packet_id, RakNet::BitStream& bs);
    void sendAuth();

    BotConfig config_;
    RakNet::RakPeerInterface* peer_ = nullptr;
    bool connected_ = false;

    OnConnectCallback    on_connect_;
    OnDisconnectCallback on_disconnect_;
    OnChatCallback       on_chat_;
    OnDialogCallback     on_dialog_;
};

} // namespace opensamp
