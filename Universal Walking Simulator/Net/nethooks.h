#pragma once

#include <MinHook/MinHook.h>

#include <Net/funcs.h>
#include <Gameplay/helper.h>
#include <Gameplay/inventory.h>
#include <Gameplay/abilities.h>
#include <Gameplay/loot.h>

#include <discord.h>
#include <Net/replication.h>

static bool bTraveled = false;

inline bool ServerAcknowledgePossessionHook(UObject* Object, UFunction* Function, void* Parameters) // This fixes movement on S5+, Cat led me to the right direction, then I figured out it's something with ClientRestart and did some common sense and found this.
{
    struct SAP_Params
    {
        UObject* P;
    };
    auto Params = (SAP_Params*)Parameters;
    if (Params)
    {
        auto Pawn = Params->P; // (UObject*)Parameters;

        *Object->Member<UObject*>(("AcknowledgedPawn")) = Pawn;
        std::cout << "Set Pawn!\n";
    }

    return false;
}

void InitializeNetUHooks()
{
    AddHook(("Function /Script/Engine.PlayerController.ServerAcknowledgePossession"), ServerAcknowledgePossessionHook);
}

void TickFlushDetour(UObject* thisNetDriver, float DeltaSeconds)
{
    // std::cout << ("TicKFluSh!\n");
    auto World = Helper::GetWorld();

    if (World)
    {
        static auto NetDriverOffset = GetOffset(World, "NetDriver");
        auto NetDriver = *(UObject**)(__int64(World) + NetDriverOffset);

        if (NetDriver)
        {
            static auto& ClientConnections = *NetDriver->Member<TArray<UObject*>>(("ClientConnections"));

            if (ClientConnections.Num() > 0)
            {
                ServerReplicateActors(NetDriver);
            }
        }
        else
            std::cout << "No World netDriver??\n";
    }
    else
        std::cout << "no world?!?!\n";

    return TickFlush(thisNetDriver, DeltaSeconds);
}

uint64_t GetNetModeDetour(UObject* World)
{
    return ENetMode::NM_ListenServer;
}

bool LP_SpawnPlayActorDetour(UObject* Player, const FString& URL, FString& OutError, UObject* World)
{
    if (!bTraveled)
    {
        return LP_SpawnPlayActor(Player, URL, OutError, World);
    }
    return true;
}

char KickPlayerDetour(__int64 a1, __int64 a2, __int64 a3)
{
    std::cout << ("Player Kick! Returning 0..\n");
    return 0;
}

char __fastcall ValidationFailureDetour(__int64* a1, __int64 a2)
{
    std::cout << ("Validation!\n");
    return 0;
}

bool bMyPawn = false; // UObject*

