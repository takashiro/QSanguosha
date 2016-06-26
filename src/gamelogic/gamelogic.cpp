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

#include "card.h"
#include "cardarea.h"
#include "engine.h"
#include "eventhandler.h"
#include "gamelogic.h"
#include "gamemode.h"
#include "gamerule.h"
#include "general.h"
#include "package.h"
#include "protocol.h"
#include "roomsettings.h"
#include "serverplayer.h"
#include "util.h"

#include <CRoom>
#include <CServer>
#include <CServerRobot>
#include <CServerUser>

#include <QDateTime>
#include <QThread>

GameLogic::GameLogic(CRoom *parent)
    : CAbstractGameLogic(parent)
    , m_currentPlayer(nullptr)
    , m_gameRule(nullptr)
    , m_skipGameRule(false)
    , m_round(0)
    , m_reshufflingCount(0)
{
    m_drawPile = new CardArea(CardArea::DrawPile);
    m_discardPile = new CardArea(CardArea::DiscardPile);
    m_table = new CardArea(CardArea::Table);
    m_table->setKeepVirtualCard(true);
    m_wugu = new CardArea(CardArea::Wugu);
}

GameLogic::~GameLogic()
{
    delete m_wugu;
    delete m_table;
    delete m_discardPile;
    delete m_drawPile;

    foreach (Card *card, m_cards)
        delete card;
}

void GameLogic::setGameRule(const GameRule *rule) {
    if (m_gameRule) {
        foreach (EventType e, m_gameRule->events()) {
            if (m_handlers[e].contains(m_gameRule))
                m_handlers[e].removeOne(m_gameRule);
        }
    }

    m_gameRule = rule;
    if (rule) {
        foreach (EventType e, rule->events())
            m_handlers[e].append(m_gameRule);
    }
}

void GameLogic::addEventHandler(const EventHandler *handler)
{
    QSet<EventType> events = handler->events();
    foreach(EventType event, events) {
        if (!m_handlers[event].contains(handler))
            m_handlers[event] << handler;
    }
}

void GameLogic::removeEventHandler(const EventHandler *handler)
{
    QSet<EventType> events = handler->events();
    foreach (EventType event, events)
        m_handlers[event].removeOne(handler);
}

bool GameLogic::trigger(EventType event, ServerPlayer *target)
{
    QVariant data;
    return trigger(event, target, data);
}

bool GameLogic::trigger(EventType event, ServerPlayer *target, QVariant &data)
{
    QList<const EventHandler *> &handlers = m_handlers[event];

    std::stable_sort(handlers.begin(), handlers.end(), [event](const EventHandler *a, const EventHandler *b){
        return a->priority(event) > b->priority(event);
    });

    bool broken = false;
    int triggerableIndex = 0;
    while (triggerableIndex < handlers.length()) {
        int currentPriority = 0;
        QMap<ServerPlayer *, EventList> triggerableEvents;

        //Construct triggerableEvents
        do {
            const EventHandler *handler = handlers.at(triggerableIndex);
            if (triggerableEvents.isEmpty() || handler->priority(event) == currentPriority) {
                EventMap events = handler->triggerable(this, event, target, data);
                if (events.size() > 0) {
                    QList<ServerPlayer *> players = this->players();
                    foreach (ServerPlayer *p, players) {
                        if (!events.contains(p))
                            continue;

                        QList<Event> ds = events.values(p);
                        triggerableEvents[p] << ds;
                        currentPriority = ds.last().handler->priority(event);
                    }
                }
            } else if (handler->priority(event) != currentPriority) {
                break;
            }
            triggerableIndex++;
        } while (triggerableIndex < handlers.length());

        if (!triggerableEvents.isEmpty()) {
            QList<ServerPlayer *> allPlayers = this->allPlayers(true);
            foreach (ServerPlayer *invoker, allPlayers) {
                if (!triggerableEvents.contains(invoker))
                    continue;

                forever {
                    EventList &events = triggerableEvents[invoker];
                    if (events.isEmpty())
                        break;

                    bool hasCompulsory = false;
                    foreach (const Event &d, events) {
                        if (d.handler->isCompulsory()) {
                            hasCompulsory = true;
                            break;
                        }
                    }

                    //Ask the invoker to determine the trigger order
                    Event choice;
                    if (events.length() > 1)
                        choice = invoker->askForTriggerOrder(events, !hasCompulsory);
                    else if (hasCompulsory)
                        choice = events.first();
                    else
                        choice = invoker->askForTriggerOrder(events, true);

                    //If the user selects "cancel"
                    if (!choice.isValid())
                        break;

                    ServerPlayer *eventTarget = choice.to.isEmpty() ? target : choice.to.first();

                    //Ask the invoker for cost
                    bool takeEffect = choice.handler->onCost(this, event, eventTarget, data, invoker);

                    //Take effect
                    if (takeEffect) {
                        broken = choice.handler->effect(this, event, eventTarget, data, invoker);
                        if (broken)
                            break;
                    }

                    //Remove targets that are in front of the triggered target
                    for (int i = 0; i < events.length(); i++) {
                        Event &d = events[i];
                        if (d.handler != choice.handler)
                            continue;

                        foreach (ServerPlayer *to, choice.to) {
                            int index = d.to.indexOf(to);
                            if (index == d.to.length() - 1) {
                                events.removeAt(i);
                                i--;
                            } else {
                                d.to = d.to.mid(index + 1);
                            }
                        }

                        if (choice.to.isEmpty()) {
                            events.removeAt(i);
                            i--;
                        }
                    }
                }
            }
        }
    }

    return broken;
}

