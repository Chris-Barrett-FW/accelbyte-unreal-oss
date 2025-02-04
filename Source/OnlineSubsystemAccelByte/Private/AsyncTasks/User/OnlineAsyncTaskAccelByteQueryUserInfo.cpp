// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineAsyncTaskAccelByteQueryUserInfo.h"
#include "OnlineSubsystemAccelByte.h"
#include "OnlineSubsystemAccelByteDefines.h"
#include "Core/AccelByteError.h"

FOnlineAsyncTaskAccelByteQueryUserInfo::FOnlineAsyncTaskAccelByteQueryUserInfo
	( FOnlineSubsystemAccelByte* const InABSubsystem
	, int32 InLocalUserNum
	, const TArray<TSharedRef<const FUniqueNetId>>& InUserIds
	, const FOnQueryUserInfoComplete& InDelegate )
	: FOnlineAsyncTaskAccelByte(InABSubsystem)
	, InitialUserIds(InUserIds)
	, Delegate(InDelegate)
{
	LocalUserNum = InLocalUserNum;
}

void FOnlineAsyncTaskAccelByteQueryUserInfo::Initialize()
{
	Super::Initialize();

	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT("Initiating User Index: %d; Initial UserId Amount: %d"), LocalUserNum, InitialUserIds.Num());

	// First, remove any invalid IDs from the initial user ID array
	InitialUserIds.RemoveAll([](const TSharedRef<const FUniqueNetId>& FoundUserId) { return !FoundUserId->IsValid(); });

	// Then, check if we have an empty array for user IDs, and bail out if we do
	if (InitialUserIds.Num() <= 0)
	{
		AB_OSS_ASYNC_TASK_TRACE_END_VERBOSITY(Warning, TEXT("No user IDs passed to query user info! Unable to complete!"));
		CompleteTask(EAccelByteAsyncTaskCompleteState::InvalidState);
		return;
	}

	// Set up arrays and counters to be the same length as the amount of user IDs we want to query. These expected counters
	// will be used for synchronization later, the task knows that it is finished when these expected values match their
	// corresponding maps.
	UserIdsToQuery.Empty(InitialUserIds.Num());

	// Iterate through each user ID that we have been requested to query for, convert them to string IDs, and then fire
	// off a request to get account data (to get display name) as well as public profile attributes for each user
	for (const TSharedRef<const FUniqueNetId>& NetId : InitialUserIds)
	{
		if (NetId->GetType() != ACCELBYTE_SUBSYSTEM)
		{
			UE_LOG_AB(Warning, TEXT("NetId passed to FOnlineUserAccelByte::QueryUserInfo (%s) was an invalid type (%s). Query the external mapping first to convert to an AccelByte ID. Skipping this ID!"), *(Subsystem->GetInstanceName().ToString()), *(NetId->GetType().ToString()));
			continue;
		}

		const TSharedRef<const FUniqueNetIdAccelByteUser> AccelByteId = StaticCastSharedRef<const FUniqueNetIdAccelByteUser>(NetId);
		UserIdsToQuery.Add(AccelByteId->GetAccelByteId());
	}

	// Try and query all of the users and cache them in the user store
	const FOnlineUserCacheAccelBytePtr UserCache = Subsystem->GetUserCache();
	if (!UserCache.IsValid())
	{
		AB_OSS_ASYNC_TASK_TRACE_END_VERBOSITY(Warning, TEXT("Could not query information on users as our user store instance is invalid!"));
		CompleteTask(EAccelByteAsyncTaskCompleteState::InvalidState);
		return;
	}

	const FOnQueryUsersComplete OnQueryUsersCompleteDelegate = FOnQueryUsersComplete::CreateRaw(this, &FOnlineAsyncTaskAccelByteQueryUserInfo::OnQueryUsersComplete);
	UserCache->QueryUsersByAccelByteIds(LocalUserNum, UserIdsToQuery, OnQueryUsersCompleteDelegate);

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryUserInfo::Finalize()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));

	// Iterate through each account that we have received from the backend and add to our user interface
	for (const TSharedRef<FAccelByteUserInfo>& QueriedUser : UsersQueried)
	{
		// Construct the user information instance and add the ID of the user to our QueriedUserIds array that will be passed to the completion delegate
		TSharedRef<FUserOnlineAccountAccelByte> User = MakeShared<FUserOnlineAccountAccelByte>(QueriedUser->Id.ToSharedRef(), QueriedUser->DisplayName);
		TSharedRef<const FUniqueNetId> FoundUserId = User->GetUserId();
		QueriedUserIds.Add(FoundUserId);

		// Add the new user information instance to the user interface's cache so the developer can retrieve it in the delegate handler
		TSharedPtr<FOnlineUserAccelByte, ESPMode::ThreadSafe> UserInterface = StaticCastSharedPtr<FOnlineUserAccelByte>(Subsystem->GetUserInterface());
		if (UserInterface != nullptr)
		{
			UserInterface->AddUserInfo(FoundUserId, User);
		}
	}

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryUserInfo::TriggerDelegates()
{
	AB_OSS_ASYNC_TASK_TRACE_BEGIN(TEXT(""));

	Delegate.Broadcast(LocalUserNum, bWasSuccessful, QueriedUserIds, ErrorStr);

	AB_OSS_ASYNC_TASK_TRACE_END(TEXT(""));
}

void FOnlineAsyncTaskAccelByteQueryUserInfo::OnQueryUsersComplete(bool bIsSuccessful, TArray<TSharedRef<FAccelByteUserInfo>> InUsersQueried)
{
	if (bIsSuccessful)
	{
		UsersQueried = InUsersQueried;
		CompleteTask(EAccelByteAsyncTaskCompleteState::Success);
	}
	else
	{
		ErrorStr = TEXT("query-users-error-response");
		UE_LOG_AB(Warning, TEXT("Failed to query users from the backend!"));
		CompleteTask(EAccelByteAsyncTaskCompleteState::RequestFailed);
	}
}
