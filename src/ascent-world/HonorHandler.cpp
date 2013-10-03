/*
 * Ascent MMORPG Server
 * Copyright (C) 2005-2008 Ascent Team <http://www.ascentemu.com/>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "StdAfx.h"

void WorldSession::HandleSetVisibleRankOpcode(WorldPacket& recv_data)
{
	CHECK_PACKET_SIZE(recv_data, 4);
	uint32 ChosenRank;
	recv_data >> ChosenRank; 
	if(ChosenRank == 0xFFFFFFFF)
	{
		_player->SetUInt32Value(PLAYER_CHOSEN_TITLE, 0);
		_player->SetPVPRank(0);
	} else if(_player->HasKnownTitle(ChosenRank)) // Check for cheating
	{
		_player->SetUInt32Value(PLAYER_CHOSEN_TITLE, ChosenRank);
		if(ChosenRank <= TITLE_HIGH_WARLORD)
		{
			if(ChosenRank > TITLE_GRAND_MARSHAL)
				ChosenRank -= TITLE_GRAND_MARSHAL;
			_player->SetPVPRank(ChosenRank);
		}
	}
}

void HonorHandler::AddHonorPointsToPlayer(Player *pPlayer, uint32 uAmount)
{
	if( pPlayer->GetMapId() == 559 || pPlayer->GetMapId() == 562 || pPlayer->GetMapId() == 572)
		return;
	pPlayer->m_honorPoints += uAmount;
	pPlayer->m_honorToday += uAmount;
	
	pPlayer->HandleProc(PROC_ON_GAIN_EXPIERIENCE, pPlayer, NULL);
	pPlayer->m_procCounter = 0;

	RecalculateHonorFields(pPlayer);
}

int32 HonorHandler::CalculateHonorPointsForKill( uint32 playerLevel, uint32 victimLevel )
{

	uint32 k_level = playerLevel;
	uint32 v_level = victimLevel;

	uint32 k_grey = 0;

	if( k_level > 5 && k_level < 40 )
	{
		k_grey = k_level - 5 - float2int32( floor( ((float)k_level) / 10.0f ) );
	}
	else
	{
		k_grey = k_level - 1 - float2int32( floor( ((float)k_level) / 5.0f ) );
	}

	if(v_level <= k_grey)
		return 0;

	// Correct formula unknown. This one is correct for lvl 70 killing lvl 70 and scales down for lower levels
	uint32 diff_level = v_level - k_level; // Should somehow affect the result

	float honor_points = 20.9f;
	honor_points *= ((float)k_level) / 70.0f;
	honor_points *= World::getSingleton().getRate( RATE_HONOR );
	if(honor_points < 1.0f) // Make sure we get at least 1 point on low levels
		return 1;
	return float2int32( honor_points );
}

void HonorHandler::OnPlayerKilledUnit( Player *pPlayer, Unit* pVictim )
{
	if( pVictim == NULL || pPlayer == NULL )
		return;

	if( pPlayer->GetTypeId() != TYPEID_PLAYER || !pVictim->IsUnit() )
		return;

	if( !pVictim->IsPlayer() || static_cast< Player* >( pVictim )->m_honorless )
		return;

    if( pVictim->IsPlayer() )
	{
		if( pPlayer->m_bg )
		{
			if( static_cast< Player* >( pVictim )->m_bgTeam == pPlayer->m_bgTeam )
				return;

			// patch 2.4, players killed >50 times in battlegrounds won't be worth honor for the rest of that bg
			if( static_cast<Player*>(pVictim)->m_bgScore.Deaths >= 50 )
				return;
		}
		else
		{
			if( pPlayer->GetTeam() == static_cast< Player* >( pVictim )->GetTeam() )
				return;
		}
	}

	// Calculate points
	int32 Points = 0;
	if(pVictim->IsPlayer() && pPlayer != pVictim)
		Points = CalculateHonorPointsForKill(pPlayer->GetUInt32Value( UNIT_FIELD_LEVEL ), pVictim->GetUInt32Value( UNIT_FIELD_LEVEL ));

	if( Points > 0 )
	{
		uint32 victimPvPRank = 0;
		if(pVictim)
			victimPvPRank = uint32(static_cast< Player* >(pVictim)->GetPVPRank());
		if(victimPvPRank)
			victimPvPRank += 4;

		if( pPlayer->m_bg )
		{
			// hackfix for battlegrounds (since the gorups there are disabled, we need to do this manually)
			vector<Player*> toadd;
			uint32 t = pPlayer->m_bgTeam;
			toadd.reserve(15);		// shouldnt have more than this
			pPlayer->m_bg->Lock();
			set<Player*> * s = &pPlayer->m_bg->m_players[t];

			for(set<Player*>::iterator itr = s->begin(); itr != s->end(); ++itr)
			{
				if((*itr) == pPlayer || (*itr)->isInRange(pPlayer,100.0f))
					toadd.push_back(*itr);
			}

			if( toadd.size() > 0 )
			{
				uint32 pts = Points / (uint32)toadd.size();
				for(vector<Player*>::iterator vtr = toadd.begin(); vtr != toadd.end(); ++vtr)
				{
					AddHonorPointsToPlayer(*vtr, pts);

					(*vtr)->m_killsToday++;
					(*vtr)->m_killsLifetime++;
					pPlayer->m_bg->HookOnHK(*vtr);
					if(pVictim)
					{
						// Send PVP credit
						WorldPacket data(SMSG_PVP_CREDIT, 12);
						data << pts << pVictim->GetGUID() << victimPvPRank;
						(*vtr)->GetSession()->SendPacket(&data);
					}
				}
			}

			pPlayer->m_bg->Unlock();
		}
		else if(pPlayer->GetGroup())
        {
            Group *pGroup = pPlayer->GetGroup();
            Player *gPlayer = NULL;
            int32 GroupPoints;
			pGroup->Lock();
            GroupPoints = (Points / (pGroup->MemberCount() ? pGroup->MemberCount() : 1));
            GroupMembersSet::iterator gitr;
			for(uint32 k = 0; k < pGroup->GetSubGroupCount(); k++)
			{
				for(gitr = pGroup->GetSubGroup(k)->GetGroupMembersBegin(); gitr != pGroup->GetSubGroup(k)->GetGroupMembersEnd(); ++gitr)
				{
					gPlayer = (*gitr)->m_loggedInPlayer;
                    
                    if(gPlayer && (gPlayer == pPlayer || gPlayer->isInRange(pPlayer,100.0f))) // visible range
                    {
                        gPlayer->m_killsToday++;
                        gPlayer->m_killsLifetime++;
						if(gPlayer->m_bg)
							gPlayer->m_bg->HookOnHK(gPlayer);

		                AddHonorPointsToPlayer(gPlayer, GroupPoints);
						if(pVictim->IsPlayer())
							sHookInterface.OnHonorableKill(gPlayer, (Player*)pVictim);
                        if(pVictim)
		                {
			                // Send PVP credit
			                WorldPacket data(SMSG_PVP_CREDIT, 12);
			                data << GroupPoints << pVictim->GetGUID() << victimPvPRank;
			                gPlayer->GetSession()->SendPacket(&data);
		                }
						//patch by emsy
                        // If we are in Halaa
                        if(pPlayer->GetZoneId() == 3518)
                        {
                            // Add Halaa Battle Token
                            SpellEntry * pvp_token_spell = dbcSpell.LookupEntry(gPlayer->GetTeam()? 33004 : 33005);
                            gPlayer->CastSpell(gPlayer, pvp_token_spell, true);
                        }
                        // If we are in Hellfire Peninsula
                        if(pPlayer->GetZoneId() == 3483)
                        {
                            // Add Mark of Thrallmar/Honor Hold
                            SpellEntry * pvp_token_spell = dbcSpell.LookupEntry(gPlayer->GetTeam()? 32158 : 32155);
                            gPlayer->CastSpell(gPlayer, pvp_token_spell, true);
                        }
                    }

                }
            }
			pGroup->Unlock();
        
        }
        else
        {
		    pPlayer->m_killsToday++;
		    pPlayer->m_killsLifetime++;
		    AddHonorPointsToPlayer(pPlayer, Points);
    		
			if(pPlayer->m_bg)
				pPlayer->m_bg->HookOnHK(pPlayer);
			
			if(pVictim->IsPlayer())
				sHookInterface.OnHonorableKill(pPlayer, (Player*)pVictim);

		    if(pVictim)
		    {
			    // Send PVP credit
			    WorldPacket data(SMSG_PVP_CREDIT, 12);
			    data << Points << pVictim->GetGUID() << victimPvPRank;
			    pPlayer->GetSession()->SendPacket(&data);
		    }
			//patch by emsy
            // If we are in Halaa
            if(pPlayer->GetZoneId() == 3518)
            {
                // Add Halaa Battle Token
                SpellEntry * halaa_spell = dbcSpell.LookupEntry(pPlayer->GetTeam()? 33004 : 33005);
                pPlayer->CastSpell(pPlayer, halaa_spell, true);
            }
        }
	}
}

void HonorHandler::RecalculateHonorFields(Player *pPlayer)
{
	pPlayer->SetUInt32Value(PLAYER_FIELD_KILLS, pPlayer->m_killsToday | ( pPlayer->m_killsYesterday << 16));
	pPlayer->SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, pPlayer->m_honorToday);
	pPlayer->SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, pPlayer->m_honorYesterday);
	pPlayer->SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORBALE_KILLS, pPlayer->m_killsLifetime);
	pPlayer->SetUInt32Value(PLAYER_FIELD_HONOR_CURRENCY, pPlayer->m_honorPoints);
	pPlayer->SetUInt32Value(PLAYER_FIELD_ARENA_CURRENCY, pPlayer->m_arenaPoints);
}

bool ChatHandler::HandleAddKillCommand(const char* args, WorldSession* m_session)
{
	uint32 KillAmount = args ? atol(args) : 1;
	Player *plr = getSelectedChar(m_session, true);
	if(plr == 0)
		return true;

	BlueSystemMessage(m_session, "Adding %u kills to player %s.", KillAmount, plr->GetName());
	GreenSystemMessage(plr->GetSession(), "You have had %u honor kills added to your character.", KillAmount);

	for(uint32 i = 0; i < KillAmount; ++i)
		HonorHandler::OnPlayerKilledUnit(plr, 0);

	return true;
}

bool ChatHandler::HandleAddHonorCommand(const char* args, WorldSession* m_session)
{
	uint32 HonorAmount = args ? atol(args) : 1;
	Player *plr = getSelectedChar(m_session, true);
	if(plr == 0)
		return true;

	BlueSystemMessage(m_session, "Adding %u honor to player %s.", HonorAmount, plr->GetName());
	GreenSystemMessage(plr->GetSession(), "You have had %u honor points added to your character.", HonorAmount);

	HonorHandler::AddHonorPointsToPlayer(plr, HonorAmount);
	return true;
}

bool ChatHandler::HandlePVPCreditCommand(const char* args, WorldSession* m_session)
{
	uint32 Rank, Points;
	if(sscanf(args, "%u %u", (unsigned int*)&Rank, (unsigned int*)&Points) != 2)
	{
		RedSystemMessage(m_session, "Command must be in format <rank> <points>.");
		return true;
	}
	Points *= 10;
	uint64 Guid = m_session->GetPlayer()->GetSelection();
	if(Guid == 0)
	{
		RedSystemMessage(m_session, "A selection of a unit or player is required.");
		return true;
	}

	BlueSystemMessage(m_session, "Building packet with Rank %u, Points %u, GUID "I64FMT".", 
		Rank, Points, Guid);

	WorldPacket data(SMSG_PVP_CREDIT, 12);
	data << Points << Guid << Rank;
	m_session->SendPacket(&data);
	return true;
}

bool ChatHandler::HandleGlobalHonorDailyMaintenanceCommand(const char* args, WorldSession* m_session)
{
	return false;
}

bool ChatHandler::HandleNextDayCommand(const char* args, WorldSession* m_session)
{
	return false;
}