QList<ServerPlayer *> GameLogic::players() const
{
    QList<ServerPlayer *> players;
    auto abstractPlayers = this->abstractPlayers();
    foreach (CAbstractPlayer *p, abstractPlayers)
        players << qobject_cast<ServerPlayer *>(p);
    return players;
}

ServerPlayer *GameLogic::findPlayer(uint id) const
{
    return qobject_cast<ServerPlayer *>(findAbstractPlayer(id));
}

ServerPlayer *GameLogic::findPlayer(CServerAgent *agent) const
{
    return qobject_cast<ServerPlayer *>(findAbstractPlayer(agent));
}

QList<ServerPlayer *> GameLogic::allPlayers(bool includeDead) const
{
    QList<ServerPlayer *> players = this->players();
    ServerPlayer *current = currentPlayer();
    if (current == nullptr)
        return players;

    std::sort(players.begin(), players.end(), [](const ServerPlayer *a, const ServerPlayer *b){
        return a->seat() < b->seat();
    });

    int currentIndex = players.indexOf(current);
    if (currentIndex == -1)
        return players;

    QList<ServerPlayer *> allPlayers;
    for (int i = currentIndex; i < players.length(); i++) {
        if (includeDead || players.at(i)->isAlive())
            allPlayers << players.at(i);
    }
    for (int i = 0; i < currentIndex; i++) {
        if (includeDead || players.at(i)->isAlive())
            allPlayers << players.at(i);
    }

    if (current->phase() == Player::Inactive && allPlayers.contains(current)) {
        allPlayers.removeOne(current);
        allPlayers.append(current);
    }

    return allPlayers;
}

QList<ServerPlayer *> GameLogic::otherPlayers(ServerPlayer *except, bool includeDead) const
{
    QList<ServerPlayer *> players = allPlayers(includeDead);
    if (except && (except->isAlive() || includeDead))
        players.removeOne(except);
    return players;
}

void GameLogic::sortByActionOrder(QList<ServerPlayer *> &players) const
{
    QList<ServerPlayer *> allPlayers = this->allPlayers(true);

    QMap<ServerPlayer *, int> actionOrder;
    foreach (ServerPlayer *player, players)
        actionOrder[player] = allPlayers.indexOf(player);

    std::sort(allPlayers.begin(), allPlayers.end(), [&actionOrder](ServerPlayer *a, ServerPlayer *b){
        return actionOrder.value(a) < actionOrder.value(b);
    });
}

