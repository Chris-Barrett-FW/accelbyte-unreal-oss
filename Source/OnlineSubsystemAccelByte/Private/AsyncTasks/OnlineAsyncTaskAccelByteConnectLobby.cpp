// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineAsyncTaskAccelByteConnectLobby.h"
#include "OnlineSubsystemAccelByte.h"
#include "OnlineIdentityInterfaceAccelByte.h"
#include "OnlineFriendsInterfaceAccelByte.h"
#include "OnlinePartyInterfaceAccelByte.h"
#include "OnlineSessionInterfaceAccelByte.h"

FOnlineAsyncTaskAccelByteConnectLobby::FOnlineAsyncTaskAccelByteConnectLobby(FOnlineSubsystemAccelByte* const InABInterface, const FUniqueNetId& InLocalUserId)
	: FOnlineAsyncTaskAccelByte(InABInterface)
{
	UserId = StaticCastSharedRef<const FUniqueNetIdAccelByteUser>(InLocalUserId.AsShared());
	ErrorStr = TEXT("");
}

void FOnlineAsyncTaskAccelByteConnectLobby::Initialize()
{
	Super::Initialize();

	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("UserId: %s"), *UserId->ToDebugString());

	// Create delegates for successfully as well as unsuccessfully connecting to the AccelByte lobby websocket
	OnLobbyConnectSuccessDelegate = TDelegateUtils<AccelByte::Api::Lobby::FConnectSuccess>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteConnectLobby::OnLobbyConnectSuccess);
	OnLobbyConnectErrorDelegate = TDelegateUtils<FErrorHandler>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteConnectLobby::OnLobbyConnectError);

	OnLobbyDisconnectedNotifDelegate = TDelegateUtils<AccelByte::Api::Lobby::FDisconnectNotif>::CreateThreadSafeSelfPtr(this, &FOnlineAsyncTaskAccelByteConnectLobby::OnLobbyDisconnectedNotif);
	ApiClient->Lobby.SetDisconnectNotifDelegate(OnLobbyDisconnectedNotifDelegate);

	AccelByte::Api::Lobby::FConnectionClosed OnLobbyConnectionClosedDelegate = AccelByte::Api::Lobby::FConnectionClosed::CreateStatic(FOnlineAsyncTaskAccelByteConnectLobby::OnLobbyConnectionClosed, LocalUserNum);
	ApiClient->Lobby.SetConnectionClosedDelegate(OnLobbyConnectionClosedDelegate);

	// Send off a request to connect to the lobby websocket, as well as connect our delegates for doing so
	ApiClient->Lobby.SetConnectSuccessDelegate(OnLobbyConnectSuccessDelegate);
	ApiClient->Lobby.SetConnectFailedDelegate(OnLobbyConnectErrorDelegate);
	ApiClient->Lobby.Connect();

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteConnectLobby::Finalize()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("bWasSuccessful: %s"), LOG_BOOL_FORMAT(bWasSuccessful));

	if (bWasSuccessful)
	{
		const FOnlineIdentityAccelBytePtr IdentityInt = FOnlineIdentityAccelByte::Get();
		if (IdentityInt.IsValid())
		{
			// #NOTE (Wiwing): Overwrite connect Lobby success delegate for reconnection
			Api::Lobby::FConnectSuccess OnLobbyReconnectionDelegate = Api::Lobby::FConnectSuccess::CreateStatic(FOnlineAsyncTaskAccelByteConnectLobby::OnLobbyReconnected, LocalUserNum);
			ApiClient->Lobby.SetConnectSuccessDelegate(OnLobbyReconnectionDelegate);
		}
	}

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteConnectLobby::TriggerDelegates()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("bWasSuccessful: %s"), LOG_BOOL_FORMAT(bWasSuccessful));

	const FOnlineIdentityAccelBytePtr IdentityInt = FOnlineIdentityAccelByte::Get();
	if (IdentityInt.IsValid())
	{
		IdentityInt->TriggerOnConnectLobbyCompleteDelegates(LocalUserNum, bWasSuccessful, *UserId.Get(), ErrorStr);
	}

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteConnectLobby::OnLobbyConnectSuccess()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));

	// Update identity interface lobby flag
	const FOnlineIdentityAccelBytePtr IdentityInterface = StaticCastSharedPtr<FOnlineIdentityAccelByte>(Subsystem->GetIdentityInterface());
	if (IdentityInterface.IsValid())
	{
		const TSharedPtr<const FUniqueNetId> UserIdPtr = IdentityInterface->GetUniquePlayerId(LocalUserNum);
		TSharedPtr<FUserOnlineAccount> UserAccount = IdentityInterface->GetUserAccount(UserId.ToSharedRef().Get());

		if (UserAccount.IsValid())
		{
			const TSharedPtr<FUserOnlineAccountAccelByte> UserAccountAccelByte = StaticCastSharedPtr<FUserOnlineAccountAccelByte>(UserAccount);
			UserAccountAccelByte->SetConnectedToLobby(true);
		}
	}

	// Register all delegates for the friends interface to get real time notifications for friend actions
	const TSharedPtr<FOnlineFriendsAccelByte, ESPMode::ThreadSafe> FriendsInterface = StaticCastSharedPtr<FOnlineFriendsAccelByte>(Subsystem->GetFriendsInterface());
	if (FriendsInterface.IsValid())
	{
		FriendsInterface->RegisterRealTimeLobbyDelegates(LocalUserNum);
	}

	// Also register all delegates for the party interface to get notifications for party actions
	const TSharedPtr<FOnlinePartySystemAccelByte, ESPMode::ThreadSafe> PartyInterface = StaticCastSharedPtr<FOnlinePartySystemAccelByte>(Subsystem->GetPartyInterface());
	if (PartyInterface.IsValid())
	{
		PartyInterface->RegisterRealTimeLobbyDelegates(UserId.ToSharedRef());
	}

	// Also register all delegates for the party interface to get notifications for party actions
	const TSharedPtr<FOnlineSessionAccelByte, ESPMode::ThreadSafe> SessionInterface = StaticCastSharedPtr<FOnlineSessionAccelByte>(Subsystem->GetSessionInterface());
	if (SessionInterface.IsValid())
	{
		SessionInterface->RegisterRealTimeLobbyDelegates(LocalUserNum);
	}

	CompleteTask(EAccelByteAsyncTaskCompleteState::Success);
	AB_OSS_ASYNC_TASK_TRACE_END(TEXT("Sending request to get or create a default user profile for user '%s'!"), *UserId->ToDebugString());
}

