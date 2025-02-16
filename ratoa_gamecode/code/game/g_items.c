/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#include "g_local.h"

/*

  Items are any object that a player can touch to gain some effect.

  Pickup will return the number of seconds until they should respawn.

  all items should pop when dropped in lava or slime

  Respawnable items don't actually go away when picked up, they are
  just made invisible and untouchable.  This allows them to ride
  movers and respawn apropriately.
*/


#define	RESPAWN_ARMOR		25
#define	RESPAWN_HEALTH		35
#define	RESPAWN_AMMO		40
#define	RESPAWN_HOLDABLE	60
#define	RESPAWN_MEGAHEALTH	35//120
#define	RESPAWN_POWERUP		120

void G_PlayDenied(gentity_t *ent, gentity_t *other) {
	int i;
	gclient_t *client;

	// give any nearby players a "denied" anti-reward
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		vec3_t		delta;
		float		len;
		//vec3_t		forward;
		trace_t		tr;

		client = &level.clients[i];
		if ( client == other->client ) {
			continue;
		}
		if ( client->pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( client->ps.stats[STAT_HEALTH] <= 0 ) {
			continue;
		}

		// if same team in team game, no sound
		// cannot use OnSameTeam as it expects to g_entities, not clients
		if ( G_IsTeamGametype() && other->client->sess.sessionTeam == client->sess.sessionTeam  ) {
			continue;
		}

#ifdef WITH_MULTITOURNAMENT
		if ( g_gametype.integer == GT_MULTITOURNAMENT && other->client->sess.gameId != client->sess.gameId) {
			continue;
		}
#endif

		// if too far away, no sound
		VectorSubtract( ent->s.pos.trBase, client->ps.origin, delta );
		len = VectorNormalize( delta );
		if ( len > 192 ) {
			continue;
		}

		//// if not facing, no sound
		//AngleVectors( client->ps.viewangles, forward, NULL, NULL );
		//if ( DotProduct( delta, forward ) < 0.4 ) {
		//	continue;
		//}

		// if not line of sight, no sound
		trap_Trace( &tr, client->ps.origin, NULL, NULL, ent->s.pos.trBase, ENTITYNUM_NONE, CONTENTS_SOLID );
		if ( tr.fraction != 1.0 ) {
			continue;
		}

		// anti-reward
		client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_DENIEDREWARD;
	}
}

void CheckQuadwhore( gentity_t *item, gentity_t *player) {
	if (!g_quadWhore.integer) {
		return;
	}

	if (item->item->giTag != PW_QUAD) {
		return;
	}

	player->client->pers.quadNum++;

	if (player->client->pers.quadTime > 0 
			&& item->flags & FL_DROPPED_ITEM 
			&& player->client->pers.quadTime + (item->item->quantity)*1000*1.5 >= level.time) {
		// ignore re-pickup
		player->client->pers.quadTime = level.time;
		return;
	}

	//if ( item->flags & FL_DROPPED_ITEM ) {
	//}

	player->client->pers.quadWhore = 0;

	if ((player->client->pers.quadTime > 0 && player->client->pers.quadTime + 3*60*1000 < level.time)
			|| (player->client->pers.quadNum > 1 && (float)player->client->pers.quadNum / (float)((level.time - level.startTime)/1000) > 1.0/180.0 )) {
		// quad whore alert!
		trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " is a " S_COLOR_RED "QUAD WHORE!!!\n\"", player->client->pers.netname) );
		trap_SendServerCommand(player - g_entities, "cp \"QUAD WHORE!!!\"" );
		player->client->pers.quadWhore = 1;
	}

	player->client->pers.quadTime = level.time;
}


//======================================================================

int Pickup_Powerup( gentity_t *ent, gentity_t *other ) {
	int			quantity;

	if ( !other->client->ps.powerups[ent->item->giTag] ) {
		// round timing to seconds to make multiple powerup timers
		// count in sync
		other->client->ps.powerups[ent->item->giTag] = 
			level.time - ( level.time % 1000 );
	}

	if ( ent->count ) {
		quantity = ent->count;
	} else {
		quantity = ent->item->quantity;
	}

	other->client->ps.powerups[ent->item->giTag] += quantity * 1000;

	G_PlayDenied(ent, other);

	CheckQuadwhore(ent, other);

	return RESPAWN_POWERUP;
}

