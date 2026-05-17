// core/include/samp/protocol.h
#pragma once

// SA-MP пакеты
enum SAMPPacketID : unsigned char {
    PACKET_AUTH              = 0x06,
    PACKET_SPAWN             = 0x4A,
    PACKET_CHAT              = 0x65,
    PACKET_PLAYER_SYNC       = 0xA0,
    PACKET_DIALOG_RESPONSE   = 0x59,
    PACKET_PING              = 0x14,
};

// SA-MP версия клиента
static const unsigned int SAMP_VERSION = 0x03; // 0.3.7
static const char* SAMP_VERSION_STR   = "0.3.7";