void FOnlineAsyncTaskAccelByteConnectLobby::OnLobbyConnectError(int32 ErrorCode, const FString& ErrorMessage)
{
	ErrorStr = TEXT("login-failed-lobby-connect-error");
	UE_LOG_AB(Warning, TEXT("Failed to connect to the AccelByte lobby websocket! Error Code: %d; Error Message: %s"), ErrorCode, *ErrorMessage);
	CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
}

void FOnlineAsyncTaskAccelByteConnectLobby::OnLobbyDisconnectedNotif(const FAccelByteModelsDisconnectNotif& Result)
{
	// Update identity interface lobby flag
	const TSharedPtr<FOnlineIdentityAccelByte, ESPMode::ThreadSafe> IdentityInterface = StaticCastSharedPtr<FOnlineIdentityAccelByte>(Subsystem->GetIdentityInterface());
	if (IdentityInterface.IsValid())
	{
		const TSharedPtr<const FUniqueNetId> UserIdPtr = IdentityInterface->GetUniquePlayerId(LocalUserNum);
		TSharedPtr<FUserOnlineAccount> UserAccount = IdentityInterface->GetUserAccount(UserId.ToSharedRef().Get());

		if (UserAccount.IsValid())
		{
			const TSharedPtr<FUserOnlineAccountAccelByte> UserAccountAccelByte = StaticCastSharedPtr<FUserOnlineAccountAccelByte>(UserAccount);
			UserAccountAccelByte->SetConnectedToLobby(false);
		}
	}

	ErrorStr = TEXT("login-failed-lobby-connect-error");

	UnbindDelegates();

	UE_LOG_AB(Warning, TEXT("Lobby disconnected. Reason '%s'"), *Result.Message);
	CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
}