//======================================================================
/*
int Pickup_PersistantPowerup( gentity_t *ent, gentity_t *other ) {
	int		clientNum;
	char	userinfo[MAX_INFO_STRING];
	float	handicap;
	int		max;

	other->client->ps.stats[STAT_PERSISTANT_POWERUP] = ent->item - bg_itemlist;
	other->client->persistantPowerup = ent;

	switch( ent->item->giTag ) {
	case PW_GUARD:
		clientNum = other->client->ps.clientNum;
		trap_GetUserinfo( clientNum, userinfo, sizeof(userinfo) );
		handicap = atof( Info_ValueForKey( userinfo, "handicap" ) );
		if( handicap<=0.0f || handicap>100.0f) {
			handicap = 100.0f;
		}
		max = (int)(2 *  handicap);

		other->health = max;
		other->client->ps.stats[STAT_HEALTH] = max;
		other->client->ps.stats[STAT_MAX_HEALTH] = max;
		other->client->ps.stats[STAT_ARMOR] = max;
		other->client->pers.maxHealth = max;

		break;

	case PW_SCOUT:
		clientNum = other->client->ps.clientNum;
		trap_GetUserinfo( clientNum, userinfo, sizeof(userinfo) );
		handicap = atof( Info_ValueForKey( userinfo, "handicap" ) );
		if( handicap<=0.0f || handicap>100.0f) {
			handicap = 100.0f;
		}
		other->client->pers.maxHealth = handicap;
		other->client->ps.stats[STAT_ARMOR] = 0;
		break;

	case PW_DOUBLER:
		clientNum = other->client->ps.clientNum;
		trap_GetUserinfo( clientNum, userinfo, sizeof(userinfo) );
		handicap = atof( Info_ValueForKey( userinfo, "handicap" ) );
		if( handicap<=0.0f || handicap>100.0f) {
			handicap = 100.0f;
		}
		other->client->pers.maxHealth = handicap;
		break;
	case PW_AMMOREGEN:
		clientNum = other->client->ps.clientNum;
		trap_GetUserinfo( clientNum, userinfo, sizeof(userinfo) );
		handicap = atof( Info_ValueForKey( userinfo, "handicap" ) );
		if( handicap<=0.0f || handicap>100.0f) {
			handicap = 100.0f;
		}
		other->client->pers.maxHealth = handicap;
		memset(other->client->ammoTimes, 0, sizeof(other->client->ammoTimes));
		break;
	default:
		clientNum = other->client->ps.clientNum;
		trap_GetUserinfo( clientNum, userinfo, sizeof(userinfo) );
		handicap = atof( Info_ValueForKey( userinfo, "handicap" ) );
		if( handicap<=0.0f || handicap>100.0f) {
			handicap = 100.0f;
		}
		other->client->pers.maxHealth = handicap;
		break;
	}

	return -1;
}
*/
//======================================================================

int Pickup_Holdable( gentity_t *ent, gentity_t *other ) {

	other->client->ps.stats[STAT_HOLDABLE_ITEM] = ent->item - bg_itemlist;
#ifdef MISSIONPACK
	if( ent->item->giTag == HI_KAMIKAZE ) {
		other->client->ps.eFlags |= EF_KAMIKAZE;
	}
#endif
	return RESPAWN_HOLDABLE;
}


//======================================================================

void Add_Ammo (gentity_t *ent, int weapon, int count)
{
	ent->client->ps.ammo[weapon] += count;
	if ( ent->client->ps.ammo[weapon] > AMMO_CAPACITY ) {
		ent->client->ps.ammo[weapon] = AMMO_CAPACITY;
	}
}

int Pickup_Ammo (gentity_t *ent, gentity_t *other)
{
	int		quantity;

	if ( ent->count ) {
		quantity = ent->count;
	} else {
		quantity = ent->item->quantity;
	}

	Add_Ammo (other, ent->item->giTag, quantity);

	return RESPAWN_AMMO;
}

//======================================================================


int Pickup_Weapon (gentity_t *ent, gentity_t *other) {
	int		quantity;

	if ( ent->count < 0 ) {
		quantity = 0; // None for you, sir!
	} else {
		if ( ent->count ) {
			quantity = ent->count;
		} else {
			quantity = ent->item->quantity;
		}

		// dropped items and teamplay weapons always have full ammo
		if ( ! (ent->flags & FL_DROPPED_ITEM) && g_gametype.integer != GT_TEAM ) {
			// respawning rules
			// drop the quantity if the already have over the minimum
			if ( other->client->ps.ammo[ ent->item->giTag ] < quantity ) {
				quantity = quantity - other->client->ps.ammo[ ent->item->giTag ];
			} else {
				quantity = 1;		// only add a single shot
			}
		}
	}

	// add the weapon
	other->client->ps.stats[STAT_WEAPONS] |= ( 1 << ent->item->giTag );

	Add_Ammo( other, ent->item->giTag, quantity );

	// if (ent->item->giTag == WP_GRAPPLING_HOOK)
	//	other->client->ps.ammo[ent->item->giTag] = -1; // unlimited ammo

	// team deathmatch has slow weapon respawns
	if ( g_gametype.integer == GT_TEAM ) {
		return g_weaponTeamRespawn.integer;
	}

	return g_weaponRespawn.integer;
}


//======================================================================

int Pickup_Health (gentity_t *ent, gentity_t *other) {
	int			max;
	int			quantity;

        if( !other->client)
            return RESPAWN_HEALTH;
        
	// small and mega healths will go over the max
	/*
	if( other->client && bg_itemlist[other->client->ps.stats[STAT_PERSISTANT_POWERUP]].giTag == PW_GUARD ) {
		max = other->client->ps.stats[STAT_MAX_HEALTH];
	}
	else
	*/
	if ( ent->item->quantity != 5 && ent->item->quantity != 100 ) {
		max = other->client->ps.stats[STAT_MAX_HEALTH];
	} else {
		max = other->client->ps.stats[STAT_MAX_HEALTH] * 2;
	}

	if ( ent->count ) {
		quantity = ent->count;
	} else {
		quantity = ent->item->quantity;
	}

	other->health += quantity;

	if (other->health > max ) {
		other->health = max;
	}
	other->client->ps.stats[STAT_HEALTH] = other->health;

	if ( ent->item->quantity == 100 ) {		// mega health respawns slow
		G_PlayDenied(ent, other);
		return RESPAWN_MEGAHEALTH;
	}

	return RESPAWN_HEALTH;
}

