/*
  Repop a single NPC: if targeted NPC has a spawn2 point, depop and respawn it from that spawn2.
  Otherwise spawn the NPC type at the caller's location. If no target, an NPC type id may be provided.
*/

#include "zone/client.h"
#include "zone/npc.h"
#include "zone/questmgr.h"
#include "zone/entity.h"
#include "zone/zone.h"
#include "common/strings.h"
#include "fmt/format.h"

extern QuestManager quest_manager;
extern Zone* zone;

void command_repopnpc(Client *c, const Seperator *sep)
{
	if (!c)
		return;

	// If an arg provided and it's a number, treat it as an NPC type id to spawn at caller's location
	uint32_t arg_npc_id = 0;
	if (sep->argnum > 0 && sep->IsNumber(1)) {
		arg_npc_id = Strings::ToUnsignedInt(sep->arg[1]);
	}

	Mob* target = c->GetTarget();

	if (target && target->IsNPC()) {
		NPC* npc = target->CastToNPC();
		uint32_t spawn2_id = npc->GetSpawnPointID();

		if (spawn2_id > 0) {
			// Depop the existing NPC and force the spawn2 to spawn a fresh mob
			npc->Depop();
			Mob* spawned = quest_manager.spawn_from_spawn2(spawn2_id);
			if (spawned) {
				c->Message(Chat::White, fmt::format("Respawned {} from spawnpoint {}.", spawned->GetCleanName(), spawn2_id).c_str());
			} else {
				c->Message(Chat::White, fmt::format("Failed to respawn from spawnpoint {}.", spawn2_id).c_str());
			}
			return;
		}

		// No spawnpoint: spawn the same NPC type at caller location
		auto npc_type_id = npc->GetNPCTypeID();
		auto tmp = content_db.LoadNPCTypesData(npc_type_id);
		if (tmp) {
			auto newnpc = new NPC(tmp, nullptr, c->GetPosition(), GravityBehavior::Water);
			if (newnpc) {
				newnpc->AddLootTable();
				if (newnpc->DropsGlobalLoot())
					newnpc->CheckGlobalLootTables();
				entity_list.AddNPC(newnpc);
				c->Message(Chat::White, fmt::format("Spawned {} ({}) at your location.", newnpc->GetCleanName(), npc_type_id).c_str());
			} else {
				c->Message(Chat::White, fmt::format("Failed to spawn NPC type {}.", npc_type_id).c_str());
			}
		} else {
			c->Message(Chat::White, fmt::format("NPC type {} was not found.", npc_type_id).c_str());
		}

		return;
	}

	// No NPC target: if numeric arg provided, spawn that NPC type at caller location
	if (arg_npc_id > 0) {
		// Try to find a spawn2 that would spawn this NPC type and use it if found
		LinkedListIterator<Spawn2 *> iterator(zone->spawn2_list);
		iterator.Reset();
		Spawn2 *found_spawn = nullptr;
		while (iterator.MoreElements()) {
			Spawn2 *cur = iterator.GetData();
			iterator.Advance();
			if (cur->CurrentNPCID() == arg_npc_id) {
				found_spawn = cur;
				break;
			}

			SpawnGroup *sg = zone->spawn_group_list.GetSpawnGroup(cur->SpawnGroupID());
			if (!sg) {
				content_db.LoadSpawnGroupsByID(cur->SpawnGroupID(), &zone->spawn_group_list);
				sg = zone->spawn_group_list.GetSpawnGroup(cur->SpawnGroupID());
				if (!sg)
					continue;
			}

			uint16 condition_value = 1;
			uint16 condition_id = cur->GetSpawnCondition();
			if (condition_id > 0) {
				condition_value = zone->spawn_conditions.GetCondition(zone->GetShortName(), zone->GetInstanceID(), condition_id);
			}

			// If the spawn group contains the requested npc type, select this spawn2
			if (sg->ContainsNPCType(arg_npc_id)) {
				found_spawn = cur;
				break;
			}
		}

		if (found_spawn) {
			if (found_spawn->NPCPointerValid()) {
				found_spawn->Depop();
			}
			Mob* spawned = quest_manager.spawn_from_spawn2(found_spawn->GetID());
			if (!spawned) {
				// If initial attempt failed (possible due to timers), trigger immediate repop and retry
				found_spawn->Repop(0);
				spawned = quest_manager.spawn_from_spawn2(found_spawn->GetID());
			}

			if (spawned) {
				c->Message(Chat::White, fmt::format("Respawned {} from spawnpoint {}.", spawned->GetCleanName(), found_spawn->GetID()).c_str());
			} else {
				c->Message(Chat::White, fmt::format("Failed to respawn from spawnpoint {}.", found_spawn->GetID()).c_str());
			}
			return;
		}

		// Fallback: spawn at caller location
		auto tmp = content_db.LoadNPCTypesData(arg_npc_id);
		if (tmp) {
			auto newnpc = new NPC(tmp, nullptr, c->GetPosition(), GravityBehavior::Water);
			if (newnpc) {
				newnpc->AddLootTable();
				if (newnpc->DropsGlobalLoot())
					newnpc->CheckGlobalLootTables();
				entity_list.AddNPC(newnpc);
				c->Message(Chat::White, fmt::format("Spawned {} ({}) at your location.", newnpc->GetCleanName(), arg_npc_id).c_str());
			} else {
				c->Message(Chat::White, fmt::format("Failed to spawn NPC type {}.", arg_npc_id).c_str());
			}
		} else {
			c->Message(Chat::White, fmt::format("NPC id {} was not found.", arg_npc_id).c_str());
		}
		return;
	}

	// Nothing to do
	c->Message(Chat::White, "Usage: target an NPC and use #repopnpc, or use #repopnpc <npc_type_id> to spawn at your location.");
}
