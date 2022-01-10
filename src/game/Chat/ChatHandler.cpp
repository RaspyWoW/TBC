/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "Log.h"
#include "WorldPacket.h"
#include "Server/WorldSession.h"
#include "World/World.h"
#include "Server/Opcodes.h"
#include "Globals/ObjectMgr.h"
#include "Chat/Chat.h"
#include "Chat/ChannelMgr.h"
#include "Groups/Group.h"
#include "Guilds/Guild.h"
#include "Guilds/GuildMgr.h"
#include "Entities/Player.h"
#include "Spells/SpellAuras.h"
#include "Tools/Language.h"
#include "Util.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"
#include "GMTickets/GMTicketMgr.h"
#include "Anticheat/Anticheat.hpp"

bool WorldSession::ProcessChatMessageAfterSecurityCheck(std::string& msg, const uint32 lang, const uint32 msgType)
{
    if (!IsLanguageAllowedForChatType(lang, msgType))
    {
        return false;
    }

    // Check max length: as of 2.3.x+ no longer disconnects, silently truncates to 255 (wowwiki)
    if (msg.length() > 255)
    {
        utf8limit(msg, 255);
    }

    if (lang != LANG_ADDON)
    {
        // Strip invisible characters for non-addon messages
        if (sWorld.getConfig(CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING))
        {
            stripLineInvisibleChars(msg);
        }

        if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY) && GetSecurity() < SEC_MODERATOR && !ChatHandler(this).CheckEscapeSequences(msg.c_str()))
        {
            sLog.outError("Player %s (GUID: %u) sent a chatmessage with an invalid link: %s", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow(), msg.c_str());

            if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK))
            {
                KickPlayer(true, true);
            }

            return false;
        }
    }

    ChatHandler handler(this);

    return !handler.ParseCommands(msg.c_str());
}

bool WorldSession::IsLanguageAllowedForChatType(const uint32 lang, const uint32 msgType)
{
    // Right now we'll just restrict addon language to the appropriate chat types
    // Anything else is OK (default previous behaviour)
    switch (lang)
    {
        case LANG_ADDON:
        {
            switch (msgType)
            {
            case CHAT_MSG_PARTY:
            case CHAT_MSG_GUILD:
            case CHAT_MSG_OFFICER:
            case CHAT_MSG_RAID:
            case CHAT_MSG_RAID_LEADER:
            case CHAT_MSG_RAID_WARNING:
            case CHAT_MSG_BATTLEGROUND:
            case CHAT_MSG_BATTLEGROUND_LEADER:
            case CHAT_MSG_CHANNEL:
                return true;
            default:
                return false;
            }
        }
        default:
            return true;
    }

    return true;
}

uint32_t WorldSession::ChatCooldown()
{
    if (!GetPlayer())
    {
        return 0;
    }

    auto cooldown = sWorld.getConfig(CONFIG_UINT32_WORLD_CHAN_CD);
    const auto minLevel = sWorld.getConfig(CONFIG_UINT32_WORLD_CHAN_MIN_LEVEL);
    const auto cooldownMaxLvl = sWorld.getConfig(CONFIG_UINT32_WORLD_CHAN_CD_MAX_LEVEL);
    const auto cooldownScaling = sWorld.getConfig(CONFIG_UINT32_WORLD_CHAN_CD_SCALING);
    const auto cooldownUseAcctLvl = sWorld.getConfig(CONFIG_UINT32_WORLD_CHAN_CD_USE_ACCOUNT_MAX_LEVEL);
    const auto playerLevel = cooldownUseAcctLvl ? GetAccountMaxLevel() : GetPlayer()->GetLevel();

    if (cooldown && cooldownMaxLvl > playerLevel)
    {
        const auto currTime = time(nullptr);
        const auto delta = currTime - GetLastPubChanMsgTime();

        if (cooldownScaling)
        {
            auto factor = static_cast<double>((cooldownMaxLvl - playerLevel)) / (cooldownMaxLvl - minLevel);
            cooldown *= factor;
        }

        if (delta < cooldown)
        {
            return cooldown - delta;
        }
    }

    return 0;
}