//======================================================================

int Pickup_Armor( gentity_t *ent, gentity_t *other ) {
	int		upperBound;

	other->client->ps.stats[STAT_ARMOR] += ent->item->quantity;
	/*
	if( other->client && bg_itemlist[other->client->ps.stats[STAT_PERSISTANT_POWERUP]].giTag == PW_GUARD ) {
		upperBound = other->client->ps.stats[STAT_MAX_HEALTH];
	}
	else
	*/
	{
		upperBound = other->client->ps.stats[STAT_MAX_HEALTH] * 2;
	}

	if ( other->client->ps.stats[STAT_ARMOR] > upperBound ) {
		other->client->ps.stats[STAT_ARMOR] = upperBound;
	}

	if ( ent->item->quantity == 100 ) {
		G_PlayDenied(ent, other);
	}

	return RESPAWN_ARMOR;
}

int Pickup_Coin( gentity_t *ent, gentity_t *other ) {
	AddScore( other, other->r.currentOrigin, 1 );
	return -1;
}

//======================================================================

/*
===============
RespawnItem
===============
*/
void RespawnItem( gentity_t *ent ) {
        //Don't spawn quad if quadfactor are 1.0 or less
        //if(ent->item->giType == IT_POWERUP && ent->item->giTag == PW_QUAD && g_quadfactor.value <= 1.0)
        //    return;

	// randomly select from teamed entities
	if (ent->team) {
		gentity_t	*master;
		int	count;
		int choice;

		if ( !ent->teammaster ) {
			G_Error( "RespawnItem: bad teammaster");
		}
		master = ent->teammaster;

		for (count = 0, ent = master; ent; ent = ent->teamchain, count++)
			;

		choice = (count > 0)? rand()%count : 0;

		for (count = 0, ent = master; count < choice; ent = ent->teamchain, count++)
			;
	}

	ent->r.contents = CONTENTS_TRIGGER;
	ent->s.eFlags &= ~EF_NODRAW;
	ent->r.svFlags &= ~SVF_NOCLIENT;
	trap_LinkEntity (ent);

	if ( ent->item->giType == IT_POWERUP ) {
		// play powerup spawn sound to all clients
		gentity_t	*te;

		// if the powerup respawn sound should Not be global
		if (ent->speed) {
			te = G_TempEntity( ent->s.pos.trBase, EV_GENERAL_SOUND );
		}
		else {
			te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_SOUND );
		}
		te->s.eventParm = G_SoundIndex( "sound/items/poweruprespawn.wav" );
		te->r.svFlags |= SVF_BROADCAST;
	}
#ifdef MISSIONPACK
	if ( ent->item->giType == IT_HOLDABLE && ent->item->giTag == HI_KAMIKAZE ) {
		// play powerup spawn sound to all clients
		gentity_t	*te;

		// if the powerup respawn sound should Not be global
		if (ent->speed) {
			te = G_TempEntity( ent->s.pos.trBase, EV_GENERAL_SOUND );
		}
		else {
			te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_SOUND );
		}
		te->s.eventParm = G_SoundIndex( "sound/items/kamikazerespawn.wav" );
		te->r.svFlags |= SVF_BROADCAST;
	}
#endif
	// play the normal respawn sound only to nearby clients
	G_AddEvent( ent, EV_ITEM_RESPAWN, 0 );

	ent->nextthink = 0;
}


