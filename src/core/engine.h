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

#ifndef ENGINE_H
#define ENGINE_H

#include <QString>
#include <QList>
#include <QMap>

class Card;
class General;
class Package;
class Skill;
class GameMode;

class Engine
{
public:
    static Engine *instance();
    ~Engine();

    void addMode(GameMode *mode) { m_modes << mode; }
    const GameMode *mode(const QString &name) const;
    QList<const GameMode *> modes() const { return m_modes; }

    void addPackage(Package *package);
    const Package *package(const QString &name) const { return m_packages.value(name); }
    QList<const Package *> packages() const;
    QList<const Package *> getPackages(const GameMode *mode) const;

    QList<const General *> getGenerals(bool includeHidden = false) const;
    const General *getGeneral(uint id) const { return m_generals.value(id); }

    QList<const Card *> getCards() const { return m_cards.values(); }
    const Card *getCard(uint id) const { return m_cards.value(id); }

    const Skill *getSkill(uint id) const { return m_skills.value(id); }

private:
    Engine();

    QList<const GameMode *> m_modes;
    QMap<QString, Package *> m_packages;
    QMap<uint, const General *> m_generals;
    QMap<uint, const Card *> m_cards;
    QMap<uint, const Skill *> m_skills;
};

#define ADD_PACKAGE(name) namespace\
{\
struct PackageAdder\
{\
    PackageAdder()\
    {\
        Engine::instance()->addPackage(new name##Package);\
    }\
};\
PackageAdder __packageAdder__;\
}

#define ADD_MODE(name) namespace\
{\
struct ModeAdder\
{\
    ModeAdder()\
    {\
        Engine::instance()->addMode(new name##Mode);\
    }\
};\
ModeAdder __modeAdder__;\
}

#endif // ENGINE_H
