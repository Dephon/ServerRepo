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

#ifdef WIN32
#define HACKY_CRASH_FIXES 1		// SEH stuff
#endif

bool isHostile(Object* objA, Object* objB)// B is hostile for A?
{
	if(!objA || !objB)
		return false;
	bool hostile = false;

	if(objB->m_faction == NULL || objA->m_faction == NULL)
		return true;

	if(objA == objB)
		return false;   // can't attack self.. this causes problems with buffs if we dont have it :p

	if(objA->GetTypeId() == TYPEID_CORPSE)
		return false;

	if(objB->GetTypeId() == TYPEID_CORPSE)
		return false;

	uint32 faction = objB->m_faction->Mask;
	uint32 host = objA->m_faction->HostileMask;

	/*if( faction != 0 )
	{*/
		if(faction & host)
		{
			hostile = true;
		}
	/*}
	else
	{
		// default to true
		hostile = true;
	}*/

	// check friend/enemy list
	for(uint32 i = 0; i < 4; i++)
	{
		if(objA->m_faction->EnemyFactions[i] == objB->m_faction->Faction)
		{
			hostile = true;
			break;
		}
		if(objA->m_faction->FriendlyFactions[i] == objB->m_faction->Faction)
		{
			hostile = false;
			break;
		}
	}

	// PvP Flag System Checks
	// We check this after the normal isHostile test, that way if we're
	// on the opposite team we'll already know :p

	if( hostile && ( objA->IsPlayer() || objA->IsPet() || ( objA->IsUnit() && !objA->IsPlayer() && static_cast< Creature* >( objA )->IsTotem() && static_cast< Creature* >( objA )->GetTotemOwner()->IsPvPFlagged() ) ) )
	{
		if( objB->IsPlayer() )
		{
			// Check PvP Flags.
			if( static_cast< Player* >( objB )->IsPvPFlagged() )
				return true;
			else
				return false;
		}
		if( objB->IsPet() )
		{
#if defined(WIN32) && defined(HACKY_CRASH_FIXES)
			__try {
				// Check PvP Flags.
				if( static_cast< Pet* >( objB )->GetPetOwner() != NULL && static_cast< Pet* >( objB )->GetPetOwner()->GetMapMgr() == objB->GetMapMgr() && static_cast< Pet* >( objB )->GetPetOwner()->IsPvPFlagged() )
					return true;
				else
					return false;
			} __except(EXCEPTION_EXECUTE_HANDLER)
			{
				static_cast<Pet*>(objB)->ClearPetOwner();
				static_cast<Pet*>(objB)->SafeDelete();
			}
#else
			// Check PvP Flags.
			if( static_cast< Pet* >( objB )->GetPetOwner() != NULL && static_cast< Pet* >( objB )->GetPetOwner()->GetMapMgr() == objB->GetMapMgr() && static_cast< Pet* >( objB )->GetPetOwner()->IsPvPFlagged() )
				return true;
			else
				return false;
#endif
		}
	}

	// Reputation System Checks
	if(objA->IsPlayer() && !objB->IsPlayer())	   // PvE
	{
		if(objB->m_factionDBC->RepListId >= 0)
			hostile = static_cast< Player* >( objA )->IsHostileBasedOnReputation( objB->m_factionDBC );
	}
	
	if(objB->IsPlayer() && !objA->IsPlayer())	   // PvE
	{
		if(objA->m_factionDBC->RepListId >= 0)
			hostile = static_cast< Player* >( objB )->IsHostileBasedOnReputation( objA->m_factionDBC );
	}

	if( objA->IsPlayer() && objB->IsPlayer() && static_cast<Player*>(objA)->m_bg != NULL )
	{
		if( static_cast<Player*>(objA)->m_bgTeam != static_cast<Player*>(objB)->m_bgTeam )
			return true;
	}

	return hostile;
}