/*
===============
Touch_Item
===============
*/
void Touch_Item (gentity_t *ent, gentity_t *other, trace_t *trace) {
	int			respawn;
	qboolean	predict;
	int		overrideRespawn = 0;

	//instant gib
	if ((g_instantgib.integer 
		|| g_rockets.integer 
		|| ((g_gametype.integer == GT_CTF_ELIMINATION || g_elimination_allgametypes.integer) && !g_elimination_spawnitems.integer)
		)
                && ent->item->giType != IT_TEAM)
		return;

	//Cannot touch flag/items before round starts
	if(G_IsElimGT() && level.roundNumber != level.roundNumberStarted && level.warmupTime == 0)
		return;

	//Cannot take ctf elimination oneway
	if(g_gametype.integer == GT_CTF_ELIMINATION && ent->item->giType == IT_TEAM && g_elimination_ctf_oneway.integer!=0 && (
			(other->client->sess.sessionTeam==TEAM_BLUE && (level.eliminationSides+level.roundNumber)%2 == 0 ) ||
			(other->client->sess.sessionTeam==TEAM_RED && (level.eliminationSides+level.roundNumber)%2 != 0 ) ))
		return;

	if ((g_gametype.integer == GT_ELIMINATION || g_gametype.integer == GT_LMS) && !g_elimination_spawnitems.integer)
		return;		//nothing to pick up in elimination

	if (!other->client)
		return;
	if (other->health < 1)
		return;		// dead people can't pickup

	// the same pickup rules are used for client side and server side
	if ( !BG_CanItemBeGrabbed( g_gametype.integer, &ent->s, &other->client->ps ) ) {
		return;
	}
#ifdef WITH_DOM_GAMETYPE
	if ((g_gametype.integer == GT_DOMINATION) && ent->item->giType == IT_TEAM) {
		if (level.time < ent->dropPickupTime) {
			return;
		}
	}
#endif

#ifdef WITH_DOUBLED_GAMETYPE
	if ((g_gametype.integer == GT_DOUBLE_D) && ent->item->giType == IT_TEAM) {
		if (level.time < ent->dropPickupTime) {
			return;
		}
	}

	//In double DD we cannot "pick up" a flag we already got
	if(g_gametype.integer == GT_DOUBLE_D) {
		if( strcmp(ent->classname, "team_CTF_redflag") == 0 )
			if(other->client->sess.sessionTeam == level.pointStatusA)
				return;
		if( strcmp(ent->classname, "team_CTF_blueflag") == 0 )
			if(other->client->sess.sessionTeam == level.pointStatusB)
				return;
	}
#endif
	if (ent->dropPickupTime && ent->dropClientNum == other->client->ps.clientNum && ent->dropPickupTime > level.time) {
		return;
	}

	G_LogPrintf( "Item: %i %s\n", other->s.number, ent->item->classname );
	//G_Printf( "Client %i picked up %s\n", other->s.number, ent->item->classname ); //mrd
	other->client->pers.items_collected[ITEM_INDEX(ent->item)]++;

	predict = other->client->pers.predictItemPickup;

	// call the item-specific pickup function
	switch( ent->item->giType ) {
	case IT_WEAPON:
		respawn = Pickup_Weapon(ent, other);
		overrideRespawn = g_overrideWeaponRespawn.integer;
//		predict = qfalse;
		break;
	case IT_AMMO:
		respawn = Pickup_Ammo(ent, other);
//		predict = qfalse;
		break;
	case IT_ARMOR:
		respawn = Pickup_Armor(ent, other);
		break;
	case IT_HEALTH:
		respawn = Pickup_Health(ent, other);
		break;
	case IT_POWERUP:
		respawn = Pickup_Powerup(ent, other);
		predict = qfalse;
		break;
		/*
	case IT_PERSISTANT_POWERUP:
		respawn = Pickup_PersistantPowerup(ent, other);
		break;
		*/
	case IT_TEAM:
		respawn = Pickup_Team(ent, other);
                //If touching a team item remove spawnprotection
                if(other->client->spawnprotected)
                    other->client->spawnprotected = qfalse;
		break;
	case IT_HOLDABLE:
		respawn = Pickup_Holdable(ent, other);
		break;
	case IT_COIN:
		respawn = Pickup_Coin(ent, other);
		break;
	default:
		return;
	}

	if ( !respawn ) {
		return;
	}

	// play the normal pickup sound
	if (predict) {
		G_AddPredictableEvent( other, EV_ITEM_PICKUP, ent->s.modelindex );
	} else {
		G_AddEvent( other, EV_ITEM_PICKUP, ent->s.modelindex );
	}

	// powerup pickups are global broadcasts
	if ( ent->item->giType == IT_POWERUP || ent->item->giType == IT_TEAM) {
		// if we want the global sound to play
		if (!ent->speed) {
			gentity_t	*te;

			te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_ITEM_PICKUP );
			te->s.eventParm = ent->s.modelindex;
			te->r.svFlags |= SVF_BROADCAST;
		} else {
			gentity_t	*te;

			te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_ITEM_PICKUP );
			te->s.eventParm = ent->s.modelindex;
			// only send this temp entity to a single client
			te->r.svFlags |= SVF_SINGLECLIENT;
			te->r.singleClient = other->s.number;
		}
	}

	// fire item targets
	G_UseTargets (ent, other);

	// wait of -1 will not respawn
	if ( ent->wait == -1 ) {
		ent->r.svFlags |= SVF_NOCLIENT;
		ent->s.eFlags |= EF_NODRAW;
		ent->r.contents = 0;
		ent->unlinkAfterEvent = qtrue;
		return;
	}

	// non zero wait overrides respawn time
	if ( ent->wait && !overrideRespawn ) {
		respawn = ent->wait;
	}

	// random can be used to vary the respawn time
	if ( ent->random && !overrideRespawn) {
		respawn += crandom() * ent->random;
		if ( respawn < 1 ) {
			respawn = 1;
		}
	}

	// dropped items will not respawn
	if ( ent->flags & FL_DROPPED_ITEM ) {
		ent->freeAfterEvent = qtrue;
	}

	// picked up items still stay around, they just don't
	// draw anything.  This allows respawnable items
	// to be placed on movers.
	ent->r.svFlags |= SVF_NOCLIENT;
	ent->s.eFlags |= EF_NODRAW;
	ent->r.contents = 0;

	// ZOID
	// A negative respawn times means to never respawn this item (but don't 
	// delete it).  This is used by items that are respawned by third party 
	// events such as ctf flags
	if ( respawn <= 0 ) {
		ent->nextthink = 0;
		ent->think = 0;
	} else {
		ent->nextthink = level.time + respawn * 1000;
		ent->think = RespawnItem;
	}
	trap_LinkEntity( ent );
}