UObject* SpawnPlayActorDetour(UObject* World, UObject* NewPlayer, ENetRole RemoteRole, FURL& URL, void* UniqueId, FString& Error, uint8_t NetPlayerIndex)
{
    static bool bSpawnedFloorLoot = false;

    if (!bSpawnedFloorLoot)
    {
        // CreateThread(0, 0, Looting::Tables::SpawnFloorLoot, 0, 0, 0);
        // CreateThread(0, 0, Looting::Tables::SpawnVehicles, 0, 0, 0);
        bSpawnedFloorLoot = true;
    }

    std::cout << ("SpawnPlayActor called!\n");
    // static UObject* CameraManagerClass = FindObject(("BlueprintGeneratedClass /Game/Blueprints/Camera/Original/MainPlayerCamera.MainPlayerCamera_C"));

    auto PlayerController = SpawnPlayActor(Helper::GetWorld(), NewPlayer, RemoteRole, URL, UniqueId, Error, NetPlayerIndex); // crashes 0x356 here sometimes when rejoining

    // *PlayerController->Member<UObject*>(("PlayerCameraManagerClass")) = CameraManagerClass;

    static auto FortPlayerControllerAthenaClass = FindObject(("Class /Script/FortniteGame.FortPlayerControllerAthena"));

    if (!PlayerController || !PlayerController->IsA(FortPlayerControllerAthenaClass))
        return nullptr;

    auto PlayerState = *PlayerController->Member<UObject*>(("PlayerState"));

    if (!PlayerState) // this happened somehow
        return PlayerController;

    /* if (Helper::Banning::IsBanned(Helper::GetfIP(PlayerState).Data.GetData()))
    {
        FString Reason;
        Reason.Set(L"You are banned!");
        Helper::KickController(PlayerController, Reason);
    } */

    auto OPlayerName = Helper::GetPlayerName(PlayerState);
    std::string PlayerName = OPlayerName;
    std::transform(OPlayerName.begin(), OPlayerName.end(), OPlayerName.begin(), ::tolower);

    if (OPlayerName.contains(("fuck")) || OPlayerName.contains(("shit")))
    {
        FString Reason;
        Reason.Set(L"Inappropriate name!");
        Helper::KickController(PlayerController, Reason);
    }

    if (OPlayerName.length() >= 40)
    {
        FString Reason;
        Reason.Set(L"Too long of a name!");
        Helper::KickController(PlayerController, Reason);
    }

    *NewPlayer->Member<UObject*>(("PlayerController")) = PlayerController;

    if (FnVerDouble < 7.4)
    {
        static const auto QuickBarsClass = FindObject("Class /Script/FortniteGame.FortQuickBars", true);
        auto QuickBars = PlayerController->Member<UObject*>("QuickBars");

        if (QuickBars)
        {
            *QuickBars = Easy::SpawnActor(QuickBarsClass, FVector(), FRotator());
            Helper::SetOwner(*QuickBars, PlayerController);
        }
    }

    *PlayerController->Member<char>(("bReadyToStartMatch")) = true;
    *PlayerController->Member<char>(("bClientPawnIsLoaded")) = true;
    *PlayerController->Member<char>(("bHasInitiallySpawned")) = true;

    *PlayerController->Member<bool>(("bHasServerFinishedLoading")) = true;
    *PlayerController->Member<bool>(("bHasClientFinishedLoading")) = true;

    *PlayerState->Member<char>(("bHasStartedPlaying")) = true;
    *PlayerState->Member<char>(("bHasFinishedLoading")) = true;
    *PlayerState->Member<char>(("bIsReadyToContinue")) = true;

    auto Pawn = Helper::InitPawn(PlayerController, true, Helper::GetPlayerStart(), true);

    if (!Pawn)
        return PlayerController;

    if (GiveAbility || GiveAbilityFTS || GiveAbilityNewer)
    {
        auto AbilitySystemComponent = *Pawn->Member<UObject*>(("AbilitySystemComponent"));

        if (AbilitySystemComponent)
        {
            std::cout << ("Granting abilities!\n");

            if (FnVerDouble < 8 && Engine_Version != 421) // idk CDO offset
            {
                static auto AbilitySet = FindObject(("FortAbilitySet /Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_DefaultPlayer.GAS_DefaultPlayer"));

                if (AbilitySet)
                {
                    auto Abilities = AbilitySet->Member<TArray<UObject*>>(("GameplayAbilities"));

                    if (Abilities)
                    {
                        for (int i = 0; i < Abilities->Num(); i++)
                        {
                            auto Ability = Abilities->At(i);

                            if (!Ability)
                                continue;

                            GrantGameplayAbility(Pawn, Ability);
                            std::cout << "Granting ability " << Ability->GetFullName() << '\n';
                        }
                    }
                }

                static auto RangedAbility = FindObject(("BlueprintGeneratedClass /Game/Abilities/Weapons/Ranged/GA_Ranged_GenericDamage.GA_Ranged_GenericDamage_C"));

                if (RangedAbility)
                    GrantGameplayAbility(Pawn, RangedAbility);
            }
            else
            {
                if (FnVerDouble >= 8 && Engine_Version < 424)
                {
                    static auto AbilitySet = FindObject(("FortAbilitySet /Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer"));

                    if (AbilitySet)
                    {
                        auto Abilities = AbilitySet->Member<TArray<UObject*>>(("GameplayAbilities"));

                        if (Abilities)
                        {
                            for (int i = 0; i < Abilities->Num(); i++)
                            {
                                auto Ability = Abilities->At(i);

                                if (!Ability)
                                    continue;

                                GrantGameplayAbility(Pawn, Ability);
                                std::cout << "Granting ability " << Ability->GetFullName() << '\n';
                            }
                        }
                    }
                }
                else
                {
                    // static auto MatsAbility = FindObject(("/Game/Athena/Playlists/Fill/GA_Fill.GA_Fill"));
//GrantGameplayAbility(Pawn, MatsAbility);

                    static auto SprintAbility = FindObject("FortGameplayAbility_Sprint /Script/FortniteGame.Default__FortGameplayAbility_Sprint");
                    static auto JumpAbility = FindObject("FortGameplayAbility_Jump /Script/FortniteGame.Default__FortGameplayAbility_Jump");
                    static auto InteractUseAbility = FindObject("GA_DefaultPlayer_InteractUse_C /Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.Default__GA_DefaultPlayer_InteractUse_C");
                    static auto InteractSearchAbility = FindObject("GA_DefaultPlayer_InteractSearch_C /Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.Default__GA_DefaultPlayer_InteractSearch_C");
                    static auto ExitAbility = FindObject("GA_AthenaExitVehicle_C /Game/Athena/DrivableVehicles/GA_AthenaExitVehicle.Default__GA_AthenaExitVehicle_C");
                    static auto EnterAbility = FindObject("GA_AthenaEnterVehicle_C /Game/Athena/DrivableVehicles/GA_AthenaEnterVehicle.Default__GA_AthenaEnterVehicle_C");
                    static auto InAbility = FindObject("GA_AthenaInVehicle_C /Game/Athena/DrivableVehicles/GA_AthenaInVehicle.Default__GA_AthenaInVehicle_C");
                    static auto RangedAbility = FindObject("GA_Ranged_GenericDamage_C /Game/Abilities/Weapons/Ranged/GA_Ranged_GenericDamage.Default__GA_Ranged_GenericDamage_C");
                    
                    /*
                    
                    [502001] GA_AthenaEnterVehicle_C /Game/Athena/DrivableVehicles/GA_AthenaEnterVehicle.Default__GA_AthenaEnterVehicle_C
                    [502002] GA_AthenaExitVehicle_C /Game/Athena/DrivableVehicles/GA_AthenaExitVehicle.Default__GA_AthenaExitVehicle_C
                    [502003] GA_AthenaInVehicle_C /Game/Athena/DrivableVehicles/GA_AthenaInVehicle.Default__GA_AthenaInVehicle_C

                    */

                    // GAB_CarryPlayer_C

                    if (RangedAbility)
                        GrantGameplayAbility(Pawn, RangedAbility);

                    if (Engine_Version >= 424)
                    {
                        static auto JumpOutAbility = FindObject("GA_Athena_HidingProp_JumpOut_C /Game/Athena/Items/EnvironmentalItems/HidingProps/GA_Athena_HidingProp_JumpOut.Default__GA_Athena_HidingProp_JumpOut_C");
                        static auto HidingAbility = FindObject("GA_Athena_HidingProp_Hide_C /Game/Athena/Items/EnvironmentalItems/HidingProps/GA_Athena_HidingProp_Hide.Default__GA_Athena_HidingProp_Hide_C");
                        static auto LandedOnAbility = FindObject("GA_Athena_HidingProp_LandedOn_C /Game/Athena/Items/EnvironmentalItems/HidingProps/GA_Athena_HidingProp_LandedOn.Default__GA_Athena_HidingProp_LandedOn_C");

                        if (JumpOutAbility)
                            GrantGameplayAbility(Pawn, JumpOutAbility);
                        if (HidingAbility)
                            GrantGameplayAbility(Pawn, HidingAbility);
                        if (LandedOnAbility)
                            GrantGameplayAbility(Pawn, LandedOnAbility);
                    }

                    if (InAbility)
                        GrantGameplayAbility(Pawn, InAbility);
                    else
                        std::cout << "No InAbility!\n";

                    if (EnterAbility)
                        GrantGameplayAbility(Pawn, EnterAbility);
                    else
                        std::cout << "No EnterAbility!\n";

                    if (ExitAbility)
                        GrantGameplayAbility(Pawn, ExitAbility);
                    else
                        std::cout << "No ExitAbility!\n";

                    if (SprintAbility)
                        GrantGameplayAbility(Pawn, SprintAbility);
                    else
                        std::cout << "No SprintAbility!\n";
                    if (JumpAbility)
                        GrantGameplayAbility(Pawn, JumpAbility);
                    else
                        std::cout << "No JumpAbility!\n";
                    if (InteractUseAbility)
                        GrantGameplayAbility(Pawn, InteractUseAbility);
                    else
                        std::cout << "No InteractUseAbility!\n";
                    if (InteractSearchAbility)
                        GrantGameplayAbility(Pawn, InteractSearchAbility);
                    else
                        std::cout << "No InteractSearchAbility!\n";

                }
            /*    static auto SprintAbility = FindObject(("Class /Script/FortniteGame.FortGameplayAbility_Sprint"));
                static auto ReloadAbility = FindObject(("Class /Script/FortniteGame.FortGameplayAbility_Reload"));
                static auto JumpAbility = FindObject(("Class /Script/FortniteGame.FortGameplayAbility_Jump"));
                static auto InteractUseAbility = FindObject(("BlueprintGeneratedClass /Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C"));
                static auto InteractSearchAbility = FindObject("BlueprintGeneratedClass /Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C");
                static auto EnterVehicleAbility = FindObject(("BlueprintGeneratedClass /Game/Athena/DrivableVehicles/GA_AthenaEnterVehicle.GA_AthenaEnterVehicle_C"));
                static auto ExitVehicleAbility = FindObject(("BlueprintGeneratedClass /Game/Athena/DrivableVehicles/GA_AthenaExitVehicle.GA_AthenaExitVehicle_C"));
                static auto InVehicleAbility = FindObject(("BlueprintGeneratedClass /Game/Athena/DrivableVehicles/GA_AthenaInVehicle.GA_AthenaInVehicle_C"));

                if (SprintAbility)
                    GrantGameplayAbility(Pawn, SprintAbility); //, true);
                if (ReloadAbility)
                    GrantGameplayAbility(Pawn, ReloadAbility);
                if (JumpAbility)
                    GrantGameplayAbility(Pawn, JumpAbility);
                if (InteractUseAbility)
                    GrantGameplayAbility(Pawn, InteractUseAbility);
                if (InteractSearchAbility)
                    GrantGameplayAbility(Pawn, InteractSearchAbility);
                if (EnterVehicleAbility)
                    GrantGameplayAbility(Pawn, EnterVehicleAbility);
                if (ExitVehicleAbility)
                    GrantGameplayAbility(Pawn, ExitVehicleAbility);
                if (InVehicleAbility)
                    GrantGameplayAbility(Pawn, InVehicleAbility);
            */

            }

            if (Engine_Version < 424) // i dont think needed
            {
                static auto EmoteAbility = FindObject(("BlueprintGeneratedClass /Game/Abilities/Emotes/GAB_Emote_Generic.GAB_Emote_Generic_C"));

                if (EmoteAbility)
                {
                    GrantGameplayAbility(Pawn, EmoteAbility);
                }
            }

            std::cout << ("Granted Abilities!\n");
        }
        else
            std::cout << ("Unable to find AbilitySystemComponent!\n");
    }
    else
        std::cout << ("Unable to grant abilities due to no GiveAbility!\n");

    // if (FnVerDouble >= 4.0) // if (std::floor(FnVerDouble) != 9 && std::floor(FnVerDouble) != 10 && Engine_Version >= 420) // if (FnVerDouble < 15) // if (Engine_Version < 424) // if (FnVerDouble < 9) // (std::floor(FnVerDouble) != 9)
    // if (FnVerDouble < 16.00)

    // itementrysize 0xb0 on 2.4.2

    if (Engine_Version >= 420)
    {
        static auto PickaxeDef = FindObject(("FortWeaponMeleeItemDefinition /Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01"));
        static auto Minis = FindObject(("FortWeaponRangedItemDefinition /Game/Athena/Items/Consumables/ShieldSmall/Athena_ShieldSmall.Athena_ShieldSmall"));
        static auto SlurpJuice = FindObject(("FortWeaponRangedItemDefinition /Game/Athena/Items/Consumables/PurpleStuff/Athena_PurpleStuff.Athena_PurpleStuff"));
        static auto GoldAR = FindObject(("FortWeaponRangedItemDefinition /Game/Athena/Items/Weapons/WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03"));

        Inventory::CreateAndAddItem(PlayerController, PickaxeDef, EFortQuickBars::Primary, 0, 1);
        Inventory::CreateAndAddItem(PlayerController, GoldAR, EFortQuickBars::Primary, 1, 1);

        if (FnVerDouble < 9.10)
        {
            static auto GoldPump = FindObject(("FortWeaponRangedItemDefinition /Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_SR_Ore_T03.WID_Shotgun_Standard_Athena_SR_Ore_T03"));
            static auto HeavySniper = FindObject(("FortWeaponRangedItemDefinition /Game/Athena/Items/Weapons/WID_Sniper_Heavy_Athena_SR_Ore_T03.WID_Sniper_Heavy_Athena_SR_Ore_T03"));

            if (!GoldPump)
                GoldPump = FindObject("FortWeaponRangedItemDefinition /Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_UC_Ore_T03.WID_Shotgun_Standard_Athena_UC_Ore_T03"); // blue pump

            Inventory::CreateAndAddItem(PlayerController, GoldPump, EFortQuickBars::Primary, 2, 1);
            Inventory::CreateAndAddItem(PlayerController, HeavySniper, EFortQuickBars::Primary, 3, 1);
        }
        else
        {
            static auto BurstSMG = FindObject(("FortWeaponRangedItemDefinition /Game/Athena/Items/Weapons/WID_Pistol_BurstFireSMG_Athena_R_Ore_T03.WID_Pistol_BurstFireSMG_Athena_R_Ore_T03"));
            static auto CombatShotgun = FindObject(("FortWeaponRangedItemDefinition /Game/Athena/Items/Weapons/WID_Shotgun_Combat_Athena_SR_Ore_T03.WID_Shotgun_Combat_Athena_SR_Ore_T03"));

            Inventory::CreateAndAddItem(PlayerController, CombatShotgun, EFortQuickBars::Primary, 2, 1);
            Inventory::CreateAndAddItem(PlayerController, BurstSMG, EFortQuickBars::Primary, 3, 1);
        }

        Inventory::CreateAndAddItem(PlayerController, Minis, EFortQuickBars::Primary, 4, 1);
        Inventory::CreateAndAddItem(PlayerController, SlurpJuice, EFortQuickBars::Primary, 5, 1);

        Inventory::GiveAllAmmo(PlayerController);
        Inventory::GiveMats(PlayerController);
        Inventory::GiveStartingItems(PlayerController); // Gives the needed items like edit tool and builds
    }

    if (false && FnVerDouble >= 7.40) // idk why this dopesnt work
    {
        struct FUniqueNetIdRepl // : public FUniqueNetIdWrapper
        {
            unsigned char                                      UnknownData00[0x1];                                       // 0x0000(0x0001) MISSED OFFSET

            unsigned char                                      UnknownData01[0x17];                                      // 0x0001(0x0017) MISSED OFFSET
            TArray<unsigned char>                              ReplicationBytes;                                         // 0x0018(0x0010) (ZeroConstructor, Transient)
        };

        static auto GameState = Helper::GetGameState();
        auto GameMemberInfoArray = GameState->Member<__int64>("GameMemberInfoArray");

        static int TeamIDX = 4;

        auto teamIndexThing = PlayerState->Member<uint8_t>("TeamIndex");

        auto oldTeamIDX = *teamIndexThing;

        *teamIndexThing = TeamIDX;
        *PlayerState->Member<uint8_t>("SquadId") = TeamIDX;

        static auto SquadIdInfoOffset = FindOffsetStruct("ScriptStruct /Script/FortniteGame.GameMemberInfo", "SquadId");
        static auto TeamIndexOffset = FindOffsetStruct("ScriptStruct /Script/FortniteGame.GameMemberInfo", "TeamIndex");
        static auto MemberUniqueIdOffset = FindOffsetStruct("ScriptStruct /Script/FortniteGame.GameMemberInfo", "MemberUniqueId");

        static auto GameMemberInfoStruct = FindObject("ScriptStruct /Script/FortniteGame.GameMemberInfo");
        static auto SizeOfGameMemberInfo = GetSizeOfStruct(GameMemberInfoStruct);
        auto NewInfo = (__int64*)malloc(SizeOfGameMemberInfo);

        static auto UniqueIdSize = GetSizeOfStruct(FindObject("ScriptStruct /Script/Engine.UniqueNetIdRepl"));

        std::cout << "acutal size: " << UniqueIdSize << '\n';
        std::cout << "ours: " << sizeof(FUniqueNetIdRepl) << '\n';

        RtlSecureZeroMemory(NewInfo, SizeOfGameMemberInfo);

        *(uint8_t*)(__int64(NewInfo) + SquadIdInfoOffset) = TeamIDX;
        *(uint8_t*)(__int64(NewInfo) + TeamIndexOffset) = TeamIDX;
        *(FUniqueNetIdRepl*)(__int64(NewInfo) + MemberUniqueIdOffset) = *PlayerState->Member<FUniqueNetIdRepl>("UniqueId");

        static auto MembersOffset = FindOffsetStruct("ScriptStruct /Script/FortniteGame.GameMemberInfoArray", "Members");

        auto Members = (TArray<__int64>*)(__int64(GameMemberInfoArray) + MembersOffset);

        MarkArrayDirty(GameMemberInfoArray);
        Members->Add(*NewInfo, SizeOfGameMemberInfo);
        MarkArrayDirty(GameMemberInfoArray);

        // MarkItemDirty(GameMemberInfoArray, (FFastArraySerializerItem*)&NewInfo);

        auto PlayerTeam = PlayerState->Member<UObject*>("PlayerTeam");

        // *PlayerTeam = GameState->Member<TArray<UObject*>>("Teams")->At(TeamIDX);
        // *PlayerState->Member<UObject*>("PlayerTeamPrivate") = *(*PlayerTeam)->Member<UObject*>("PrivateInfo");
        (*PlayerTeam)->Member<TArray<UObject*>>("TeamMembers")->Add(PlayerController);

        static auto OnRep_SquadId = PlayerState->Function("OnRep_SquadId");
        static auto OnRep_PlayerTeam = PlayerState->Function("OnRep_PlayerTeam");
        static auto OnRep_TeamIndex = PlayerState->Function("OnRep_TeamIndex");

        PlayerState->ProcessEvent(OnRep_SquadId);
        PlayerState->ProcessEvent(OnRep_PlayerTeam);
        PlayerState->ProcessEvent(OnRep_TeamIndex, &oldTeamIDX);
    }
    else if (FnVerDouble <= 7.30)
    {
        static int Team = 4;

        auto Teams = Helper::GetGameState()->Member<TArray<UObject*>>("Teams");

        // if (GameState->Teams[Team]->TeamMembers.Num() >= MaxPlayersPerTeam)
           // Team++;

        auto PlayerTeam = PlayerState->Member<UObject*>("PlayerTeam");

        auto TeamMembers = Teams->At(Team)->Member<TArray<UObject*>>("TeamMembers");

        std::cout << "After Team: " << Team << '\n';
        std::cout << "Team A: " << Teams->At(Team) << '\n';

        *PlayerState->Member<uint8_t>("TeamIndex") = Team; // GetMath()->STATIC_RandomIntegerInRange(2, GameState->CurrentPlaylistData->MaxTeamCount));
        *PlayerTeam = Teams->At(Team);
        *(*PlayerState->Member<UObject*>("PlayerTeamPrivate"))->Member<UObject*>("TeamInfo") = *PlayerTeam;

        TeamMembers->Add(PlayerController);

        std::cout << "New Team Size: " << TeamMembers->Num() << "\n";
        *PlayerState->Member<uint8_t>("SquadId") = TeamMembers->Num();

        static auto OnRep_SquadId = PlayerState->Function("OnRep_SquadId");

        PlayerState->ProcessEvent(OnRep_SquadId);
        // OnRep_TeamIndex
    }

    Inventory::Update(PlayerController);

    std::cout << ("Spawned Player!\n");

    return PlayerController;
}