/// Where we check if we object A can attack object B. This is used in many feature's
/// Including the spell class and the player class.
bool isAttackable(Object* objA, Object* objB, bool CheckStealth)// A can attack B?
{
	if(!objA || !objB || objB->m_factionDBC == NULL || objA->m_factionDBC == NULL)
		return false;

	if(objB->m_faction == NULL || objA->m_faction == NULL )
		return true;

	if(objA == objB)
		return false;   // can't attack self.. this causes problems with buffs if we don't have it :p

	if(objA->GetTypeId() == TYPEID_CORPSE)
		return false;

	if(objB->GetTypeId() == TYPEID_CORPSE)
		return false;

	// Checks for untouchable, unattackable
	if(objA->IsUnit() && objA->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_9 | UNIT_FLAG_MOUNTED_TAXI | UNIT_FLAG_NOT_SELECTABLE))
		return false;
	if(objB->IsUnit())
	{
		if(objB->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_9 | UNIT_FLAG_MOUNTED_TAXI | UNIT_FLAG_NOT_SELECTABLE))
			return false;

		/// added by Zack : 
        /// we cannot attack sheathed units. Maybe checked in other places too ?
		/// !! warning, this presumes that objA is attacking ObjB
        /// Capt: Added the possibility to disregard this (regarding the spell class)
		if(static_cast<Unit *>(objB)->IsStealth() && CheckStealth)
			return false;
	}

	if(objA->IsPlayer() && objB->IsPlayer())
	{
		if(
			static_cast< Player* >( objA )->DuelingWith == static_cast< Player* >(objB) && 
			static_cast< Player* >( objA )->GetDuelState() == DUEL_STATE_STARTED
			)
		return true;

		if(objA->HasFlag(PLAYER_FLAGS,PLAYER_FLAG_FREE_FOR_ALL_PVP) && objB->HasFlag(PLAYER_FLAGS,PLAYER_FLAG_FREE_FOR_ALL_PVP))
		{
			if( static_cast< Player* >( objA )->m_bg != NULL )
				if( static_cast< Player* >( objA )->GetGroup() == static_cast< Player* >( objB )->GetGroup() )
					return false;

			return true;		// can hurt each other in FFA pvp
		}
	}
	
	// handle for pets in duel
	if(objA->IsPet())
	{
		if(objB->IsPlayer())
			if(
				static_cast<Pet *>(objA)->GetPetOwner() &&
				static_cast<Pet *>(objA)->GetPetOwner()->DuelingWith == static_cast< Player* >(objB) && 
				static_cast<Pet *>(objA)->GetPetOwner()->GetDuelState() == DUEL_STATE_STARTED
				)
				return true;
		if(objB->IsPet())
			if(static_cast<Pet *>(objA)->GetPetOwner() &&
				static_cast<Pet *>(objB)->GetPetOwner() &&
				static_cast<Pet *>(objA)->GetPetOwner()->DuelingWith == static_cast<Pet *>(objB)->GetPetOwner() && 
				static_cast<Pet *>(objA)->GetPetOwner()->GetDuelState() == DUEL_STATE_STARTED
				)
				return true;
	}
	if(objB->IsPet())
	{
		if(objA->IsPlayer())
			if(
				static_cast<Pet*>(objB)->GetPetOwner() && static_cast<Pet *>(objB)->GetPetOwner() &&
				static_cast<Pet *>(objB)->GetPetOwner()->DuelingWith == static_cast< Player* >(objA) && 
				static_cast<Pet *>(objB)->GetPetOwner()->GetDuelState() == DUEL_STATE_STARTED
				)
				return true;
		else if(objA->IsPet())
			if(static_cast<Pet*>(objA)->GetPetOwner() && static_cast<Pet *>(objB)->GetPetOwner() &&
				static_cast<Pet*>(objB)->GetPetOwner() &&
				static_cast<Pet *>(objB)->GetPetOwner()->DuelingWith == static_cast<Pet *>(objA)->GetPetOwner() && 
				static_cast<Pet *>(objB)->GetPetOwner()->GetDuelState() == DUEL_STATE_STARTED
				)
				return true;
	}

	// handle for totems
	if(objA->IsUnit() && !objA->IsPlayer()) // must be creature
	{
		if(static_cast<Creature *>(objA)->IsTotem())
		{
			if(objB->IsPlayer())
				if( static_cast<Creature *>(objA)->GetTotemOwner() &&
					static_cast<Creature *>(objA)->GetTotemOwner()->DuelingWith == static_cast< Player* >(objB) && 
					static_cast<Creature *>(objA)->GetTotemOwner()->GetDuelState() == DUEL_STATE_STARTED
					)
					return true;
			if(objB->IsPet())
				if( static_cast<Creature *>(objA)->GetTotemOwner() &&
					static_cast<Creature *>(objA)->GetTotemOwner()->DuelingWith == static_cast<Pet *>(objB)->GetPetOwner() && 
					static_cast<Creature *>(objA)->GetTotemOwner()->GetDuelState() == DUEL_STATE_STARTED
					)
					return true;
		}
	}
	if(objB->IsUnit() && !objB->IsPlayer()) // must be creature
	{
		if(static_cast<Creature *>(objB)->IsTotem())
		{
			if(objA->IsPlayer())
				if( static_cast<Creature *>(objB)->GetTotemOwner() &&
					static_cast<Creature *>(objB)->GetTotemOwner()->DuelingWith == static_cast< Player* >(objA) && 
					static_cast<Creature *>(objB)->GetTotemOwner()->GetDuelState() == DUEL_STATE_STARTED
					)
					return true;
			if(objA->IsPet())
				if( static_cast<Creature *>(objB)->GetTotemOwner() &&
					static_cast<Creature *>(objB)->GetTotemOwner()->DuelingWith == static_cast<Pet *>(objA)->GetPetOwner() && 
					static_cast<Creature *>(objB)->GetTotemOwner()->GetDuelState() == DUEL_STATE_STARTED
					)
					return true;
		}
	}

	// do not let people attack each other in sanctuary
	// Dueling is already catered for
	AreaTable *atA;
	AreaTable *atB;
	if( objA->IsPet() && static_cast< Pet* >( objA )->GetPetOwner() )
		atA = dbcArea.LookupEntry( static_cast< Pet* >( objA )->GetPetOwner()->GetAreaID() );
	else if( objA->IsPlayer() )
		atA = dbcArea.LookupEntry( static_cast< Player* >( objA )->GetAreaID() );
	else if( objA->IsCreature() && static_cast< Creature* >( objA )->IsTotem() &&
			static_cast< Creature* >( objA )->GetTotemOwner())
		atA = dbcArea.LookupEntry( static_cast< Creature* >( objA )->GetTotemOwner()->GetAreaID() );
	else if( objA->IsUnit() && objA->GetUInt32Value(UNIT_FIELD_CREATEDBY) && objA->GetMapMgr())
		atA = dbcArea.LookupEntry( objA->GetMapMgr()->GetAreaID(objA->GetPositionX(),objA->GetPositionY()) );
	else
		atA = NULL;

	if( objB->IsPet() && static_cast< Pet* >( objB )->GetPetOwner() )
		atB = dbcArea.LookupEntry( static_cast< Pet* >( objB )->GetPetOwner()->GetAreaID() );
	else if( objB->IsPlayer() )
		atB = dbcArea.LookupEntry( static_cast< Player* >( objB )->GetAreaID() );
	else if( objB->IsCreature() && static_cast< Creature* >( objB )->IsTotem() &&
			static_cast< Creature* >( objB )->GetTotemOwner())
		atB = dbcArea.LookupEntry( static_cast< Creature* >( objB )->GetTotemOwner()->GetAreaID() );
	else if( objB->IsUnit() && objB->GetUInt32Value(UNIT_FIELD_CREATEDBY) && objB->GetMapMgr())
		atB = dbcArea.LookupEntry( objB->GetMapMgr()->GetAreaID(objB->GetPositionX(),objB->GetPositionY()) );
	else
		atB = NULL;

	// We have the area codes
	// We know they aren't dueling
	if (atA && atB)
	{
		if(atA->AreaFlags & 0x800 || atB->AreaFlags & 0x800)
			return false;
	}

	if(objA->m_faction == objB->m_faction)  // same faction can't kill each other unless in ffa pvp/duel
		return false;

	bool attackable = isHostile(objA, objB); // B is attackable if its hostile for A
	/*if((objA->m_faction->HostileMask & 8) && (objB->m_factionDBC->RepListId != 0) && 
		(objB->GetTypeId() != TYPEID_PLAYER) && objB->m_faction->Faction != 31) // B is attackable if its a neutral Creature*/

	// Neutral Creature Check
	if(objA->IsPlayer() || objA->IsPet())
	{
		if(objB->m_factionDBC->RepListId == -1 && objB->m_faction->HostileMask == 0 && objB->m_faction->FriendlyMask == 0)
		{
			attackable = true;
		}
	}
	else if(objB->IsPlayer() || objB->IsPet())
	{
		if(objA->m_factionDBC->RepListId == -1 && objA->m_faction->HostileMask == 0 && objA->m_faction->FriendlyMask == 0)
		{
			attackable = true;
		}
	}

	return attackable;
}