//======================================================================

/*
================
LaunchItem

Spawns an item and tosses it forward
================
*/
gentity_t *LaunchItem( gitem_t *item, vec3_t origin, vec3_t velocity ) {
	gentity_t	*dropped;

	dropped = G_Spawn();

	dropped->s.eType = ET_ITEM;
	dropped->s.modelindex = item - bg_itemlist;	// store item number in modelindex
	dropped->s.modelindex2 = 1; // This is non-zero is it's a dropped item

	dropped->classname = item->classname;
	dropped->item = item;
	VectorSet (dropped->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS);
	VectorSet (dropped->r.maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS);
	dropped->r.contents = CONTENTS_TRIGGER;

	dropped->touch = Touch_Item;

	G_SetOrigin( dropped, origin );
	dropped->s.pos.trType = TR_GRAVITY;
	dropped->s.pos.trTime = level.time;
	VectorCopy( velocity, dropped->s.pos.trDelta );

	dropped->s.eFlags |= EF_BOUNCE_HALF;
	// if ((g_gametype.integer == GT_CTF || g_gametype.integer == GT_1FCTF || g_gametype.integer == GT_CTF_ELIMINATION || g_gametype.integer == GT_DOUBLE_D)			&& item->giType == IT_TEAM) { // Special case for CTF flags
	// if ((g_gametype.integer == GT_CTF || g_gametype.integer == GT_1FCTF || g_gametype.integer == GT_CTF_ELIMINATION) && item->giType == IT_TEAM) { // Special case for CTF flags
	if ((g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTF_ELIMINATION) && item->giType == IT_TEAM) { // Special case for CTF flags
		dropped->think = Team_DroppedFlagThink;
		dropped->nextthink = level.time + 30000;
		Team_CheckDroppedItem( dropped );
	} else if (item->giType == IT_COIN) {
		dropped->think = G_FreeEntity;
		dropped->nextthink = level.time + g_coinTime.integer * 1000;
	} else { // auto-remove after 30 seconds
		dropped->think = G_FreeEntity;
		dropped->nextthink = level.time + 30000;
	}
	// let the client know when this will disappear to allow for more
	// accurate pickup prediction / fading out items
	dropped->s.time2 = dropped->nextthink;

	dropped->flags = FL_DROPPED_ITEM;

	trap_LinkEntity (dropped);

	return dropped;
}

/*
================
Drop_Item

Spawns an item and tosses it forward
================
*/
gentity_t *Drop_Item( gentity_t *ent, gitem_t *item, float angle ) {
	vec3_t	velocity;
	vec3_t	angles;

	VectorCopy( ent->s.apos.trBase, angles );
	angles[YAW] += angle;
	angles[PITCH] = 0;	// always forward

	AngleVectors( angles, velocity, NULL, NULL );
	VectorScale( velocity, 150, velocity );
	velocity[2] += 200 + crandom() * 50;
	
	return LaunchItem( item, ent->s.pos.trBase, velocity );
}

/*
================
Drop_ItemNonRamdom

Spawns an item and tosses it forward
================
*/
gentity_t *Drop_ItemNonRandom( gentity_t *ent, gitem_t *item, float angle ) {
	vec3_t forward, right, up;
	vec3_t muzzle;
	gentity_t *item_ent;

	if (!ent->client) {
		return NULL;
	}
	AngleVectors (ent->client->ps.viewangles, forward, right, up );

	CalcMuzzlePoint ( ent, forward, right, up, muzzle );

	forward[2] += 0.2f;
	VectorNormalize(forward);
	//VectorScale( forward, 350, forward );
	VectorScale( forward, 450, forward );

	// add player inertia
	//VectorAdd(forward, ent->client->ps.velocity, forward);
	
	//velocity[2] += 250;
	
	item_ent = LaunchItem( item, muzzle, forward );
	return item_ent;
}


/*
================
Use_Item

Respawn the item
================
*/
void Use_Item( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	RespawnItem( ent );
}

//======================================================================

/*
================
FinishSpawningItem

Traces down to find where an item should rest, instead of letting them
free fall from their spawn points
================
*/
void FinishSpawningItem( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		dest;

	VectorSet( ent->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS );
	VectorSet( ent->r.maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS );

	ent->s.eType = ET_ITEM;
	ent->s.modelindex = ent->item - bg_itemlist;		// store item number in modelindex
	ent->s.modelindex2 = 0; // zero indicates this isn't a dropped item

	ent->r.contents = CONTENTS_TRIGGER;
	ent->touch = Touch_Item;
	// useing an item causes it to respawn
	ent->use = Use_Item;

	if ( ent->spawnflags & 1 ) {
		// suspended
		G_SetOrigin( ent, ent->s.origin );
	} else {
		// drop to floor
		VectorSet( dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 4096 );
		trap_Trace( &tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ent->s.number, MASK_SOLID );
		if ( tr.startsolid ) {
			G_Printf ("FinishSpawningItem: %s startsolid at %s\n", ent->classname, vtos(ent->s.origin));
			G_FreeEntity( ent );
			return;
		}

		// allow to ride movers
		ent->s.groundEntityNum = tr.entityNum;

		G_SetOrigin( ent, tr.endpos );
	}

	// team slaves and targeted items aren't present at start
	if ( ( ent->flags & FL_TEAMSLAVE ) || ent->targetname ) {
		ent->s.eFlags |= EF_NODRAW;
		ent->r.contents = 0;
		return;
	}

	
	// powerups don't spawn in for a while (but not in elimination)
	if(((!G_IsElimGT() && !g_elimination_allgametypes.integer) || g_elimination_spawnitems.integer)
			&& !g_instantgib.integer && !g_rockets.integer )
	if ( ent->item->giType == IT_POWERUP ) {
		float	respawn;

		respawn = 45 + crandom() * 15;
		ent->s.eFlags |= EF_NODRAW;
		ent->r.contents = 0;
		ent->nextthink = level.time + respawn * 1000;
		ent->think = RespawnItem;
		return;
	}


	trap_LinkEntity (ent);
}