Card *GameLogic::getDrawPileCard()
{
    if (m_drawPile->length() < 1)
        reshuffleDrawPile();
    return m_drawPile->first();
}

QList<Card *> GameLogic::getDrawPileCards(int n)
{
    if (m_drawPile->length() < n)
        reshuffleDrawPile();
    return m_drawPile->first(n);
}

void GameLogic::reshuffleDrawPile()
{
    if (m_discardPile->length() <= 0) {
        //@to-do: stand off. Game over.
    }

    m_reshufflingCount++;

    //@to-do: check reshuffling count limit
    /*if (limit > 0 && times == limit)
        gameOver(".");*/

    QList<Card *> cards = m_discardPile->cards();
    m_discardPile->clear();
    qShuffle(cards);
    foreach (Card *card, cards)
        m_cardPosition[card] = m_drawPile;
    m_drawPile->add(cards, CardArea::Bottom);
}

void GameLogic::moveCards(const CardsMoveStruct &move)
{
    moveCards(QList<CardsMoveStruct>() << move);
}

void GameLogic::moveCards(QList<CardsMoveStruct> &moves)
{
    filterCardsMove(moves);
    QVariant moveData = QVariant::fromValue(&moves);
    QList<ServerPlayer *> allPlayers = this->allPlayers();
    foreach (ServerPlayer *player, allPlayers)
        trigger(BeforeCardsMove, player, moveData);

    filterCardsMove(moves);
    allPlayers = this->allPlayers();
    foreach (ServerPlayer *player, allPlayers)
        trigger(CardsMove, player, moveData);

    filterCardsMove(moves);
    for (int i = 0 ; i < moves.length(); i++) {
        const CardsMoveStruct &move = moves.at(i);
        CardArea *to = findArea(move.to);
        if (to == nullptr)
            continue;

        CardArea *from = findArea(move.from);
        if (from == nullptr)
            continue;

        foreach (Card *card, move.cards) {
            if (from != m_cardPosition.value(card))
                continue;
            if (from->remove(card)) {
                to->add(card, move.to.direction);
                m_cardPosition[card] = to;
            }
        }
    }

    QList<ServerPlayer *> viewers = players();
    foreach (ServerPlayer *viewer, viewers) {
        QVariantList data;
        foreach (const CardsMoveStruct &move, moves)
            data << move.toVariant(move.isRelevant(viewer));
        CServerAgent *agent = viewer->agent();
        agent->notify(S_COMMAND_MOVE_CARDS, data);
    }

    allPlayers = this->allPlayers();
    foreach (ServerPlayer *player, allPlayers)
        trigger(AfterCardsMove, player, moveData);
}

bool GameLogic::useCard(CardUseStruct &use)
{
    if (use.card == nullptr || use.from == nullptr)
        return false;

    //Initialize isHandcard
    use.isHandcard = true;
    QList<Card *> realCards = use.card->realCards();
    foreach (Card *card, realCards) {
        CardArea *area = m_cardPosition[card];
        if (area == nullptr || area->owner() != use.from || area->type() != CardArea::Hand) {
            use.isHandcard = false;
            break;
        }
    }

    if (use.from->phase() == Player::Play && use.addHistory)
        use.from->addCardHistory(use.card->objectName());

    try {
        use.card->onUse(this, use);

        QVariant data = QVariant::fromValue(&use);
        trigger(CardUsed, use.from, data);

        if (use.from) {
            trigger(TargetChoosing, use.from, data);

            QVariantMap args;
            args["from"] = use.from->id();
            //args["cards"]
            QVariantList tos;
            foreach (ServerPlayer *to, use.to)
                tos << to->id();
            args["to"] = tos;
            room()->broadcastNotification(S_COMMAND_USE_CARD, args);

            if (use.from) {
                if (!use.to.isEmpty()) {
                    foreach (ServerPlayer *to, use.to) {
                        if (!use.to.contains(to))
                            continue;
                        trigger(TargetConfirming, to, data);
                    }

                    if (use.from && !use.to.isEmpty()) {
                        trigger(TargetChosen, use.from, data);

                        if (use.from && !use.to.isEmpty()) {
                            foreach (ServerPlayer *to, use.to) {
                                if (!use.to.contains(to))
                                    continue;
                                trigger(TargetConfirmed, to, data);
                            }

                            use.card->use(this, use);
                        }
                    }
                } else if (use.target) {
                    use.card->use(this, use);
                }
            }
        }

        trigger(CardFinished, use.from, data);

    } catch (EventType e) {
        //@to-do: handle TurnBroken and StageChange
        throw e;
    }

    return true;
}