void WorldSession::HandleMessagechatOpcode(WorldPacket& recv_data)
{
    uint32 type;
    uint32 lang;

    recv_data >> type;
    recv_data >> lang;

    if (type >= MAX_CHAT_MSG_TYPE)
    {
        sLog.outError("CHAT: Wrong message type received: %u", type);
        return;
    }

    DEBUG_LOG("CHAT: packet received. type %u, lang %u", type, lang);

    // Prevent talking at unknown language (cheating)
    LanguageDesc const* langDesc = GetLanguageDescByID(lang);
    if (!langDesc)
    {
        SendNotification(LANG_UNKNOWN_LANGUAGE);
        return;
    }

    if (_player && langDesc->skill_id != 0 && !_player->HasSkill(langDesc->skill_id))
    {
        // Also check SPELL_AURA_COMPREHEND_LANGUAGE (client offers option to speak in that language)
        Unit::AuraList const& langAuras = _player->GetAurasByType(SPELL_AURA_COMPREHEND_LANGUAGE);
        bool foundAura = false;
        for (auto langAura : langAuras)
        {
            if (langAura->GetModifier()->m_miscvalue == int32(lang))
            {
                foundAura = true;
                break;
            }
        }

        if (!foundAura)
        {
            SendNotification(LANG_NOT_LEARNED_LANGUAGE);
            return;
        }
    }

    if (lang == LANG_ADDON)
    {
        // Disabled addon channel?
        if (!sWorld.getConfig(CONFIG_BOOL_ADDON_CHANNEL))
        {
            return;
        }
    }
    // LANG_ADDON should not be changed nor be affected by flood control
    else
    {
        // Send in universal language if player in .gmon mode (ignore spell effects)
        if (_player && _player->IsGameMaster())
        {
            lang = LANG_UNIVERSAL;
        }
        else
        {
            // Send message in universal language if crossfaction chat is enabled and player is using default faction languages.
            if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) && (lang == LANG_COMMON || lang == LANG_ORCISH))
            {
                lang = LANG_UNIVERSAL;
            }
            else
            {
                switch (type)
                {
                    case CHAT_MSG_PARTY:
                    case CHAT_MSG_RAID:
                    case CHAT_MSG_RAID_LEADER:
                    case CHAT_MSG_RAID_WARNING:
                    {
                        // Allow two side chat at group channel if two side group allowed
                        if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP))
                        {
                            lang = LANG_UNIVERSAL;
                        }

                        break;
                    }
                    case CHAT_MSG_GUILD:
                    case CHAT_MSG_OFFICER:
                    {
                        // Allow two side chat at guild channel if two side guild allowed
                        if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD))
                        {
                            lang = LANG_UNIVERSAL;
                        }

                        break;
                    }
                }
            }

            if (_player)
            {
                // But overwrite it by SPELL_AURA_MOD_LANGUAGE auras (only single case used)
                Unit::AuraList const& ModLangAuras = _player->GetAurasByType(SPELL_AURA_MOD_LANGUAGE);
                if (!ModLangAuras.empty())
                {
                    lang = ModLangAuras.front()->GetModifier()->m_miscvalue;
                }
            }
        }

        if (type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
        {
            if (type != CHAT_MSG_WHISPER) // Whisper checked later
            {
                const auto currTime = time(nullptr);

                if (m_muteTime > currTime) // Muted
                {
                    const std::string timeStr = secsToTimeString(m_muteTime - currTime);
                    SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
                    return;
                }
            }

            if (lang != LANG_ADDON && _player)
            {
                _player->UpdateSpeakTime(); // Anti chat flood
            }
        }
    }

    std::string msg, channel, to;
    // Message parsing
    switch (type)
    {
        case CHAT_MSG_CHANNEL:
        {
            recv_data >> channel;
            recv_data >> msg;

            if (!ProcessChatMessageAfterSecurityCheck(msg, lang, type))
            {
                return;
            }

            if (msg.empty())
            {
                return;
            }

            break;
        }
        case CHAT_MSG_WHISPER:
            recv_data >> to;
            // no break
        case CHAT_MSG_SAY:
        case CHAT_MSG_EMOTE:
        case CHAT_MSG_YELL:
        case CHAT_MSG_PARTY:
        case CHAT_MSG_GUILD:
        case CHAT_MSG_OFFICER:
        case CHAT_MSG_RAID:
        case CHAT_MSG_RAID_LEADER:
        case CHAT_MSG_RAID_WARNING:
        case CHAT_MSG_BATTLEGROUND:
        case CHAT_MSG_BATTLEGROUND_LEADER:
        {
            recv_data >> msg;

            if (!ProcessChatMessageAfterSecurityCheck(msg, lang, type))
            {
                return;
            }

            if (msg.empty())
            {
                return;
            }

            break;
        }
        case CHAT_MSG_AFK:
        case CHAT_MSG_DND:
        {
            recv_data >> msg;
            break;
        }
        default:
        {
            sLog.outError("CHAT: unknown message type %u, lang: %u", type, lang);
            return;
        }
    }

    switch (type)
    {
        case CHAT_MSG_CHANNEL:
        {
            if (ChannelMgr* cMgr = channelMgr(_player->GetTeam()))
            {
                if (Channel* chn = cMgr->GetChannel(channel, _player))
                {
                    // Level channels restrictions
                    const auto minChannelLvL = sWorld.getConfig(CONFIG_UINT32_WORLD_CHAN_MIN_LEVEL);
                    if (chn->IsLevelRestricted() && _player->GetLevel() < minChannelLvL && GetAccountMaxLevel() < sWorld.getConfig(CONFIG_UINT32_PUB_CHANS_MUTE_VANISH_LEVEL))
                    {
                        ChatHandler(this).PSendSysMessage("You need to be at least level %u to talk in public channels.", minChannelLvL);
                        return;
                    }

                    // Public channels restrictions
                    if (!chn->HasFlag(Channel::CHANNEL_FLAG_CUSTOM))
                    {
                        // GMs should not be able to use public channels
                        if (GetSecurity() > SEC_PLAYER && GetSecurity() < SEC_ADMINISTRATOR && !sWorld.getConfig(CONFIG_BOOL_GMS_ALLOW_PUBLIC_CHANNELS))
                        {
                            ChatHandler(this).SendSysMessage("Moderators or higher aren't allowed to talk in public channels.");
                            return;
                        }

                        if (const auto cooldown = ChatCooldown())
                        {
                            ChatHandler(this).PSendSysMessage("Please wait %u seconds before sending another message.", cooldown);
                            return;
                        }
                    }

                    chn->Say(_player, msg.c_str(), lang);
                    SetLastPubChanMsgTime(time(nullptr));

                    if (lang != LANG_ADDON && (chn->HasFlag(Channel::ChannelFlags::CHANNEL_FLAG_GENERAL) || chn->IsStatic()))
                    {
                        m_anticheat->Channel(msg);
                    }
                }

                if (lang != LANG_ADDON)
                {
                    normalizePlayerName(channel);
                    sWorld.LogChat(this, "Channel", msg, 0, channel.c_str());
                }
            }

            break;
        }
        case CHAT_MSG_SAY:
        {
            const uint32 minSayLvL = sWorld.getConfig(CONFIG_UINT32_SAY_MIN_LEVEL);
            if (GetPlayer()->GetLevel() < minSayLvL && GetAccountMaxLevel() < sWorld.getConfig(CONFIG_UINT32_PUB_CHANS_MUTE_VANISH_LEVEL))
            {
                ChatHandler(this).PSendSysMessage("You need to be at least level %u to say something.", minSayLvL);
                return;
            }

            if (!GetPlayer()->IsAlive())
            {
                return;
            }

            GetPlayer()->Say(msg, lang);

            if (lang != LANG_ADDON && !m_anticheat->IsSilenced())
            {
                m_anticheat->Say(msg);
                sWorld.LogChat(this, "Say", msg);
            }

            break;
        }
        case CHAT_MSG_EMOTE:
        {
            const uint32 minEmoteLvL = sWorld.getConfig(CONFIG_UINT32_SAY_EMOTE_MIN_LEVEL);
            if (GetPlayer()->GetLevel() < minEmoteLvL && GetAccountMaxLevel() < sWorld.getConfig(CONFIG_UINT32_PUB_CHANS_MUTE_VANISH_LEVEL))
            {
                ChatHandler(this).PSendSysMessage("You need to be at least level %u to use emotes.", minEmoteLvL);
                return;
            }

            if (!GetPlayer()->IsAlive())
            {
                return;
            }

            GetPlayer()->TextEmote(msg);

            if (lang != LANG_ADDON && !m_anticheat->IsSilenced())
            {
                m_anticheat->Say(msg);
                sWorld.LogChat(this, "Emote", msg);
            }

            break;
        }
        case CHAT_MSG_YELL:
        {
            const uint32 minYellLvL = sWorld.getConfig(CONFIG_UINT32_YELL_MIN_LEVEL);
            if (GetPlayer()->GetLevel() < minYellLvL && GetAccountMaxLevel() < sWorld.getConfig(CONFIG_UINT32_PUB_CHANS_MUTE_VANISH_LEVEL))
            {
                ChatHandler(this).PSendSysMessage("You need to be at least level %u to yell.", minYellLvL);
                return;
            }

            if (!GetPlayer()->IsAlive())
            {
                return;
            }

            GetPlayer()->Yell(msg, lang);

            if (lang != LANG_ADDON && !m_anticheat->IsSilenced())
            {
                m_anticheat->Say(msg);
                sWorld.LogChat(this, "Yell", msg);
            }

            break;
        }
        case CHAT_MSG_WHISPER:
        {
            if (!normalizePlayerName(to))
            {
                SendPlayerNotFoundNotice(to);
                break;
            }

            Player* player = sObjectMgr.GetPlayer(to.c_str());

            std::pair<bool, bool> hookresult = sTicketMgr.HookGMTicketWhisper(GetPlayer(), to, player, msg, (lang == LANG_ADDON));

            if (hookresult.first)
            {
                if (!hookresult.second)
                    SendPlayerNotFoundNotice(to);

                return;
            }

            uint32 tSecurity = GetSecurity();
            uint32 pSecurity = player ? player->GetSession()->GetSecurity() : SEC_PLAYER;
            if (!player || (tSecurity == SEC_PLAYER && pSecurity > SEC_PLAYER && !_player->isAcceptWhispers()))
            {
                SendPlayerNotFoundNotice(to);
                return;
            }

            // Can only whisper GMs while muted.
            if (pSecurity == SEC_PLAYER)
            {
                const auto currTime = time(nullptr);

                if (m_muteTime > currTime) // Muted
                {
                    std::string timeStr = secsToTimeString(m_muteTime - currTime);
                    SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
                    return;
                }
            }

            if (tSecurity == SEC_PLAYER && pSecurity == SEC_PLAYER)
            {
                if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) && GetPlayer()->GetTeam() != player->GetTeam())
                {
                    SendWrongFactionNotice();
                    return;
                }

                const uint32 whisperMinLvL = sWorld.getConfig(CONFIG_UINT32_WHISP_DIFF_ZONE_MIN_LEVEL);
                if (GetPlayer()->GetZoneId() != _player->GetZoneId() && _player->GetLevel() < whisperMinLvL)
                {
                    ChatHandler(this).PSendSysMessage("You need to be at least level %u to whisper to players in different zones.", whisperMinLvL);
                    return;
                }

                const uint32 maxLevel = _player->GetSession()->GetAccountMaxLevel();
                auto& whisper_targets = _player->GetSession()->GetWhisperTargets();
                Player* toPlayer = player->GetSession()->GetPlayer();

                if (toPlayer && !whisper_targets.CanWhisper(toPlayer->GetObjectGuid(), maxLevel))
                {
                    ChatHandler(this).SendSysMessage("You have whispered too many different players too quickly.");
                    ChatHandler(this).SendSysMessage("Please wait a while before whispering any additional players.");
                    return;
                }
            }

            if (Player* toPlayer = player->GetSession()->GetPlayer())
            {
                const bool allowIgnoreAntispam = toPlayer->IsAllowedWhisperFrom(_player->GetObjectGuid());
                bool allowSendWhisper = allowIgnoreAntispam;

                if (!sWorld.getConfig(CONFIG_BOOL_WHISPER_RESTRICTION) || !toPlayer->IsEnabledWhisperRestriction())
                {
                    allowSendWhisper = true;
                }

                if (toPlayer->IsGameMaster() || allowSendWhisper)
                {
                    _player->Whisper(msg, lang, player->GetObjectGuid());
                }
                else
                {
                    ChatHandler(this).SendSysMessage("Your whisper target don't want to receive any messages at this time.");
                }

                if (lang != LANG_ADDON && !m_anticheat->IsSilenced())
                {
                    sWorld.LogChat(this, "Whisp", msg, player->GetObjectGuid());

                    if (!allowIgnoreAntispam)
                    {
                        m_anticheat->Whisper(msg, player->GetObjectGuid());
                    }
                }
            }

            break;
        }
        case CHAT_MSG_PARTY:
        {
            // If player is in battleground, party chat is sent only to members of normal group
            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = _player->GetGroup();

            if (!group)
            {
                return;
            }

            if (group->IsBattleGroup())
            {
                group = _player->GetOriginalGroup();
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, ChatMsg(type), msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(data, false, group->GetMemberGroup(GetPlayer()->GetObjectGuid()));

            if (lang != LANG_ADDON)
            {
                sWorld.LogChat(this, "Group", msg, group->GetId());
            }

            break;
        }
        case CHAT_MSG_GUILD:
        {
            if (GetPlayer()->GetGuildId())
            {
                if (Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId()))
                {
                    guild->BroadcastToGuild(this, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                }
            }

            if (lang != LANG_ADDON)
            {
                sWorld.LogChat(this, "Guild", msg, _player->GetGuildId());
            }

            break;
        }
        case CHAT_MSG_OFFICER:
        {
            if (GetPlayer()->GetGuildId())
            {
                if (Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId()))
                {
                    guild->BroadcastToOfficers(this, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                }
            }

            if (lang != LANG_ADDON)
            {
                sWorld.LogChat(this, "Officer", msg, _player->GetGuildId());
            }

            break;
        }
        case CHAT_MSG_RAID:
        {
            // If player is in battleground, raid chat is sent only to members of normal group
            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = _player->GetGroup();

            if (!group)
            {
                return;
            }

            if (group->IsBattleGroup())
            {
                group = _player->GetOriginalGroup();
            }

            if (!group->IsRaidGroup())
            {
                return;
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(data, false);

            if (lang != LANG_ADDON)
            {
                sWorld.LogChat(this, "Raid", msg, group->GetId());
            }

            break;
        }
        case CHAT_MSG_RAID_LEADER:
        {
            // If player is in battleground, raid chat is sent only to members of normal group
            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = _player->GetGroup();

            if (!group)
            {
                return;
            }

            if (group->IsBattleGroup())
            {
                group = _player->GetOriginalGroup();
            }

            if (!group->IsRaidGroup() || !group->IsLeader(_player->GetObjectGuid()))
            {
                return;
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_LEADER, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(data, false);

            if (lang != LANG_ADDON)
            {
                sWorld.LogChat(this, "Raid", msg, group->GetId());
            }

            break;
        }
        case CHAT_MSG_RAID_WARNING:
        {
            // If player is in battleground, raid warning is sent only to players in battleground
            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = _player->GetGroup();

            if (!group)
            {
                return;
            }
            else if (group->IsRaidGroup())
            {
                if (!group->IsLeader(_player->GetObjectGuid()) && !group->IsAssistant(_player->GetObjectGuid()))
                {
                    return;
                }
            }
            else if (sWorld.getConfig(CONFIG_BOOL_CHAT_RESTRICTED_RAID_WARNINGS))
            {
                return;
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_WARNING, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(data, true);

            if (lang != LANG_ADDON)
            {
                sWorld.LogChat(this, "Raid", msg, group->GetId());
            }

            break;
        }
        case CHAT_MSG_BATTLEGROUND:
        {
            // Battleground chat is sent only to players in battleground
            // Battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = _player->GetGroup();

            if (!group || !group->IsBattleGroup())
            {
                return;
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_BATTLEGROUND, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(data, false);

            if (lang != LANG_ADDON)
            {
                sWorld.LogChat(this, "BG", msg, group->GetId());
            }

            break;
        }
        case CHAT_MSG_BATTLEGROUND_LEADER:
        {
            // Battleground chat is sent only to players in battleground
            // Battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = _player->GetGroup();
            if (!group || !group->IsBattleGroup() || !group->IsLeader(_player->GetObjectGuid()))
            {
                return;
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_BATTLEGROUND_LEADER, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(data, false);

            if (lang != LANG_ADDON)
            {
                sWorld.LogChat(this, "BG", msg, group->GetId());
            }

            break;
        }
        case CHAT_MSG_AFK:
        {
            if (_player && _player->IsInCombat())
            {
                break;
            }

            if (msg.empty() || !_player->isAFK())
            {
                _player->ToggleAFK();

                if (_player->isAFK() && _player->isDND())
                {
                    _player->ToggleDND();
                }
            }

            break;
        }
        case CHAT_MSG_DND:
        {
            if (msg.empty() || !_player->isDND())
            {
                _player->ToggleDND();

                if (_player->isDND() && _player->isAFK())
                {
                    _player->ToggleAFK();
                }
            }

            break;
        }
    }
}

void WorldSession::HandleEmoteOpcode(WorldPacket& recv_data)
{
    if (!GetPlayer()->IsAlive() || GetPlayer()->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PREVENT_ANIM))
    {
        return;
    }

    if (!GetPlayer()->CanSpeak())
    {
        std::string timeStr = secsToTimeString(m_muteTime - time(nullptr));
        SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
        return;
    }

    uint32 emote;
    recv_data >> emote;

    // Restrict to the only emotes hardcoded in client
    if (emote != EMOTE_ONESHOT_NONE && emote != EMOTE_ONESHOT_WAVE)
    {
        return;
    }

    GetPlayer()->InterruptSpellsAndAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ANIM_CANCELS);
    GetPlayer()->HandleEmoteCommand(emote);
}

namespace MaNGOS
{
    class EmoteChatBuilder
    {
        public:
            EmoteChatBuilder(Player const& pl, uint32 text_emote, uint32 emote_num, Unit const* target)
                : i_player(pl), i_text_emote(text_emote), i_emote_num(emote_num), i_target(target) {}

            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* nam = i_target ? i_target->GetNameForLocaleIdx(loc_idx) : nullptr;
                const auto namlen = (nam ? strlen(nam) : 0) + 1;

                data.Initialize(SMSG_TEXT_EMOTE, (20 + namlen));
                data << ObjectGuid(i_player.GetObjectGuid());
                data << uint32(i_text_emote);
                data << uint32(i_emote_num);
                data << uint32(namlen);

                if (namlen > 1)
                {
                    data.append(nam, namlen);
                }
                else
                {
                    data << uint8(0x00);
                }
            }

        private:
            Player const& i_player;
            uint32 i_text_emote;
            uint32 i_emote_num;
            Unit const* i_target;
    };
}

void WorldSession::HandleTextEmoteOpcode(WorldPacket& recv_data)
{
    if (!GetPlayer()->IsAlive() || GetPlayer()->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PREVENT_ANIM))
    {
        return;
    }

    if (!GetPlayer()->CanSpeak())
    {
        std::string timeStr = secsToTimeString(m_muteTime - time(nullptr));
        SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
        return;
    }

    uint32 text_emote, emoteNum;
    ObjectGuid guid;

    recv_data >> text_emote;
    recv_data >> emoteNum;
    recv_data >> guid;

    EmotesTextEntry const* em = sEmotesTextStore.LookupEntry(text_emote);
    if (!em)
    {
        return;
    }

    const uint32 emote_id = em->textid;

    switch (emote_id)
    {
        case EMOTE_STATE_SLEEP:
        case EMOTE_STATE_SIT:
        case EMOTE_STATE_KNEEL:
        case EMOTE_ONESHOT_NONE:
            break;
        default:
        {
            GetPlayer()->InterruptSpellsAndAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ANIM_CANCELS);
            GetPlayer()->HandleEmoteCommand(emote_id);
            break;
        }
    }

    Unit* unit = GetPlayer()->GetMap()->GetUnit(guid);

    MaNGOS::EmoteChatBuilder emote_builder(*GetPlayer(), text_emote, emoteNum, unit);
    MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > emote_do(emote_builder);
    MaNGOS::CameraDistWorker<MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > > emote_worker(GetPlayer(), sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE), emote_do);
    Cell::VisitWorldObjects(GetPlayer(), emote_worker, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE));

    // Send scripted event call
    if (unit && unit->AI())
    {
        unit->AI()->ReceiveEmote(GetPlayer(), text_emote);
    }
}

void WorldSession::HandleChatIgnoredOpcode(WorldPacket& recv_data)
{
    ObjectGuid iguid;
    uint8 unk;

    recv_data >> iguid;
    recv_data >> unk; // Probably related to spam reporting

    Player* player = sObjectMgr.GetPlayer(iguid);
    if (!player || !player->GetSession())
    {
        return;
    }

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_IGNORED, _player->GetName(), LANG_UNIVERSAL, CHAT_TAG_NONE, _player->GetObjectGuid());
    player->GetSession()->SendPacket(data);
}

void WorldSession::SendPlayerNotFoundNotice(const std::string& name) const
{
    WorldPacket data(SMSG_CHAT_PLAYER_NOT_FOUND, name.size() + 1);
    data << name;
    SendPacket(data);
}

void WorldSession::SendWrongFactionNotice() const
{
    WorldPacket data(SMSG_CHAT_WRONG_FACTION, 0);
    SendPacket(data);
}

void WorldSession::SendChatRestrictedNotice(ChatRestrictionType restriction) const
{
    WorldPacket data(SMSG_CHAT_RESTRICTED, 1);
    data << uint8(restriction);
    SendPacket(data);
}
