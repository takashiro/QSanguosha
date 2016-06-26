/********************************************************************
    Copyright (c) 2013-2015 - Mogara

    This file is part of QSanguosha.

    This game engine is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 3.0
    of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    See the LICENSE file for more details.

    Mogara
*********************************************************************/

#include "lobby.h"
#include "client.h"

#include <CClientUser>

Lobby::Lobby(QQuickItem *parent)
    : QQuickItem(parent)
    , m_client(Client::instance())
{
    connect(m_client, &Client::roomConfigChanged, this, &Lobby::onRoomConfigChanged);
    connect(m_client, &Client::roomListUpdated, this, &Lobby::roomListUpdated);
    connect(m_client, &Client::roomEntered, this, &Lobby::onRoomEntered);
    connect(m_client, &Client::userAdded, this, &Lobby::onUserAdded);
    connect(m_client, &Client::userRemoved, this, &Lobby::onUserRemoved);
    connect(m_client, &Client::systemMessage, this, &Lobby::onSystemMessageReceived);
    connect(m_client, &Client::gameStarted, this, &Lobby::gameStarted);
}

void Lobby::createRoom()
{
    m_client->createRoom();
}

void Lobby::speakToServer(const QString &text)
{
    m_client->speakToServer(text);
}

void Lobby::updateRoomList()
{
    m_client->fetchRoomList();
}

void Lobby::updateConfig(const QString &key, const QVariant &value)
{
    m_client->configureRoom(key, value);
}

void Lobby::onCreateButtonClicked()
{
    if (property("roomId").toUInt() == 0)
        m_client->createRoom();
    else
        m_client->exitRoom();
}

void Lobby::onRoomListItemClicked(uint id)
{
    m_client->enterRoom(id);
}

void Lobby::onReadyButtonClicked()
{
    //@to-do: Get ready or start game
    m_client->startGame();
}

void Lobby::onAddRobotButtonClicked()
{
    m_client->addRobot();
}

void Lobby::onRoomConfigChanged(const QString &name, const QVariant &value)
{
    if (name == "ownerId") {
        setProperty("isOwner", value.toUInt() == m_client->self()->id());
    } else if (name == "id") {
        setProperty("roomId", value);
    } else {
        setConfig(name, value);
    }
}

void Lobby::onRoomEntered(const QVariant &config)
{
    QVariantMap info = config.toMap();
    if (!info.isEmpty()) {
        setProperty("roomId", info["id"]);
        setProperty("chatLog", "");

        setConfig("name", info["name"]);
    }

    CClientUser *self = m_client->self();
    if (self) {
        setProperty("isOwner", info["ownerId"].toUInt() == self->id());
        setProperty("userAvatar", self->avatar());

        setConfig("name", self->screenName());
    }

    foreach (const CClientUser *user, m_client->users())
        connect(user, &CClientUser::speak, this, &Lobby::onUserSpeaking, Qt::UniqueConnection);
}

void Lobby::onUserAdded(const CClientUser *user)
{
    connect(user, &CClientUser::speak, this, &Lobby::onUserSpeaking);
    emit messageLogged(tr("User %1(%2) logged in.").arg(user->screenName()).arg(user->id()));
}

void Lobby::onUserRemoved(const CClientUser *user)
{
    disconnect(user, &CClientUser::speak, this, &Lobby::onUserSpeaking);
    emit messageLogged(tr("User %1(%2) logged out.").arg(user->screenName()).arg(user->id()));
}

void Lobby::onUserSpeaking(const QString &message)
{
    CClientUser *user = qobject_cast<CClientUser *>(sender());
    if (user == nullptr)
        return;
    emit messageLogged(tr("%1(%2): %3").arg(user->screenName()).arg(user->id()).arg(message));
}

void Lobby::onSystemMessageReceived(const QString &message)
{
    emit messageLogged(tr("System: %1").arg(message));
}

C_REGISTER_QMLTYPE("Sanguosha", 1, 0, Lobby)
