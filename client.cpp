// core/src/net/client.cpp
#include "net/client.h"
#include <RakPeerInterface.h>
#include <BitStream.h> 
#include <MessageIdentifiers.h>
#include <iostream>

namespace opensamp {

SAMPClient::SAMPClient(const BotConfig& config)
    : config_(config) {
    peer_ = RakNet::RakPeerInterface::GetInstance();
}

SAMPClient::~SAMPClient() {
    disconnect();
    RakNet::RakPeerInterface::DestroyInstance(peer_);
}

bool SAMPClient::connect() {
    // Инициализация RakNet
    RakNet::SocketDescriptor sd;
    peer_->Startup(1, &sd, 1);
    peer_->SetMaximumIncomingConnections(0);

    // Подключение к серверу
    auto result = peer_->Connect(
        config_.server_ip.c_str(),
        config_.port,
        nullptr, 0
    );

    if (result != RakNet::CONNECTION_ATTEMPT_STARTED) {
        std::cerr << "[opensamp] Не удалось начать подключение" << std::endl;
        return false;
    }

    std::cout << "[opensamp] Подключаемся к "
              << config_.server_ip << ":" << config_.port << std::endl;
    return true;
}

void SAMPClient::disconnect() {
    if (peer_) {
        peer_->Shutdown(300);
        connected_ = false;
    }
}

void SAMPClient::tick() {
    RakNet::Packet* packet;
    while ((packet = peer_->Receive()) != nullptr) {
        unsigned char packet_id = packet->data[0];

        switch (packet_id) {
            case ID_CONNECTION_REQUEST_ACCEPTED:
                std::cout << "[opensamp] RakNet соединение установлено" << std::endl;
                sendAuth(); // Отправляем SA-MP авторизацию
                break;

            case ID_DISCONNECTION_NOTIFICATION:
            case ID_CONNECTION_LOST:
                connected_ = false;
                if (on_disconnect_) on_disconnect_("connection_lost");
                break;

            default: {
                RakNet::BitStream bs(packet->data, packet->length, false);
                handlePacket(packet_id, bs);
                break;
            }
        }

        peer_->DeallocatePacket(packet);
    }
}

void SAMPClient::sendAuth() {
    // SA-MP авторизационный пакет
    // Структуру точнее смотри в open.mp исходнике:
    // https://github.com/openmultiplayer/open.mp
    RakNet::BitStream bs;
    bs.Write((unsigned char)PACKET_AUTH);
    bs.Write((unsigned int)SAMP_VERSION);

    // Ник (Pascal string: длина + байты)
    unsigned char nick_len = (unsigned char)config_.nick.size();
    bs.Write(nick_len);
    bs.Write(config_.nick.c_str(), nick_len);

    // Пароль
    unsigned char pass_len = (unsigned char)config_.password.size();
    bs.Write(pass_len);
    if (pass_len > 0)
        bs.Write(config_.password.c_str(), pass_len);

    peer_->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0,
                RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

    std::cout << "[opensamp] Auth отправлен, ник: " << config_.nick << std::endl;
}

void SAMPClient::sendChat(const std::string& message) {
    RakNet::BitStream bs;
    bs.Write((unsigned char)PACKET_CHAT);
    unsigned char len = (unsigned char)message.size();
    bs.Write(len);
    bs.Write(message.c_str(), len);

    peer_->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0,
                RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
}

void SAMPClient::sendDialogResponse(int dialog_id, int button,
                                     int list_item, const std::string& input) {
    RakNet::BitStream bs;
    bs.Write((unsigned char)PACKET_DIALOG_RESPONSE);
    bs.Write((unsigned short)dialog_id);
    bs.Write((unsigned char)button);
    bs.Write((unsigned short)list_item);
    unsigned char len = (unsigned char)input.size();
    bs.Write(len);
    if (len > 0) bs.Write(input.c_str(), len);

    peer_->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0,
                RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
}

void SAMPClient::handlePacket(unsigned char packet_id, RakNet::BitStream& bs) {
    switch (packet_id) {
        case PACKET_CHAT: {
            // TODO: распарсить сообщение чата
            // Референс: open.mp ChatComponent
            if (on_chat_) on_chat_(0, "[raw chat packet]");
            break;
        }
        case PACKET_DIALOG_RESPONSE: {
            // TODO: распарсить диалог
            if (on_dialog_) on_dialog_(0, 0, "title", "content");
            break;
        }
        case PACKET_SPAWN: {
            connected_ = true;
            std::cout << "[opensamp] Спавн! Бот в игре." << std::endl;
            if (on_connect_) on_connect_();
            break;
        }
        default:
            // Логируем неизвестные пакеты при отладке
            // std::cout << "[opensamp] Unknown packet: 0x"
            //           << std::hex << (int)packet_id << std::endl;
            break;
    }
}

} // namespace opensamp
