#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace elkbledom::protocol {

inline constexpr auto DefaultAddress = "192.168.1.100";
inline constexpr auto TargetName = "Magic Home";
inline constexpr quint16 Port = 5577;
inline constexpr quint16 DiscoveryPort = 48899;
inline constexpr auto DiscoveryPayload = "HF-A11ASSISTHREAD";

// Magic Home / Flux LED: every TCP packet ends with a checksum byte
// equal to the sum of all preceding bytes modulo 256.
inline quint8 checksum(const QByteArray& data)
{
    quint8 sum = 0;
    for (const char byte : data) {
        sum = quint8(quint8(quint8(sum) + quint8(byte)));
    }
    return sum;
}

// 0x31 r g b ww mask 0x0f sum  — permanent color change.
inline QByteArray colorCommand(int red, int green, int blue)
{
    QByteArray command;
    command.reserve(8);
    command.append(char(0x31));
    command.append(static_cast<char>(qBound(0, red, 255)));
    command.append(static_cast<char>(qBound(0, green, 255)));
    command.append(static_cast<char>(qBound(0, blue, 255)));
    command.append(char(0x00)); // warm white
    command.append(char(0x00)); // mask
    command.append(char(0x0f));
    command.append(static_cast<char>(checksum(command)));
    return command;
}

// 0x71 0x23/0x24 0x0f sum  — power on/off.
inline QByteArray powerCommand(bool on)
{
    QByteArray command;
    command.reserve(4);
    command.append(char(0x71));
    command.append(on ? char(0x23) : char(0x24));
    command.append(char(0x0f));
    command.append(static_cast<char>(checksum(command)));
    return command;
}

inline QByteArray powerOnCommand()
{
    return powerCommand(true);
}

inline QByteArray powerOffCommand()
{
    return powerCommand(false);
}

// 0x81 0x8a 0x8b sum  — query current controller state.
inline QByteArray queryStateCommand()
{
    const QByteArray body = QByteArrayLiteral("\x81\x8a\x8b");
    QByteArray command = body;
    command.append(static_cast<char>(checksum(body)));
    return command;
}

// Map the app speed multiplier (0.05..10) into the device speed range (1..100).
inline int deviceSpeed(double appSpeed)
{
    return qBound(1, qRound(appSpeed * 30.0), 100);
}

// 0x61 patternCode delay 0x0f sum  — play a built-in pattern.
inline QByteArray builtinPatternCommand(int patternCode, int speed100)
{
    const int speed = qBound(0, speed100, 100);
    const int delay = 31 - qRound(speed / 100.0 * 30.0);
    QByteArray command;
    command.reserve(5);
    command.append(char(0x61));
    command.append(static_cast<char>(qBound(0, patternCode, 0xff)));
    command.append(static_cast<char>(qBound(1, delay, 31)));
    command.append(char(0x0f));
    command.append(static_cast<char>(checksum(command)));
    return command;
}

// Built-in pattern codes of the Magic Home firmware.
inline constexpr int PatternSevenColorCrossFade = 0x25;
inline constexpr int PatternRedGradual = 0x26;
inline constexpr int PatternGreenGradual = 0x27;
inline constexpr int PatternBlueGradual = 0x28;
inline constexpr int PatternYellowGradual = 0x29;
inline constexpr int PatternCyanGradual = 0x2a;
inline constexpr int PatternPurpleGradual = 0x2b;
inline constexpr int PatternWhiteGradual = 0x2c;
inline constexpr int PatternRedGreenCrossFade = 0x2d;
inline constexpr int PatternRedBlueCrossFade = 0x2e;
inline constexpr int PatternGreenBlueCrossFade = 0x2f;
inline constexpr int PatternSevenColorStrobe = 0x30;
inline constexpr int PatternRedStrobe = 0x31;
inline constexpr int PatternGreenStrobe = 0x32;
inline constexpr int PatternBlueStrobe = 0x33;
inline constexpr int PatternYellowStrobe = 0x34;
inline constexpr int PatternCyanStrobe = 0x35;
inline constexpr int PatternPurpleStrobe = 0x36;
inline constexpr int PatternWhiteStrobe = 0x37;
inline constexpr int PatternSevenColorJumping = 0x38;

// Effects that are executed on the controller itself via built-in patterns.
inline int builtinPatternCode(const QString& effectId)
{
    if (effectId == QStringLiteral("rainbow")) {
        return PatternSevenColorCrossFade;
    }
    if (effectId == QStringLiteral("strobe")) {
        return PatternSevenColorStrobe;
    }
    if (effectId == QStringLiteral("wave")) {
        return PatternSevenColorJumping;
    }
    if (effectId == QStringLiteral("chase")) {
        return PatternRedGreenCrossFade;
    }
    if (effectId == QStringLiteral("color_cycle")) {
        return PatternRedBlueCrossFade;
    }
    return -1;
}

inline bool usesBuiltinPattern(const QString& effectId)
{
    return builtinPatternCode(effectId) >= 0;
}

inline bool isColorCommand(const QByteArray& data)
{
    return data.size() == 8
        && quint8(data[0]) == 0x31
        && quint8(data[6]) == 0x0f;
}

inline bool isBuiltinPatternCommand(const QByteArray& data)
{
    return data.size() == 5
        && quint8(data[0]) == 0x61
        && quint8(data[4]) == 0x0f;
}

inline QString commandName(const QByteArray& data)
{
    if (isColorCommand(data)) {
        return QStringLiteral("color rgb=(%1,%2,%3)")
            .arg(quint8(data[1]))
            .arg(quint8(data[2]))
            .arg(quint8(data[3]));
    }
    if (data.size() >= 4 && quint8(data[0]) == 0x71) {
        return quint8(data[1]) == 0x23 ? QStringLiteral("on") : QStringLiteral("off");
    }
    if (isBuiltinPatternCommand(data)) {
        return QStringLiteral("pattern code=%1").arg(quint8(data[1]));
    }
    if (data.size() >= 4 && quint8(data[0]) == 0x81) {
        return QStringLiteral("query");
    }
    return QStringLiteral("unknown");
}

} // namespace elkbledom::protocol