static bool __fastcall HasClientLoadedCurrentWorldDetour(UObject* pc)
{
    std::cout << ("yes\n");
    return true;
}

void* NetDebugDetour(UObject* idk)
{
    // std::cout << "NetDebug!\n";
    return nullptr;
}

char __fastcall malformedDetour(__int64 a1, __int64 a2)
{
    /* 9.41:
    
    v20 = *(_QWORD *)(a1 + 48);
    *(double *)(a1 + 0x430) = v19;
    if ( v20 )
      (*(void (__fastcall **)(__int64))(*(_QWORD *)v20 + 0xC90i64))(v20);// crashes

    */

    auto old = *(__int64*)(a1 + 48);
    std::cout << "Old: " << old << '\n';
    *(__int64*)(a1 + 48) = 0;
    std::cout << "Old2: " << old << '\n';
    auto ret = malformed(a1, a2);
    *(__int64*)(a1 + 48) = old;
    return ret;
}

FString* GetRequestURL(UObject* Connection)
{
    if (FnVerDouble >= 7 && Engine_Version < 424)
        return (FString*)(__int64(Connection) + 424);
    else if (FnVerDouble < 7)
        return (FString*)((__int64*)Connection + 54);
    else if (Engine_Version >= 424)
        return (FString*)(__int64(Connection) + 440);

    return nullptr;
}