qboolean	itemRegistered[MAX_ITEMS];

/*
==================
G_CheckTeamItems
==================
*/
void G_CheckTeamItems( void ) {

	// Set up team stuff
	Team_InitGame();

#ifdef WITH_DOUBLED_GAMETYPE
	if( g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTF_ELIMINATION || g_gametype.integer == GT_DOUBLE_D ) {
#else
	if( g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTF_ELIMINATION ) {
#endif
		gitem_t	*item;

		// check for the two flags
		item = BG_FindItem( "Red Flag" );
		if ( !item || !itemRegistered[ item - bg_itemlist ] ) {
			G_Printf( S_COLOR_YELLOW "WARNING: No team_CTF_redflag in map\n" );
		}
		item = BG_FindItem( "Blue Flag" );
		if ( !item || !itemRegistered[ item - bg_itemlist ] ) {
			G_Printf( S_COLOR_YELLOW "WARNING: No team_CTF_blueflag in map\n" );
		}
	}
#ifdef MISSIONPACK
	if( g_gametype.integer == GT_1FCTF ) {
		gitem_t	*item;

		// check for all three flags
		item = BG_FindItem( "Red Flag" );
		if ( !item || !itemRegistered[ item - bg_itemlist ] ) {
			G_Printf( S_COLOR_YELLOW "WARNING: No team_CTF_redflag in map\n" );
		}
		item = BG_FindItem( "Blue Flag" );
		if ( !item || !itemRegistered[ item - bg_itemlist ] ) {
			G_Printf( S_COLOR_YELLOW "WARNING: No team_CTF_blueflag in map\n" );
		}
		item = BG_FindItem( "Neutral Flag" );
		if ( !item || !itemRegistered[ item - bg_itemlist ] ) {
			G_Printf( S_COLOR_YELLOW "WARNING: No team_CTF_neutralflag in map\n" );
		}
	}
	if( g_gametype.integer == GT_OBELISK ) {
		gentity_t	*ent;

		// check for the two obelisks
		ent = NULL;
		ent = G_Find( ent, FOFS(classname), "team_redobelisk" );
		if( !ent ) {
			G_Printf( S_COLOR_YELLOW "WARNING: No team_redobelisk in map\n" );
		}

		ent = NULL;
		ent = G_Find( ent, FOFS(classname), "team_blueobelisk" );
		if( !ent ) {
			G_Printf( S_COLOR_YELLOW "WARNING: No team_blueobelisk in map\n" );
		}
	}

	if( g_gametype.integer == GT_HARVESTER ) {
		gentity_t	*ent;

		// check for all three obelisks
		ent = NULL;
		ent = G_Find( ent, FOFS(classname), "team_redobelisk" );
		if( !ent ) {
			G_Printf( S_COLOR_YELLOW "WARNING: No team_redobelisk in map\n" );
		}

		ent = NULL;
		ent = G_Find( ent, FOFS(classname), "team_blueobelisk" );
		if( !ent ) {
			G_Printf( S_COLOR_YELLOW "WARNING: No team_blueobelisk in map\n" );
		}

		ent = NULL;
		ent = G_Find( ent, FOFS(classname), "team_neutralobelisk" );
		if( !ent ) {
			G_Printf( S_COLOR_YELLOW "WARNING: No team_neutralobelisk in map\n" );
		}
	}
#endif
}

/*
==============
ClearRegisteredItems
==============
*/
void ClearRegisteredItems( void ) {
	memset( itemRegistered, 0, sizeof( itemRegistered ) );

	if(g_instantgib.integer) {
            if(g_instantgib.integer & 2)
                RegisterItem( BG_FindItemForWeapon( WP_GAUNTLET ) );
            //RegisterItem( BG_FindItemForWeapon( WP_MACHINEGUN ) );
            RegisterItem( BG_FindItemForWeapon( WP_RAILGUN ) );
	}
        else
	if(g_rockets.integer) {
		//RegisterItem( BG_FindItemForWeapon( WP_GAUNTLET ) );
		//RegisterItem( BG_FindItemForWeapon( WP_MACHINEGUN ) );
		RegisterItem( BG_FindItemForWeapon( WP_ROCKET_LAUNCHER ) );
	}
	else
	{
		// players always start with the base weapon
		RegisterItem( BG_FindItemForWeapon( WP_MACHINEGUN ) );
		RegisterItem( BG_FindItemForWeapon( WP_GAUNTLET ) );
		if(G_IsElimGT() || g_elimination_allgametypes.integer)
		{
			RegisterItem( BG_FindItemForWeapon( WP_SHOTGUN ) );
			RegisterItem( BG_FindItemForWeapon( WP_GRENADE_LAUNCHER ) );
			RegisterItem( BG_FindItemForWeapon( WP_ROCKET_LAUNCHER ) );
			RegisterItem( BG_FindItemForWeapon( WP_LIGHTNING ) );
			RegisterItem( BG_FindItemForWeapon( WP_RAILGUN ) );
			RegisterItem( BG_FindItemForWeapon( WP_PLASMAGUN ) );
			RegisterItem( BG_FindItemForWeapon( WP_BFG ) );
#ifdef MISSIONPACK
			RegisterItem( BG_FindItemForWeapon( WP_NAILGUN ) );
			RegisterItem( BG_FindItemForWeapon( WP_PROX_LAUNCHER ) );
			RegisterItem( BG_FindItemForWeapon( WP_CHAINGUN ) );
#endif
		}
	}
#ifdef MISSIONPACK
	if( g_gametype.integer == GT_HARVESTER ) {
		RegisterItem( BG_FindItem( "Red Cube" ) );
		RegisterItem( BG_FindItem( "Blue Cube" ) );
	}
#endif

#ifdef WITH_TREASURE_HUNTER_GAMETYPE
	if( g_gametype.integer == GT_TREASURE_HUNTER ) {
		RegisterItem( BG_FindItem( "Red Cube" ) );
		RegisterItem( BG_FindItem( "Blue Cube" ) );
	}
#endif

#ifdef WITH_DOUBLED_GAMETYPE
	if( g_gametype.integer == GT_DOUBLE_D ) {
		RegisterItem( BG_FindItem( "Point A (Blue)" ) );
		RegisterItem( BG_FindItem( "Point A (Red)" ) );
		RegisterItem( BG_FindItem( "Point A (White)" ) );
		RegisterItem( BG_FindItem( "Point B (Blue)" ) );
		RegisterItem( BG_FindItem( "Point B (Red)" ) );
		RegisterItem( BG_FindItem( "Point B (White)" ) );
	}
#endif

#ifdef WITH_DOM_GAMETYPE
	if( g_gametype.integer == GT_DOMINATION ) {
		RegisterItem( BG_FindItem( "Neutral domination point" ) );
		RegisterItem( BG_FindItem( "Red domination point" ) );
		RegisterItem( BG_FindItem( "Blue domination point" ) );
	}
#endif

	
/* For Coin{FFA,..} */
#if 0
	if (g_coins.integer > 0) {
		if (G_IsTeamGametype()) {
			RegisterItem( BG_FindItem( "Red Coin" ) );
			RegisterItem( BG_FindItem( "Blue Coin" ) );
		} else {
			RegisterItem( BG_FindItem( "Gold Coin" ) );
		}
	}
#endif
}

/*
===============
RegisterItem

The item will be added to the precache list
===============
*/
void RegisterItem( gitem_t *item ) {
	if ( !item ) {
		G_Error( "RegisterItem: NULL" );
	}
	itemRegistered[ item - bg_itemlist ] = qtrue;
}


/*
===============
SaveRegisteredItems

Write the needed items to a config string
so the client will know which ones to precache
===============
*/
void SaveRegisteredItems( void ) {
	char	string[MAX_ITEMS+1];
	int		i;
	int		count;

	count = 0;
	for ( i = 0 ; i < bg_numItems ; i++ ) {
		if ( itemRegistered[i] ) {
			count++;
			string[i] = '1';
		} else {
			string[i] = '0';
		}
	}
	string[ bg_numItems ] = 0;
        G_Printf( "%i items registered\n", count );
	trap_SetConfigstring(CS_ITEMS, string);
}

/*
============
G_ItemDisabled
============
*/
int G_ItemDisabled( gitem_t *item ) {

	char name[128];

	if (!g_enableGreenArmor.integer && strcmp("item_armor_jacket", item->classname) == 0) {
		return 1;
	}

	Com_sprintf(name, sizeof(name), "disable_%s", item->classname);
	return trap_Cvar_VariableIntegerValue( name );
}

/*
============
G_SpawnItem

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
void G_SpawnItem (gentity_t *ent, gitem_t *item) {
	G_SpawnFloat( "random", "0", &ent->random );
	G_SpawnFloat( "wait", "0", &ent->wait );

	if((item->giType == IT_TEAM && (g_instantgib.integer || g_rockets.integer) ) || (!g_instantgib.integer && !g_rockets.integer) )
	{
		//Don't load pickups in Elimination (or maybe... gives warnings)
		if (!G_IsElimGT() || g_elimination_spawnitems.integer)
			RegisterItem( item );
		//Registrer flags anyway in CTF Elimination:
		if (g_gametype.integer == GT_CTF_ELIMINATION && item->giType == IT_TEAM)
			RegisterItem( item );
		if ( G_ItemDisabled(item) ) {
			// XXX FIXME:
			// this is to prevent this entity to be registered into a team (item group), which will cause an error later on
			// Maybe this entity should be free()d completely instead?
			ent->team = 0;
			return;
		}
	}
        if(!g_persistantpowerups.integer && item->giType == IT_PERSISTANT_POWERUP)
            return;

	ent->item = item;
	// some movers spawn on the second frame, so delay item
	// spawns until the third frame so they can ride trains
	ent->nextthink = level.time + FRAMETIME * 2;
	ent->think = FinishSpawningItem;

	ent->physicsBounce = 0.50;		// items are bouncy

	if (((g_gametype.integer == GT_ELIMINATION || g_gametype.integer == GT_LMS) && !g_elimination_spawnitems.integer) || 
			( item->giType != IT_TEAM && (g_instantgib.integer || g_rockets.integer || 
						      ((g_elimination_allgametypes.integer || g_gametype.integer==GT_CTF_ELIMINATION) && !g_elimination_spawnitems.integer)) ) ) {
		ent->s.eFlags |= EF_NODRAW; //Invisible in elimination
                ent->r.svFlags |= SVF_NOCLIENT;  //Don't broadcast
        }
#ifdef WITH_DOUBLED_GAMETYPE
	if( g_gametype.integer == GT_DOUBLE_D && (strcmp(ent->classname, "team_CTF_redflag") == 0 || strcmp(ent->classname, "team_CTF_blueflag") == 0 || strcmp(ent->classname, "team_CTF_neutralflag") == 0 || item->giType == IT_PERSISTANT_POWERUP ))
		ent->s.eFlags |= EF_NODRAW; //Don't draw the flag models/persistant powerups
#endif

#ifdef MISSIONPACK
	if( g_gametype.integer != GT_1FCTF && strcmp(ent->classname, "team_CTF_neutralflag") == 0 )
		ent->s.eFlags |= EF_NODRAW; // Don't draw the flag in CTF_elimination
#endif

    if( strcmp(ent->classname, "domination_point") == 0 )
        ent->s.eFlags |= EF_NODRAW; // Don't draw domination_point. It is just a pointer to where the Domination points should be placed
	if ( item->giType == IT_POWERUP ) {
		G_SoundIndex( "sound/items/poweruprespawn.wav" );
		G_SpawnFloat( "noglobalsound", "0", &ent->speed);
	}

	if ( item->giType == IT_PERSISTANT_POWERUP ) {
		ent->s.generic1 = ent->spawnflags;
	}
}


/*
================
G_BounceItem

================
*/
void G_BounceItem( gentity_t *ent, trace_t *trace ) {
	vec3_t	velocity;
	float	dot;
	int		hitTime;

	// reflect the velocity on the trace plane
	hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
	BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2*dot, trace->plane.normal, ent->s.pos.trDelta );

	// cut the velocity to keep from bouncing forever
	VectorScale( ent->s.pos.trDelta, ent->physicsBounce, ent->s.pos.trDelta );

	// check for stop
	if ( trace->plane.normal[2] > 0 && ent->s.pos.trDelta[2] < 40 ) {
		trace->endpos[2] += 1.0;	// make sure it is off ground
		if (!G_IsFrozenPlayerRemnant(ent)) {
			SnapVector( trace->endpos );
		}
		G_SetOrigin( ent, trace->endpos );
		ent->s.groundEntityNum = trace->entityNum;
		return;
	}

	VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin);
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trTime = level.time;

	if (ent->item && ent->item->giType == IT_COIN) {
		G_AddEvent( ent, EV_COIN_BOUNCE, 0 );
	}
}