bool GameLogic::takeCardEffect(CardEffectStruct &effect)
{
    QVariant data = QVariant::fromValue(&effect);
    bool canceled = false;
    if (effect.to) {
        if (effect.to->isAlive()) {
            canceled = trigger(CardEffect, effect.to, data);
            if (!canceled) {
                canceled = trigger(CardEffected, effect.to, data);
                if (!canceled) {
                    effect.use.card->onEffect(this, effect);
                    if (effect.to->isAlive() && !effect.isNullified())
                        effect.use.card->effect(this, effect);
                }
            }
        }
    } else if (effect.use.target) {
        effect.use.card->onEffect(this, effect);
        if (!effect.isNullified())
            effect.use.card->effect(this, effect);
    }
    trigger(PostCardEffected, effect.to, data);
    return !canceled;
}

bool GameLogic::respondCard(CardResponseStruct &response)
{
    CardsMoveStruct move;
    move.cards << response.card;
    move.to.type = CardArea::Table;
    move.isOpen = true;
    moveCards(move);

    QVariant data = QVariant::fromValue(&response);
    bool broken = trigger(CardResponded, response.from, data);

    if (response.card && m_table->contains(response.card)) {
        CardsMoveStruct move;
        move.cards << response.card;
        move.to.type = CardArea::DiscardPile;
        move.isOpen = true;
        moveCards(move);
    }

    return !broken;
}

void GameLogic::judge(JudgeStruct &judge)
{
    QVariant data = QVariant::fromValue(&judge);

    if (trigger(StartJudge, judge.who, data))
        return;

    judge.card = getDrawPileCard();
    judge.updateResult();

    CardsMoveStruct move;
    move.cards << judge.card;
    move.to.type = CardArea::Judge;
    move.to.owner = judge.who;
    move.isOpen = true;
    moveCards(move);

    QList<ServerPlayer *> players = allPlayers();
    foreach (ServerPlayer *player, players) {
        if (trigger(AskForRetrial, player, data))
            break;
    }
    trigger(FinishRetrial, judge.who, data);
    trigger(FinishJudge, judge.who, data);

    const CardArea *judgeCards = judge.who->judgeCards();
    if (judgeCards->contains(judge.card)) {
        CardsMoveStruct move;
        move.cards << judge.card;
        move.to.type = CardArea::DiscardPile;
        move.isOpen = true;
        moveCards(move);
    }
}

QList<Card *> GameLogic::findCards(const QVariant &data)
{
    QList<Card *> cards;
    QVariantList dataList = data.toList();
    foreach (const QVariant &cardId, dataList) {
        Card *card = findCard(cardId.toUInt());
        if (card)
            cards << card;
    }
    return cards;
}