void World_NotifyControlMessageDetour(UObject* World, UObject* Connection, uint8_t MessageType, __int64* Bunch)
{
    std::cout << ("Receieved control message: ") << std::to_string((int)MessageType) << '\n';

    switch (MessageType)
    {
    case 0:
    {
        //S5-6 Fix
        if (Engine_Version == 421) // not the best way to fix it...
        {
            {
                /* uint8_t */ char IsLittleEndian = 0;
                unsigned int RemoteNetworkVersion = 0;
                auto v38 = (__int64*)Bunch[1];
                FString EncryptionToken; // This should be a short* but ye

                if ((unsigned __int64)(*v38 + 1) > v38[1])
                    (*(void(__fastcall**)(__int64*, char*, __int64))(*Bunch + 72))(Bunch, &IsLittleEndian, 1);
                else
                    IsLittleEndian = *(char*)(*v38)++;
                auto v39 = Bunch[1];
                if ((unsigned __int64)(*(__int64*)v39 + 4) > *(__int64*)(v39 + 8))
                {
                    (*(void(__fastcall**)(__int64*, unsigned int*, __int64))(*Bunch + 72))(Bunch, &RemoteNetworkVersion, 4);
                    if ((*((char*)Bunch + 41) & 0x10) != 0)
                        Idkf(__int64(Bunch), __int64(&RemoteNetworkVersion), 4);
                }
                else
                {
                    RemoteNetworkVersion = **(int32_t**)v39;
                    *(__int64*)v39 += 4;
                }

                ReceiveFString(Bunch, EncryptionToken);
                // if (*((char*)Bunch + 40) < 0)
                    // do stuff

                std::cout << ("EncryptionToken: ") << __int64(EncryptionToken.Data.GetData()) << '\n';

                if (EncryptionToken.Data.GetData())
                    std::cout << ("EncryptionToken Str: ") << EncryptionToken.ToString() << '\n';

                std::cout << ("Sending challenge!\n");
                SendChallenge(World, Connection);
                std::cout << ("Sent challenge!\n");
                // World_NotifyControlMessage(World, Connection, MessageType, (void*)Bunch);
                // std::cout << "IDK: " << *((char*)Bunch + 40) << '\n';
                return;
            }
        }
        break;
    }
    case 4: // NMT_Netspeed // Do we even have to rei,plment this?
        // if (Engine_Version >= 423)
          //  *Connection->Member<int>(("CurrentNetSpeed")) = 60000; // sometimes 60000
        // else
        *Connection->Member<int>(("CurrentNetSpeed")) = 30000; // sometimes 60000
        return;
    case 5: // NMT_Login
    {
        if (Engine_Version >= 420)
        {
            Bunch[7] += (16 * 1024 * 1024);

            FString OnlinePlatformName;

            static double CurrentFortniteVersion = FnVerDouble;

            if (CurrentFortniteVersion >= 7 && Engine_Version < 424)
                ReceiveFString(Bunch, *(FString*)(__int64(Connection) + 400)); // clientresponse
            else if (CurrentFortniteVersion < 7)
                ReceiveFString(Bunch, *(FString*)((__int64*)Connection + 51));
            else if (Engine_Version >= 424)
                ReceiveFString(Bunch, *(FString*)(__int64(Connection) + 416));

            auto RequestURL = GetRequestURL(Connection);

            if (!RequestURL)
                return;

            ReceiveFString(Bunch, *RequestURL);

            ReceiveUniqueIdRepl(Bunch, Connection->Member<__int64>(("PlayerID")));
            std::cout << ("Got PlayerID!\n");
            // std::cout << "PlayerID => " + *Connection->Member<__int64>(("PlayerID")) << '\n';
            // std::cout << "PlayerID => " + std::to_string(*UniqueId) << '\n';
            ReceiveFString(Bunch, OnlinePlatformName);

            std::cout << ("Got OnlinePlatformName!\n");

            if (OnlinePlatformName.Data.GetData())
                std::cout << ("OnlinePlatformName => ") << OnlinePlatformName.ToString() << '\n';

            Bunch[7] -= (16 * 1024 * 1024);

            WelcomePlayer(Helper::GetWorld(), Connection);

            /* auto funnyURL = RequestURL->ToString();

            std::string teamCodeStr = "NONE";

            if (funnyURL.find("?teamCode=") != std::string::npos)
            {
                // open 127.0.0.1?teamCode=100

                funnyURL = funnyURL.substr(funnyURL.find("?teamCode") + 1);
                funnyURL = funnyURL.substr(0, funnyURL.find('?'));

                auto teamCodeParam = funnyURL;

                teamCodeStr = teamCodeParam.substr(teamCodeParam.find('=') + 1);
            }
            else if (false) // here we would use like regex match or something
            {
                // Milxnor[100]

                funnyURL = funnyURL.substr(funnyURL.find("?Name") + 1);
                funnyURL = funnyURL.substr(0, funnyURL.find('?'));
                auto Name = funnyURL.substr(funnyURL.find('=') + 1);

                const std::regex base_regex((R"(\[[0-9]+\])"));
                std::cmatch base_match;

                std::regex_search(Name.c_str(), base_match, base_regex);

                teamCodeStr = base_match.str();
                teamCodeStr.erase(std::remove(teamCodeStr.begin(), teamCodeStr.end(), '['), teamCodeStr.end());
                teamCodeStr.erase(std::remove(teamCodeStr.begin(), teamCodeStr.end(), ']'), teamCodeStr.end());
            } */

            return;
        }
        break;
    }
    case 6:
        return;
    case 9: // NMT_Join
    {
        if (Engine_Version == 421 || Engine_Version < 420)
        {
            auto ConnectionPC = Connection->Member<UObject*>(("PlayerController"));
            if (!*ConnectionPC)
            {
                FURL InURL;
                /* std::cout << "Calling1!\n";
                std::wcout << L"RequestURL Data: " << Connection->RequestURL.Data << '\n';
                InURL = o_FURL_Construct(InURL, NULL, Connection->RequestURL.Data, ETravelType::TRAVEL_Absolute);
                std::cout << "Called2!\n";
                std::cout << "URL2 Map: " << InURL.Map.ToString() << '\n'; */

                // FURL InURL(NULL, Connection->RequestURL, ETravelType::TRAVEL_Absolute);

                InURL.Map.Set(L"/Game/Maps/Frontend");
                InURL.Valid = 1;
                InURL.Protocol.Set(L"unreal");
                InURL.Port = 7777;

                // ^ This was all taken from 3.5

                FString ErrorMsg;
                SpawnPlayActorDetour(World, Connection, ENetRole::ROLE_AutonomousProxy, InURL, Connection->Member<void>(("PlayerID")), ErrorMsg, 0);

                if (!*ConnectionPC)
                {
                    std::cout << ("Failed to spawn PlayerController! Error Msg: ") << ErrorMsg.ToString() << "\n";
                    // TODO: Send NMT_Failure and FlushNet
                    return;
                }
                else
                {
                    std::cout << ("Join succeeded!\n");
                    FString LevelName;
                    const bool bSeamless = true; // If this is true and crashes the client then u didn't remake NMT_Hello or NMT_Join correctly.

                    static auto ClientTravelFn = (*ConnectionPC)->Function(("ClientTravel"));
                    struct {
                        FString URL;
                        ETravelType TravelType;
                        bool bSeamless;
                        FGuid MapPackageGuid;
                    } paramsTravel{ LevelName, ETravelType::TRAVEL_Relative, bSeamless, FGuid() };

                    // ClientTravel(*ConnectionPC, LevelName, ETravelType::TRAVEL_Relative, bSeamless, FGuid());

                    (*ConnectionPC)->ProcessEvent(("ClientTravel"), &paramsTravel);

                    std::cout << ("Traveled client.\n");

                    *(int32_t*)(__int64(&*Connection->Member<int>(("LastReceiveTime"))) + (sizeof(double) * 4) + (sizeof(int32_t) * 2)) = 0; // QueuedBits
                }
                return;
            }
        }
        break;
    }
    }

    return World_NotifyControlMessage(Helper::GetWorld(), Connection, MessageType, Bunch);
}