/*
================
G_RunItem

================
*/
void G_RunItem( gentity_t *ent ) {
	vec3_t		origin;
	trace_t		tr;
	int			contents;
	int			mask;

	// if groundentity has been set to -1, it may have been pushed off an edge
	if ( ent->s.groundEntityNum == -1 ) {
		if ( ent->s.pos.trType != TR_GRAVITY ) {
			ent->s.pos.trType = TR_GRAVITY;
			ent->s.pos.trTime = level.time;
		}
	}

	if ( ent->s.pos.trType == TR_STATIONARY ) {
		// check think function
		G_RunThink( ent );
		return;
	}

	// get current position
	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );

	// trace a line from the previous position to the current position
	if ( ent->clipmask ) {
		mask = ent->clipmask;
	} else {
		mask = MASK_PLAYERSOLID & ~CONTENTS_BODY;//MASK_SOLID;
	}
	trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, 
		ent->r.ownerNum, mask );

	VectorCopy( tr.endpos, ent->r.currentOrigin );

	if ( tr.startsolid ) {
		tr.fraction = 0;
	}

	trap_LinkEntity( ent );	// FIXME: avoid this for stationary?

	// check think function
	G_RunThink( ent );

	if ( tr.fraction == 1 ) {
		return;
	}

	// if it is in a nodrop volume, remove it
	contents = trap_PointContents( ent->r.currentOrigin, -1 );
	if ( contents & CONTENTS_NODROP ) {
		if (ent->item && ent->item->giType == IT_TEAM) {
			Team_FreeEntity(ent);
		} else {
			G_FreeEntity( ent );
		}
		return;
	}

	G_BounceItem( ent, &tr );
}

