#pragma once

#include <Gameplay/helper.h>
#include <Gameplay/inventory.h>

void DoHarvesting(UObject* Controller, UObject* BuildingActor, float Damage = 0.f)
{
	std::random_device rd; // obtain a random number from hardware
	std::mt19937 gen(rd()); // seed the generator
	std::uniform_int_distribution<> distr(4, 6); // define the range

	auto Random = distr(gen);

	auto HitWeakspot = (Damage) == 100.f;

	auto funne = distr(gen);

	if (HitWeakspot)
	{
		std::random_device rd; // obtain a random number from hardware
		std::mt19937 gen(rd()); // seed the generator
		std::uniform_int_distribution<> distr(4, 5); // define the range

		funne += distr(gen);
	}

	struct
	{
		UObject* BuildingSMActor;                                          // (Parm, ZeroConstructor, IsPlainOldData)
		TEnumAsByte<EFortResourceType>                     PotentialResourceType;                                    // (Parm, ZeroConstructor, IsPlainOldData)
		int                                                PotentialResourceCount;                                   // (Parm, ZeroConstructor, IsPlainOldData)
		bool                                               bDestroyed;                                               // (Parm, ZeroConstructor, IsPlainOldData)
		bool                                               bJustHitWeakspot;                                         // (Parm, ZeroConstructor, IsPlainOldData)
	} AFortPlayerController_ClientReportDamagedResourceBuilding_Params{ BuildingActor, *BuildingActor->Member<TEnumAsByte<EFortResourceType>>(("ResourceType")),
		 funne, false, HitWeakspot }; // ender weakspotrs

	static auto ClientReportDamagedResourceBuilding = Controller->Function(("ClientReportDamagedResourceBuilding"));

	if (ClientReportDamagedResourceBuilding)
	{
		auto Params = &AFortPlayerController_ClientReportDamagedResourceBuilding_Params;

		Controller->ProcessEvent(ClientReportDamagedResourceBuilding, &AFortPlayerController_ClientReportDamagedResourceBuilding_Params);

		// idk y hook no work

		auto Pawn = *Controller->Member<UObject*>(("Pawn"));

		static auto WoodItemData = FindObject(("FortResourceItemDefinition /Game/Items/ResourcePickups/WoodItemData.WoodItemData"));
		static auto StoneItemData = FindObject(("FortResourceItemDefinition /Game/Items/ResourcePickups/StoneItemData.StoneItemData"));
		static auto MetalItemData = FindObject(("FortResourceItemDefinition /Game/Items/ResourcePickups/MetalItemData.MetalItemData"));

		UObject* ItemDef = WoodItemData;

		if (Params->PotentialResourceType.Get() == EFortResourceType::Stone)
			ItemDef = StoneItemData;

		if (Params->PotentialResourceType.Get() == EFortResourceType::Metal)
			ItemDef = MetalItemData;

		auto ItemInstance = Inventory::FindItemInInventory(Controller, ItemDef);

		int AmountToGive = Params->PotentialResourceCount;

		if (ItemInstance && Pawn)
		{
			auto Entry = ItemInstance->Member<__int64>(("ItemEntry"));

			// BUG: You lose some mats if you have like 998 or idfk
			if (*FFortItemEntry::GetCount(Entry) >= 999)
			{
				Helper::SummonPickup(Pawn, ItemDef, Helper::GetActorLocation(Pawn), EFortPickupSourceTypeFlag::Other, EFortPickupSpawnSource::Unset, AmountToGive);
				return;
			}
		}

		Inventory::GiveItem(Controller, ItemDef, EFortQuickBars::Secondary, 1, AmountToGive);
	}
}