void GameLogic::damage(DamageStruct &damage)
{
    if (damage.to == nullptr || damage.to->isDead())
        return;

    QVariant data = QVariant::fromValue(&damage);
    if (!damage.chain && !damage.transfer) {
        trigger(ConfirmDamage, damage.from, data);
    }

    if (trigger(BeforeDamage, damage.from, data))
        return;

    try {
        do {
            if (trigger(DamageStart, damage.to, data))
                break;

            if (damage.from && trigger(Damaging, damage.from, data))
                break;

            if (damage.to && trigger(Damaged, damage.to, data))
                break;
        } while (false);

        if (damage.to)
            trigger(BeforeHpReduced, damage.to, data);

        if (damage.to) {
            QVariantList arg;
            arg << damage.to->id();
            arg << damage.nature;
            arg << damage.damage;
            room()->broadcastNotification(S_COMMAND_DAMAGE, arg);

            int newHp = damage.to->hp() - damage.damage;
            damage.to->setHp(newHp);
            damage.to->broadcastProperty("hp");

            trigger(AfterHpReduced, damage.to, data);
        }

        if (damage.from)
            trigger(AfterDamaging, damage.from, data);

        if (damage.to)
            trigger(AfterDamaged, damage.to, data);

        if (damage.to)
            trigger(DamageComplete, damage.to, data);

    } catch (EventType e) {
        //@to-do: handle TurnBroken and StageChange
        throw e;
    }
}

void GameLogic::loseHp(ServerPlayer *victim, int lose)
{
    if (lose <= 0 || victim->isDead())
        return;

    QVariant data = lose;
    if (trigger(HpLost, victim, data))
        return;

    lose = data.toInt();
    if (lose <= 0)
        return;

    victim->setHp(victim->hp() - lose);
    victim->broadcastProperty("hp");

    QVariantMap arg;
    arg["victimId"] = victim->id();
    arg["loseHp"] = lose;
    room()->broadcastNotification(S_COMMAND_LOSE_HP, arg);

    trigger(AfterHpReduced, victim, data);
    trigger(AfterHpLost, victim, data);
}

void GameLogic::recover(RecoverStruct &recover)
{
    if (recover.to == nullptr || recover.to->lostHp() == 0 || recover.to->isDead())
        return;

    QVariant data = QVariant::fromValue(&recover);
    if (trigger(BeforeRecover, recover.to, data))
        return;
    if (recover.to == nullptr)
        return;

    int newHp = qMin(recover.to->hp() + recover.recover, recover.to->maxHp());
    recover.to->setHp(newHp);
    recover.to->broadcastProperty("hp");

    QVariantMap arg;
    arg["from"] = recover.from ? recover.from->id() : 0;
    arg["to"] = recover.to->id();
    arg["num"] = recover.recover;
    room()->broadcastNotification(S_COMMAND_RECOVER, arg);

    trigger(AfterRecover, recover.to, data);
}

void GameLogic::killPlayer(ServerPlayer *victim, DamageStruct *damage)
{
    victim->setAlive(false);
    victim->broadcastProperty("alive");
    victim->broadcastProperty("role");

    DeathStruct death;
    death.who = victim;
    death.damage = damage;
    QVariant data = QVariant::fromValue(&death);

    trigger(BeforeGameOverJudge, victim, data);
    trigger(GameOverJudge, victim, data);

    trigger(Died, victim, data);
    trigger(BuryVictim, victim, data);
}

void GameLogic::gameOver(const QList<ServerPlayer *> &winners)
{
    QVariantList data;
    foreach (ServerPlayer *winner, winners)
        data << winner->id();
    room()->broadcastNotification(S_COMMAND_GAME_OVER, data);
    throw GameFinish;
}