char Beacon_NotifyControlMessageDetour(UObject* Beacon, UObject* Connection, uint8_t MessageType, __int64* Bunch)
{
    std::cout << "beacon ncm!\n";
    World_NotifyControlMessageDetour(Helper::GetWorld(), Connection, MessageType, Bunch);
    return true;
}

char __fastcall NoReserveDetour(__int64* a1, __int64 a2, char a3, __int64* a4)
{
    std::cout << ("No Reserve!\n");
    return 0;
}

__int64 CollectGarbageDetour(__int64) { return 0; }

bool TryCollectGarbageHook()
{
    return 0;
}

int __fastcall ReceivedPacketDetour(__int64* a1, LARGE_INTEGER a2, char a3)
{
}

void InitializeNetHooks()
{
    MH_CreateHook((PVOID)SpawnPlayActorAddr, SpawnPlayActorDetour, (void**)&SpawnPlayActor);
    MH_EnableHook((PVOID)SpawnPlayActorAddr);

    MH_CreateHook((PVOID)Beacon_NotifyControlMessageAddr, Beacon_NotifyControlMessageDetour, (void**)&Beacon_NotifyControlMessage);
    MH_EnableHook((PVOID)Beacon_NotifyControlMessageAddr);

    MH_CreateHook((PVOID)World_NotifyControlMessageAddr, World_NotifyControlMessageDetour, (void**)&World_NotifyControlMessage);
    MH_EnableHook((PVOID)World_NotifyControlMessageAddr);

    if (Engine_Version < 424 && GetNetModeAddr) // i dont even think we have to hook this
    {
        MH_CreateHook((PVOID)GetNetModeAddr, GetNetModeDetour, (void**)&GetNetMode);
        MH_EnableHook((PVOID)GetNetModeAddr);
    }
    
    if (Engine_Version > 423)
    {
        MH_CreateHook((PVOID)ValidationFailureAddr, ValidationFailureDetour, (void**)&ValidationFailure);
        MH_EnableHook((PVOID)ValidationFailureAddr);
    }

    MH_CreateHook((PVOID)TickFlushAddr, TickFlushDetour, (void**)&TickFlush);
    MH_EnableHook((PVOID)TickFlushAddr);

    if (KickPlayer)
    {
        std::cout << "Hooked kickplayer!\n";
        MH_CreateHook((PVOID)KickPlayerAddr, KickPlayerDetour, (void**)&KickPlayer);
        MH_EnableHook((PVOID)KickPlayerAddr);
    }

    if (LP_SpawnPlayActorAddr)
    {
        MH_CreateHook((PVOID)LP_SpawnPlayActorAddr, LP_SpawnPlayActorDetour, (void**)&LP_SpawnPlayActor);
        MH_EnableHook((PVOID)LP_SpawnPlayActorAddr);
    }

    if (NoReserveAddr)
    {
        MH_CreateHook((PVOID)NoReserveAddr, NoReserveDetour,  (void**)&NoReserve);
        MH_EnableHook((PVOID)NoReserveAddr);
    }

    // if (NetDebug)
    {
        if (NetDebugAddr && Engine_Version >= 419 && FnVerDouble < 17.00)
        {
            MH_CreateHook((PVOID)NetDebugAddr, NetDebugDetour, (void**)&NetDebug);
            MH_EnableHook((PVOID)NetDebugAddr);
        }

        if (CollectGarbageAddr)
        {
            // if (Engine_Version < 424)
            {
                if (Engine_Version >= 422 && Engine_Version < 424)
                {
                    MH_CreateHook((PVOID)CollectGarbageAddr, TryCollectGarbageHook, nullptr);
                    MH_EnableHook((PVOID)CollectGarbageAddr);
                }
                else
                {
                    MH_CreateHook((PVOID)CollectGarbageAddr, CollectGarbageDetour, (void**)&CollectGarbage);
                    MH_EnableHook((PVOID)CollectGarbageAddr);
                }
            }
        }
        else
            std::cout << ("[WARNING] Unable to hook CollectGarbage!\n");
    }
}