bool isCombatSupport(Object* objA, Object* objB)// B combat supports A?
{
	if(!objA || !objB)
		return false;

	if(objA->GetTypeId() == TYPEID_CORPSE)
		return false;

	if(objB->GetTypeId() == TYPEID_CORPSE)
		return false;

	if(objB->m_faction == 0 || objA->m_faction == 0)
		return false;

	bool combatSupport = false;

	uint32 fSupport = objB->m_faction->FriendlyMask;
	uint32 myFaction = objA->m_faction->Mask;

	if(myFaction & fSupport)
	{
		combatSupport = true;
	}
	// check friend/enemy list
	for(uint32 i = 0; i < 4; i++)
	{
		if(objB->m_faction->EnemyFactions[i] == objA->m_faction->Faction)
		{
			combatSupport = false;
			break;
		}
		if(objB->m_faction->FriendlyFactions[i] == objA->m_faction->Faction)
		{
			combatSupport = true;
			break;
		}
	}
	return combatSupport;
}


bool isAlliance(Object* objA)// A is alliance?
{
	FactionTemplateDBC * m_sw_faction = dbcFactionTemplate.LookupEntry(11);
	FactionDBC * m_sw_factionDBC = dbcFaction.LookupEntry(72);
	if(!objA || objA->m_factionDBC == NULL || objA->m_faction == NULL)
		return true;

	if(m_sw_faction == objA->m_faction || m_sw_factionDBC == objA->m_factionDBC)
		return true;

	//bool hostile = false;
	uint32 faction = m_sw_faction->Faction;
	uint32 host = objA->m_faction->HostileMask;

	if(faction & host)
		return false;

	// check friend/enemy list
	for(uint32 i = 0; i < 4; i++)
	{
		if(objA->m_faction->EnemyFactions[i] == faction)
			return false;
	}

	faction = objA->m_faction->Faction;
	host = m_sw_faction->HostileMask;

	if(faction & host)
		return false;

	// check friend/enemy list
	for(uint32 i = 0; i < 4; i++)
	{
		if(objA->m_faction->EnemyFactions[i] == faction)
			return false;
	}

	return true;
}