void FOnlineAsyncTaskAccelByteConnectLobby::OnLobbyConnectionClosed(int32 StatusCode, const FString& Reason, bool WasClean, int32 InLocalUserNum)
{
	UE_LOG_AB(Warning, TEXT("Lobby connection closed. Reason '%s' Code : '%d'"), *Reason, StatusCode);

	const FOnlineIdentityAccelBytePtr IdentityInterface = FOnlineIdentityAccelByte::Get();
	const FOnlinePartySystemAccelBytePtr PartyInterface = FOnlinePartySystemAccelByte::Get();

	if (!IdentityInterface.IsValid() || !PartyInterface.IsValid())
	{
		UE_LOG_AB(Warning, TEXT("Error due to either IdentityInterface or PartyInterface is invalid"));
		return;
	}

	const TSharedPtr<const FUniqueNetId> UserIdPtr = IdentityInterface->GetUniquePlayerId(InLocalUserNum);
	TSharedPtr<FUserOnlineAccount> UserAccount;

	if (UserIdPtr.IsValid())
	{
		const FUniqueNetId& UserId = UserIdPtr.ToSharedRef().Get();
		UserAccount = IdentityInterface->GetUserAccount(UserId);
	}

	if (UserAccount.IsValid())
	{
		const TSharedPtr<FUserOnlineAccountAccelByte> UserAccountAccelByte = StaticCastSharedPtr<FUserOnlineAccountAccelByte>(UserAccount);
		UserAccountAccelByte->SetConnectedToLobby(false);
	}

	FString LogoutReason;
	if (StatusCode == 1006) // Internet connection is disconnected
	{
		LogoutReason = TEXT("socket-disconnected");
		AccelByte::FApiClientPtr ApiClient = IdentityInterface->GetApiClient(InLocalUserNum);
		ApiClient->CredentialsRef->ForgetAll();

		TSharedPtr<FUniqueNetIdAccelByteUser const> LocalUserId = StaticCastSharedPtr<FUniqueNetIdAccelByteUser const>(IdentityInterface->GetUniquePlayerId(InLocalUserNum));
		PartyInterface->RemovePartyFromInterface(LocalUserId.ToSharedRef());
	}
	IdentityInterface->Logout(InLocalUserNum, LogoutReason);
}

void FOnlineAsyncTaskAccelByteConnectLobby::OnLobbyReconnected(int32 InLocalUserNum)
{
	UE_LOG_AB(Log, TEXT("Lobby successfully reconnected."));

	const FOnlineIdentityAccelBytePtr IdentityInterface = FOnlineIdentityAccelByte::Get();
	const FOnlinePartySystemAccelBytePtr PartyInterface = FOnlinePartySystemAccelByte::Get();
	if (IdentityInterface.IsValid() && PartyInterface.IsValid())
	{
		TSharedPtr<FUniqueNetIdAccelByteUser const> LocalUserId = StaticCastSharedPtr<FUniqueNetIdAccelByteUser const>(IdentityInterface->GetUniquePlayerId(InLocalUserNum));

		PartyInterface->RemovePartyFromInterface(LocalUserId.ToSharedRef());

		PartyInterface->RestoreParties(LocalUserId.ToSharedRef().Get(), FOnRestorePartiesComplete());
	}
}

void FOnlineAsyncTaskAccelByteConnectLobby::UnbindDelegates()
{
	OnLobbyConnectSuccessDelegate.Unbind();
	OnLobbyConnectErrorDelegate.Unbind();
	OnLobbyDisconnectedNotifDelegate.Unbind();
}