QMap<uint, QList<const General *>> GameLogic::broadcastRequestForGenerals(const QList<ServerPlayer *> &players, int num, int limit)
{
    GeneralList generals;
    QList<const Package *> packages = this->packages();
    foreach(const Package *package, packages)
        generals << package->generals();
    qShuffle(generals);

    int minCandidateNum = limit * players.length();
    while (minCandidateNum > generals.length())
        generals << generals.mid(0, minCandidateNum - generals.length());

    QMap<ServerPlayer *, GeneralList> playerCandidates;

    foreach (ServerPlayer *player, players) {
        GeneralList candidates = generals.mid((player->seat() - 1) * limit, limit);
        playerCandidates[player] = candidates;

        QVariantList candidateData;
        foreach (const General *general, candidates)
            candidateData << general->id();

        QVariantList bannedPairData;
        //@todo: load banned pairs

        QVariantMap data;
        data["num"] = num;
        data["candidates"] = candidateData;
        data["banned"] = bannedPairData;

        CServerAgent *agent = findAgent(player);
        agent->prepareRequest(S_COMMAND_CHOOSE_GENERAL, data);
    }

    //@to-do: timeout should be loaded from config
    CRoom *room = this->room();
    QList<CServerAgent *> agents;
    foreach (ServerPlayer *player, players)
        agents << player->agent();
    room->broadcastRequest(agents, settings()->timeout * 1000);

    QMap<uint, GeneralList> result;
    foreach (ServerPlayer *player, players) {
        const GeneralList &candidates = playerCandidates[player];

        GeneralList generals;
        CServerAgent *agent = findAgent(player);
        if (agent) {
            QVariantList reply = agent->waitForReply(0).toList();
            foreach (const QVariant &choice, reply) {
                uint id = choice.toUInt();
                foreach (const General *general, candidates) {
                    if (general->id() == id) {
                        generals << general;
                        break;
                    }
                }
            }
        }

        //@to-do: handle banned pairs
        if (generals.length() < num)
            generals = candidates.mid(0, num);

        result[player->id()] = generals;
    }

    return result;
}

CAbstractPlayer *GameLogic::createPlayer(CServerAgent *agent)
{
    return new ServerPlayer(this, agent);
}

void GameLogic::loadMode(const GameMode *mode)
{
    setGameRule(mode->rule());

    QList<const EventHandler *> rules = mode->extraRules();
    foreach (const EventHandler *rule, rules)
        addEventHandler(rule);

    Engine *engine = Engine::instance();
    setPackages(engine->getPackages(mode));
}

const RoomSettings *GameLogic::settings() const
{
    return room()->settings<RoomSettings>();
}

void GameLogic::prepareToStart()
{
    CRoom *room = this->room();

    //Load game mode
    Engine *engine = Engine::instance();
    const GameMode *mode = engine->mode(settings()->mode);
    loadMode(mode);

    //Arrange seats for all the players
    QList<ServerPlayer *> players = this->players();
    qShuffle(players);
    for (int i = 1; i < players.length(); i++) {
        players[i - 1]->setSeat(i);
        players[i - 1]->setNext(players.at(i));
    }
    ServerPlayer *lastPlayer = players.last();
    lastPlayer->setSeat(players.length());
    lastPlayer->setNext(players.first());
    setCurrentPlayer(players.first());

    QVariantList playerList;
    foreach (ServerPlayer *player, players) {
        CServerAgent *agent = findAgent(player);
        QVariantMap info;
        if (agent->isHuman()) {
            info["userId"] = agent->id();
        } else {
            info["robotId"] = agent->id();
        }
        info["playerId"] = player->id();
        playerList << info;
    }
    room->broadcastNotification(S_COMMAND_ARRANGE_SEAT, playerList);

    //Import packages
    GeneralList generals;
    foreach (const Package *package, m_packages) {
        generals << package->generals();

        QList<const Card *> cards = package->cards();
        foreach (const Card *card, cards)
            m_cards.insert(card->id(), card->clone());
    }

    //Prepare cards
    QVariantList cardData;
    foreach (const Card *card, m_cards)
        cardData << card->id();
    room->broadcastNotification(S_COMMAND_PREPARE_CARDS, cardData);

    foreach (Card *card, m_cards) {
        m_drawPile->add(card);
        m_cardPosition[card] = m_drawPile;
    }
    qShuffle(m_drawPile->cards());

    m_gameRule->prepareToStart(this);
}