void DisableNetHooks()
{
    MH_DisableHook((PVOID)SpawnPlayActorAddr);
    MH_DisableHook((PVOID)Beacon_NotifyControlMessageAddr);
    MH_DisableHook((PVOID)World_NotifyControlMessageAddr);

    if (Engine_Version < 424 && GetNetModeAddr) // i dont even think we have to hook this
    {
        MH_DisableHook((PVOID)GetNetModeAddr);
    }

    if (Engine_Version > 423)
    {
        MH_DisableHook((PVOID)ValidationFailureAddr);
    }

    MH_DisableHook((PVOID)TickFlushAddr);

    if (KickPlayer)
    {
        MH_DisableHook((PVOID)KickPlayerAddr);
    }

    if (LP_SpawnPlayActorAddr)
    {
        MH_DisableHook((PVOID)LP_SpawnPlayActorAddr);
    }

    if (NoReserveAddr)
    {
        MH_DisableHook((PVOID)NoReserveAddr);
    }

    // if (NetDebug)
    {
        if (NetDebugAddr)
        {
            MH_DisableHook((PVOID)NetDebugAddr);
        }

        if (CollectGarbageAddr)
        {
            // if (Engine_Version < 424)
            {
                if (Engine_Version >= 422 && Engine_Version < 424)
                {
                    MH_DisableHook((PVOID)CollectGarbageAddr);
                }
                else
                {
                    MH_DisableHook((PVOID)CollectGarbageAddr);
                }
            }
        }
        else
            std::cout << ("[WARNING] Unable to hook CollectGarbage!\n");
    }
}