inline bool OnDamageServerHook(UObject* BuildingActor, UFunction* Function, void* Parameters)
{
	static auto BuildingSMActorClass = FindObject(("Class /Script/FortniteGame.BuildingSMActor"));
	
	if (BuildingActor->IsA(BuildingSMActorClass)) // || BuildingActor->GetFullName().contains(("Car_")))
	{
		auto InstigatedByOffset = FindOffsetStruct(("Function /Script/FortniteGame.BuildingActor.OnDamageServer"), ("InstigatedBy"));
		auto InstigatedBy = *(UObject**)(__int64(Parameters) + InstigatedByOffset);

		auto DamageCauserOffset = FindOffsetStruct(("Function /Script/FortniteGame.BuildingActor.OnDamageServer"), ("DamageCauser"));
		auto DamageCauser = *(UObject**)(__int64(Parameters) + DamageCauserOffset);

		auto DamageTagsOffset = FindOffsetStruct(("Function /Script/FortniteGame.BuildingActor.OnDamageServer"), ("DamageTags"));
		auto DamageTags = (FGameplayTagContainer*)(__int64(Parameters) + DamageTagsOffset);

		auto DamageOffset = FindOffsetStruct(("Function /Script/FortniteGame.BuildingActor.OnDamageServer"), ("Damage"));
		auto Damage = (float*)(__int64(Parameters) + DamageOffset);

		struct Bitfield
		{
			unsigned char                                      UnknownData09 : 1;                                        // 0x0544(0x0001)
			unsigned char                                      bWorldReadyCalled : 1;                                    // 0x0544(0x0001) (Transient)
			unsigned char                                      bBeingRotatedOrScaled : 1;                                // 0x0544(0x0001) (Transient)
			unsigned char                                      bBeingTranslated : 1;                                     // 0x0544(0x0001) (Transient)
			unsigned char                                      bRotateInPlaceEditor : 1;                                 // 0x0544(0x0001)
			unsigned char                                      bEditorPlaced : 1;                                        // 0x0544(0x0001) (Net, Transient)
			unsigned char                                      bPlayerPlaced : 1;                                        // 0x0544(0x0001) (Edit, BlueprintVisible, BlueprintReadOnly, Net, DisableEditOnTemplate)
			unsigned char                                      bShouldTick : 1;
		};

		auto BitField = BuildingActor->Member<Bitfield>(("bPlayerPlaced"));
		auto bPlayerPlaced = false; // BitField->bPlayerPlaced;

		static auto FortPlayerControllerAthenaClass = FindObject(("Class /Script/FortniteGame.FortPlayerControllerAthena"));

		// if (DamageTags)
			// std::cout << ("DamageTags: ") << DamageTags->ToStringSimple(false) << '\n';

		if (!bPlayerPlaced && InstigatedBy && InstigatedBy->IsA(FortPlayerControllerAthenaClass) &&
			DamageCauser->GetFullName().contains("B_Melee_Impact_Pickaxe_Athena_C")) // cursed
		{
			// TODO: Not hardcode the PickaxeDef, do like slot 0  or something
			static auto PickaxeDef = FindObject(("FortWeaponMeleeItemDefinition /Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01"));
			auto CurrentWeapon = *(*InstigatedBy->Member<UObject*>(("MyFortPawn")))->Member<UObject*>(("CurrentWeapon"));

			if (CurrentWeapon && *CurrentWeapon->Member<UObject*>(("WeaponData")) == PickaxeDef)
			{
				DoHarvesting(InstigatedBy, BuildingActor, *Damage);
			}
		}
;	}

	return false;
}

inline bool BlueprintCanAttemptGenerateResourcesHook(UObject* BuildingActor, UFunction* Function, void* Parameters)
{
	// very wrong idek whats happening

	struct parms {
		FGameplayTagContainer InTags;
		UObject* InstigatorController; // AController*
		bool ret;
	};

	auto Params = (parms*)Parameters;

	ProcessEventO(BuildingActor, Function, Params);

	static auto PickaxeDef = FindObject(("FortWeaponMeleeItemDefinition /Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01"));
	auto CurrentWeapon = *(*Params->InstigatorController->Member<UObject*>(("MyFortPawn")))->Member<UObject*>(("CurrentWeapon"));

	if (CurrentWeapon && *CurrentWeapon->Member<UObject*>(("WeaponData")) == PickaxeDef)
	{
		if (Params && Params->ret)
		{
			DoHarvesting(Params->InstigatorController, BuildingActor);
		}
	}

	return false;
}

void InitializeHarvestingHooks()
{
	AddHook(("Function /Script/FortniteGame.BuildingActor.OnDamageServer"), OnDamageServerHook);

	if (Engine_Version > 424)
		AddHook("Function /Script/FortniteGame.BuildingSMActor.BlueprintCanAttemptGenerateResources", BlueprintCanAttemptGenerateResourcesHook);
}