CardArea *GameLogic::findArea(const CardsMoveStruct::Area &area)
{
    if (area.owner) {
        switch (area.type) {
        case CardArea::Hand: {
            ServerPlayer *owner = findPlayer(area.owner->id());
            return owner->handcardArea();
        }
        case CardArea::Equip:
            return area.owner->equipArea();
        case CardArea::DelayedTrick:
            return area.owner->delayedTrickArea();
        case CardArea::Judge:
            return area.owner->judgeCards();
        default: qWarning("Owner Area Not Found");
        }
    } else {
        switch (area.type) {
        case CardArea::DrawPile:
            return m_drawPile;
        case CardArea::DiscardPile:
            return m_discardPile;
        case CardArea::Table:
            return m_table;
        case CardArea::Wugu:
            return m_wugu;
        case CardArea::Unknown:
            return nullptr;
        default: qWarning("Global Area Not Found");
        }
    }
    return nullptr;
}

void GameLogic::filterCardsMove(QList<CardsMoveStruct> &moves)
{
    //Fill card source information
    for (int i = 0, maxi = moves.length(); i < maxi; i++) {
        CardsMoveStruct &move = moves[i];

        CardArea *destination = findArea(move.to);
        foreach (Card *card, move.cards) {
            if (card->isVirtual()) {
                QList<Card *> realCards = card->realCards();
                move.cards.removeOne(card);
                move.cards << realCards;

                if (m_cardPosition.contains(card)) {
                    CardArea *source = m_cardPosition.value(card);
                    if (source) {
                        source->remove(card);

                        QVariantMap data;
                        data["cardName"] = card->metaObject()->className();
                        data["area"] = source->toVariant();
                        data["exists"] = false;
                        room()->broadcastNotification(S_COMMAND_SET_VIRTUAL_CARD, data);
                    }
                    m_cardPosition.remove(card);
                }

                if (destination->add(card)) {
                    m_cardPosition[card] = destination;

                    QVariantMap data;
                    data["cardName"] = card->metaObject()->className();
                    data["area"] = destination->toVariant();
                    data["exists"] = true;
                    room()->broadcastNotification(S_COMMAND_SET_VIRTUAL_CARD, data);
                }
            }
        }

        if (move.from.type != CardArea::Unknown)
            continue;

        QMap<CardArea *, QList<Card *>> cardSource;
        foreach (Card *card, move.cards) {
            CardArea *from = m_cardPosition[card];
            if (from == nullptr)
                continue;
            cardSource[from].append(card);
        }

        QMapIterator<CardArea *, QList<Card *>> iter(cardSource);
        while (iter.hasNext()) {
            iter.next();
            CardArea *from = iter.key();
            CardsMoveStruct submove;
            submove.from.type = from->type();
            submove.from.owner = from->owner();
            submove.from.name = from->name();
            submove.cards = iter.value();
            submove.to = move.to;
            submove.isOpen = move.isOpen;
            moves << submove;
        }

        moves.removeAt(i);
        i--;
        maxi--;
    }
}

void GameLogic::run()
{
    qsrand((uint) QDateTime::currentMSecsSinceEpoch());

    prepareToStart();

    //@to-do: Turn broken event
    QList<ServerPlayer *> allPlayers = this->allPlayers();
    foreach (ServerPlayer *player, allPlayers)
        trigger(GameStart, player);

    forever {
        try {
            ServerPlayer *current = currentPlayer();
            forever {
                if (current->seat() == 1)
                    m_round++;
                if (current->isDead()) {
                    current = current->next();
                    continue;
                }

                setCurrentPlayer(current);
                trigger(TurnStart, current);
                current = current->next();

                while (!m_extraTurns.isEmpty()) {
                    ServerPlayer *extra = m_extraTurns.takeFirst();
                    setCurrentPlayer(extra);
                    trigger(TurnStart, extra);
                }
            }
        } catch (EventType event) {
            if (event == GameFinish) {
                return;
            } else if (event == TurnBroken) {
                ServerPlayer *current = currentPlayer();
                trigger(TurnBroken, current);
                ServerPlayer *next = current->nextAlive(1, false);
                if (current->phase() != Player::Inactive) {
                    QVariant data;
                    m_gameRule->effect(this, PhaseEnd, current, data, current);
                    //@todo:
                    current->setPhase(Player::Inactive);
                    current->broadcastProperty("phase");
                }
                setCurrentPlayer(next);
            }
        }
    }
}
